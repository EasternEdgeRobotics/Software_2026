import {
    Box,
    Grid,
    Paper,
    Switch,
    Table,
    TableBody,
    TableCell,
    TableRow
} from "@mui/material";
import { useAtom } from "jotai";
import {
    CameraURLs,
    ThrusterMultipliers,
    PilotActions,
    KeyboardInputMap,
    ProfilesList,
    CurrentProfile,
    Mappings,
    ControllerInput,
    DiagnosticsData2,
    KeyboardMode
} from "../api/Atoms";
import { useState, useEffect } from "react";
import React from "react";

// Main camera vs alt camera are the exact same without margin and different height

export function MainCamera(props: { ip: string, width: number, enableHud: boolean }) {
    return (
        <Box height={(props.width) * (1473 / 6118)} width={props.width * 0.428} sx={{ backgroundImage: `url(${props.ip}), url(./nosignal.jpg)`, backgroundSize: "100% 100%", borderRadius: "12px", margin: "0 4px 0 4px" }}>
            <Box display={props.enableHud ? "block" : "none"} sx={{ width: "100%", height: "20%", backgroundColor: "rgba(0,0,0,0.5)", backdropFilter: "blur(1px)", float: "right", borderTopRightRadius: "12px", borderTopLeftRadius: "12px" }}>
                <h1 style={{ color: "red", display: "inline" }}>demo text</h1>
                <h1 style={{ color: "lime", display: "inline" }}>&nbsp;stats will be here</h1>
            </Box>
        </Box>
    );
}

export function AltCamera(props: { ip: string, width: number }) {
    return (
        <Box height={props.width * (1620.3 / 6118)} width={props.width * 0.4708} sx={{ backgroundImage: `url(${props.ip}), url(./nosignal.jpg)`, backgroundSize: "100% 100%", borderRadius: "12px" }} />
    );
}

export default function CameraTab() {
    const [width, setWidth] = React.useState<number>(window.innerWidth);
    const [URLs] = useAtom<string[]>(CameraURLs);

    const [, setThrusterMultipliers] =
        useAtom(ThrusterMultipliers);
    const [pilotActions] = useAtom(PilotActions);
    const [keyboardInputMap] = useAtom(KeyboardInputMap);
    const [diagnostics_data_2] = useAtom(DiagnosticsData2);

    const [profilesList] = useAtom(ProfilesList);
    const [currentProfile] = useAtom(CurrentProfile);
    const [mappings] = useAtom(Mappings);
    const [, setController1Detected] = useState(false);
    const [, setController2Detected] = useState(false);
    const [, setControllerInput] = useAtom(ControllerInput);
    const [keyboardMode, setKeyboardMode] = useAtom(KeyboardMode);

    const [controller1Name, setController1Name] = useState("Not Assigned");
    const [controller2Name, setController2Name] = useState("Not Assigned");

    const [, reloadComponent] = useState<number>(0);

    let initialPageLoad = true;

    let pressed_key = "";

    // All inputs will be in the format of a list, where the first element is input value (1 for button and between (-1,1) for axis)
    // and the second element is the action

    // Likely one of the most important functions in the software package
    // This function listens to the controller input and updates the ControllerInput global state. When this state is changed,
    // ROS.tsx recognizes this and publishes the input to the device running the rosbridge server (RPi4 inside the enclosure)
    const input_listener = () => {
        let controller1 = "null";
        let controller2 = "null";

        let controller1Index = -1;
        let controller2Index = -1;

        let controller1Set = false;

        // Find out the controller names, stored in both a normal javascript variable for this function and as a react state to be displayed in GUI
        for (let i = 0; i < profilesList.length; i++) {
            if (currentProfile == profilesList[i].name) {
                controller1 = profilesList[i].controller1;
                setController1Name(controller1);

                controller2 =
                    profilesList[i].controller2 != null
                        ? profilesList[i].controller2
                        : "null";
                setController2Name(controller2);
            }
        }

        // Find out the controller index in navigator
        for (
            let controllerIndex = 0;
            controllerIndex < navigator.getGamepads().length;
            controllerIndex++
        ) {
            if (
                navigator.getGamepads()[controllerIndex]?.id == controller1 &&
                !controller1Set
            ) {
                controller1Set = true; // This is in case controller 1 and controller 2 have the same name
                controller1Index = controllerIndex;
                setController1Detected(true);
            } else if (navigator.getGamepads()[controllerIndex]?.id == controller2) {
                controller2Index = controllerIndex;
                setController2Detected(true);
            }
        }

        const controllerIndexes = [controller1Index, controller2Index];
        const controllerInput: (number | undefined)[] = [
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ]; // Current controller input from pilot. Must match length of ControllerInput defined in Atmos.tsx

        if (controllerIndexes[0] == -1) {
            setController1Detected(false);
        }
        if (controllerIndexes[1] == -1) {
            setController2Detected(false);
        }

        // Iterate through controller 1 and controller 2
        for (let i = 0; i < 2; i++) {
            if (Object.keys(mappings[i]).length == 0) {
                // This indicates that no mappings have been loaded for this controller.
                if (initialPageLoad) {
                    console.log(
                        "No mappings have been loaded. Go to the settings and load a profile."
                    );
                    initialPageLoad = false;
                }
                continue;
            }

            // The below for loop is quick, so it will detect a button press on any button even if it was for a short time

            const gamepadInstance = navigator.getGamepads()[controllerIndexes[i]];

            for (let j = 0; j < (gamepadInstance?.buttons.length as number); j++) {
                if (gamepadInstance?.buttons[j].pressed) {
                    //Button press detected
                    if (mappings[i]["buttons"][j] != "None") {
                        for (let k = 0; k < pilotActions.length; k++) {
                            if (pilotActions[k] == mappings[i]["buttons"][j]) {
                                controllerInput[k - 1] = 1;
                                break;
                            }
                        }
                    }
                }
            }
            for (let j = 0; j < (gamepadInstance?.axes.length as number); j++) {
                const deadzone =
                    Number(mappings[i]["deadzones"][j]) > 0.05
                        ? Number(mappings[i]["deadzones"][j])
                        : 0.05;
                if (Math.abs(gamepadInstance?.axes[j] as number) >= deadzone) {
                    // The axis has been moved beyond it's "deadzone", which means that this is actual pilot input and not controller drift
                    if (mappings[i]["axes"][j] != "None") {
                        for (let k = 0; k < pilotActions.length; k++) {
                            if (pilotActions[k] == mappings[i]["axes"][j]) {
                                controllerInput[k - 1] = Math.round(
                                    j % 2 == 0
                                        ? (gamepadInstance?.axes[j] as number) * 100
                                        : (gamepadInstance?.axes[j] as number) * -100
                                );
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (keyboardMode && pressed_key) {
            for (let i = 0; i < keyboardInputMap.length; i++) {
                if (keyboardInputMap[i][0] == pressed_key) {

                    for (let j = 0; j < pilotActions.length; j++) {
                        if (pilotActions[j] == keyboardInputMap[i][1]) {
                            controllerInput[j - 1] = keyboardInputMap[i][2] as number;
                            break;
                        }
                    }

                    break;
                }
            }
        }

        setControllerInput(controllerInput);
    };



    useEffect(() => {

        if (keyboardMode) {
            // Add keyboard input listeners for keyboard mode
            window.addEventListener(
                "keydown",
                function (e) {
                    pressed_key = e.key;
                },
                false
            );
            window.addEventListener(
                "keyup",
                function () {
                    pressed_key = "";
                },
                false
            );
        } else {
            // Remove keyboard input listeners for keyboard mode
            window.removeEventListener(
                "keydown",
                function (e) {
                    pressed_key = e.key;
                },
                false
            );
            window.removeEventListener(
                "keyup",
                function () {
                    pressed_key = "";
                },
                false
            );
        }
        document.addEventListener("keypress", (event) => {
            switch (event.key) {
                case "0":
                    setThrusterMultipliers([0, 0, 0, 0, 0, 0, 0]);
                    break;
                case "1":
                    setThrusterMultipliers([100, 100, 100, 100, 100, 100, 70]);
                    break;
                case "2":
                    setThrusterMultipliers([100, 0, 0, 100, 0, 0, 0]);
                    break;
                case "3":
                    setThrusterMultipliers([0, 100, 0, 0, 0, 0, 0]);
                    break;
                default:
                    break;
            }
        });

        // Constantly run the input listener
        const interval = setInterval(() => {
            reloadComponent(Math.random());
            input_listener();
        }, 100); // 100 ms or 10Hz
        return () => clearInterval(interval); // Stop listening for input when we click off the BotTab
    }, [keyboardMode, currentProfile]);

    // Create a listener to update width on window resize
    React.useEffect(() => { window.addEventListener("resize", () => { setWidth(window.innerWidth); }); }, []);

    return (
        <Grid container spacing={1}>
            <Table>
                <TableBody>
                    <TableRow>
                        <TableCell>
                            Current Profile: {currentProfile}
                        </TableCell>
                        <TableCell>
                            Controller 1: {controller1Name}
                        </TableCell>
                        <TableCell>
                            Controller 2: {controller2Name}
                        </TableCell>
                        <TableCell>
                            Keyboard Mode {keyboardMode ? "on" : "off"}
                            <Switch
                                onChange={(event: React.ChangeEvent<HTMLInputElement>) => { setKeyboardMode(event.target.checked); }}
                            />
                        </TableCell>
                        <TableCell>
                            {diagnostics_data_2}
                        </TableCell>
                    </TableRow>
                </TableBody>
            </Table>

            {URLs.filter((url) => url !== "N/A").map((url, index) => (
                <Grid item xs display="flex" flexDirection="column" justifyContent="space-between" key={index}>
                    <Box display="flex" justifyContent="center">
                        <Paper elevation={7} sx={{ alignSelf: "flex-start", borderRadius: "12px", width: `${width * 0.428}`, marginBottom: "8px" }}>
                            <h3 style={{ textAlign: "center", margin: "0" }}>Camera {index + 1}</h3>
                            <AltCamera ip={url} width={width} />
                        </Paper>
                    </Box>
                </Grid>
            ))}
        </Grid>
    );
}