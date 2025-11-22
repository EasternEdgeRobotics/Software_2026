import { useAtom } from "jotai";
import ROSLIB, { Ros } from "roslib";
import {
    IsROSConnected, ROSIP, CameraURLs, ThrusterMultipliers, RequestingConfig,
    RequestingProfilesList, Mappings, ProfilesList, CurrentProfile, ControllerInput,
    DiagnosticsData1, DiagnosticsData2, PilotActions
} from "./Atoms";
import React from "react";

let initial_page_load = true;
let first_input_sent = false;

export function InitROS() {
    const [RosIP] = useAtom(ROSIP); // The ip of the device running the rosbridge server (will be the Pi4 in enclosure)
    const [, setIsRosConnected] = useAtom(IsROSConnected); // Used in BotTab, indicates if we are communicating with the rosbridge server
    const [thrusterMultipliers] = useAtom(ThrusterMultipliers);
    const [requestingConfig, setRequestingConfig] = useAtom(RequestingConfig); // Used for requesting controller mappings data from the database running in the Pi4
    const [requestingProfilesList, setRequestingProfilesList] = useAtom(RequestingProfilesList); // Used for requesting profiles data from the database running in the Pi4
    const [mappings, setMappings] = useAtom(Mappings); // Controller mappings
    const [, setProfilesList] = useAtom(ProfilesList); // The known list of pilot profiles
    const [cameraURLs, setCameraURLs] = useAtom(CameraURLs);
    const [currentProfile,] = useAtom(CurrentProfile);
    const [controllerInput] = useAtom(ControllerInput); // The current controller input from the pilot
    const [, setDiagnosticsData1] = useAtom(DiagnosticsData1);
    const [, setDiagnosticsData2] = useAtom(DiagnosticsData2);
    const [pilotActions] = useAtom(PilotActions);

    const [hasRecieved, setHasRecieved] = React.useState<boolean>(false);
    const [ros, setRos] = React.useState<Ros>(new ROSLIB.Ros({}));

    ros.on("connection", () => {
        console.log("ROS Connected!");
        setIsRosConnected(true);
        // thrusterRequestService.callService(0, (response: {power: number, surge: number, sway: number, heave: number, pitch: number, yaw: number}) =>
        //     {setThrusterMultipliers([response.power, response.surge, response.sway, response.heave, response.pitch, response.yaw]);});
        setHasRecieved(true);
    });
    ros.on("close", () => {
        console.log("ROS Disconnected!");
        setIsRosConnected(false);
        setHasRecieved(false);
    });
    ros.on("error", () => { /* empty */ }); // to prevent page breaking

    React.useEffect(() => {
        ros.connect(`ws://${RosIP}:9090`);
        setInterval(() => {
            if (!ros.isConnected) {
                setRos(new ROSLIB.Ros({}));
                ros.connect(`ws://${RosIP}:9090`);
            }
        }, 1000);
    }, []);

    // Create a publisher on the "/pilot_input" ros2 topic, using the default String message which will be used from transporting JSON data
    const controllerInputTopic = new ROSLIB.Topic({
        ros: ros,
        name: "/pilot_input",
        messageType: "eer_interfaces/PilotInput",
        queue_size: 1
    });

    // Publish the new controller input whenever it changes (10 Hz)
    React.useEffect(() => {
        const controllerInputVals = new ROSLIB.Message({
            surge: controllerInput[0],
            sway: controllerInput[1],
            heave: (controllerInput[8] || controllerInput[9]) ? ((controllerInput[8]||0)-(controllerInput[9]||0))*100 : controllerInput[2],
            pitch: (controllerInput[10] || controllerInput[11]) ? ((controllerInput[10]||0)-(controllerInput[11]||0))*100 : controllerInput[3],
            roll: (controllerInput[12] || controllerInput[13]) ? ((controllerInput[12]||0)-(controllerInput[13]||0))*100 : controllerInput[4],
            yaw: controllerInput[5],
            open_claw: controllerInput[6] ? true : false,
            close_claw: controllerInput[7] ? true : false,
            turn_front_servo_cw: controllerInput[14] ? true : false,
            turn_front_servo_ccw: controllerInput[15] ? true : false,
            turn_back_servo_cw: controllerInput[16] ? true : false,
            turn_back_servo_ccw: controllerInput[17] ? true : false,
            brighten_led: controllerInput[18] ? true : false,
            dim_led: controllerInput[19] ? true : false,
            turn_stepper_cw: controllerInput[20] ? true : false,
            turn_stepper_ccw: controllerInput[21] ? true : false,
            read_outside_temperature_probe: controllerInput[22] ? true : false,
            enter_auto_mode: controllerInput[23] ? true : false,
            power_multiplier: thrusterMultipliers[0],
            surge_multiplier: thrusterMultipliers[1],
            sway_multiplier: thrusterMultipliers[2],
            heave_multiplier: thrusterMultipliers[3],
            pitch_multiplier: thrusterMultipliers[4],
            roll_multiplier: thrusterMultipliers[5],
            yaw_multiplier: thrusterMultipliers[6]
        });
        controllerInputTopic.publish(controllerInputVals);
        first_input_sent = true;
    }
        , [controllerInput]);

    // Create a ROS service on the "/profile_config" topic using a custom EER service interface
    const configClient = new ROSLIB.Service({
        ros: ros,
        name: "/get_config",
        serviceType: "eer_interfaces/srv/GetConfig"
    });

    const saveConfigPublisher = new ROSLIB.Topic({
        ros: ros,
        name: "/save_config",
        messageType: "eer_interfaces/msg/SaveConfig"
    });

    
    // Run the block of code below whenever the RequestingConfig global state is changed
    React.useEffect(() => {
        if (requestingConfig.state == 0) { // State 0 indicates that we are saving mappings for a certain profile to the database
            const config: {name: string, controller1: string, controller2: string, cameras: string[], mappings: {[controller: number]: { [type: string]: { [index: number]: string}}} } = {
                name: requestingConfig.name,
                controller1: requestingConfig.controller1,
                controller2: requestingConfig.controller2,
                cameras: cameraURLs,
                mappings: mappings
            };

            const message = new ROSLIB.Message({
                name: requestingConfig.name,
                data: JSON.stringify(config)
            });

            saveConfigPublisher.publish(message);
        }
        else if (requestingConfig.state == 1) { // State 1 indicates that we are requesting mappings for a certain profile from the database
            const request = new ROSLIB.ServiceRequest({
                name: requestingConfig.name
            });

            console.log("requesting config " + requestingConfig.name);

            // When a response is recieved for a ROS service request, it is expected to be run through a "callback function". In this case, the function definition is being
            // used as a parameter to configClient.callService instead of a reference to the function.
            configClient.callService(request, function (result) {
                const databaseStoredMappings = JSON.parse(result.config)["mappings"]; // Turn the recieved string into a JSON object 
                const tmp = mappings;
                
                for (let i = 0; i < 2; i++) {
                    if ([requestingConfig.controller1, requestingConfig.controller2][i] == "recognized") {
                        tmp[i] = { "buttons": {}, "axes": {}, "deadzones": {} };
                        tmp[i]["buttons"] = Object.keys(databaseStoredMappings[i]["buttons"]).reduce((acc, key) => {
                            const action = databaseStoredMappings[i]["buttons"][key as unknown as number];
                            acc[key as unknown as number] = pilotActions.includes(action) ? action : "None";
                            return acc;
                        }, {} as { [index: number]: string });
                          
                        tmp[i]["axes"] = Object.keys(databaseStoredMappings[i]["axes"]).reduce((acc, key) => {
                            const action = databaseStoredMappings[i]["axes"][key as unknown as number];
                            acc[key as unknown as number] = pilotActions.includes(action) ? action : "None";
                            return acc;
                        }, {} as { [index: number]: string });
                        tmp[i]["deadzones"] = databaseStoredMappings[i]["deadzones"];
                    }
                }

                const cameraIPs = [];

                for (let i = 0; i < 4; i++) {
                    try {
                        cameraIPs.push(JSON.parse(result.config)["cameras"][i]);
                    } catch (err) {
                        cameraIPs.push("http://");
                    }
                }

                setCameraURLs(cameraIPs);

                // Set the global Mappings state to the tmp object which now holds the mappings recieved from the database. Note that just running setMappings(databaseStoredMappings) didn't work
                setMappings(tmp);
            });
        }
        if (requestingConfig.state != 2) { // State 2 doesn't do anything, and is used as the default state. Whenever a service call is done for state 0 or 1, RequestingConfig returns to state 2
            setRequestingConfig({ state: 2, name: "default", controller1: "null", controller2: "null" });
        }
    }
        , [requestingConfig]);


    // Create a ROS service on the "/profile_list" topic using a custom EER service interface
    const listProfilesService = new ROSLIB.Service({
        ros: ros,
        name: "/list_configs",
        serviceType: "eer_interfaces/srv/ListConfig"
    });

    const deleteProfilesService = new ROSLIB.Service({
        ros: ros,
        name: "/delete_config",
        serviceType: "eer_interfaces/srv/DeleteConfig" 
    });

    // Run the block of code below whenever the RequestingProfilesList global state is changed
    React.useEffect(() => {
        if (requestingProfilesList == 0) { // State 0 indicates that we would like to delete a profile from the database.
            // Note that profiles are created when RequestingConfig state 0 is called and the database doesn't recognize the name of the profile coming from the GUI
 
            const request = new ROSLIB.ServiceRequest({
                name: currentProfile
            });

            deleteProfilesService.callService(request, function (result) {
                console.log(result.success); // Returns true if success, false if failure.
                setRequestingProfilesList(1); // Re-fetch the profile list
            });
        }
        else if (requestingProfilesList == 1) { // State 1 indicates that we are requesting the entire list of profiles from the database
            const request = new ROSLIB.ServiceRequest({});

            listProfilesService.callService(request, function (result) {
                if (result.names != "[]" && result.configs != "[]") { // Do not load the result if there are no profiles stored (i.e. empty list is returned)

                    const profilesList: {
                        id: number;
                        name: string;
                        controller1: string;
                        controller2: string;
                    }[] = [];

                    for (let i = 0; i < result.configs.length; i++) {
                        try {
                            const config = JSON.parse(result.configs[i]);
                            
                            // Ignore `waterwitch_config`, which has a different format
                            if (config.name == "waterwitch_config") continue;

                            profilesList.push({
                                id: profilesList.length,
                                name: config.name,
                                controller1: config.controller1,
                                controller2: config.controller2
                            });

                            setProfilesList(profilesList);
                        } catch (err) { console.log("Could not load " + result.names[i] + ": " + err); }
                    }
                }
                else { // If we recieve an empty list, then just set the profile to "default". The pilot will have to create a profile on the fly that will only be saved locally, and will be gone on page reload
                    setProfilesList([{ id: 0, name: "default", controller1: "null", controller2: "null" }]);
                }
            });
        }
        setRequestingProfilesList(2);
    }
        , [requestingProfilesList]);

    React.useEffect(() => {
        setRequestingProfilesList(1);
    }, []);

    if (initial_page_load && first_input_sent) {
        const DiagnosticsData1Listener = new ROSLIB.Topic({
            ros: ros,
            name: "/diagnostics_data_1",
            messageType: "std_msgs/String"
        });

        DiagnosticsData1Listener.subscribe(function (message) {
            setDiagnosticsData1((message as { data: string }).data);
        });

        const DiagnosticsData2Listener = new ROSLIB.Topic({
            ros: ros,
            name: "/diagnostics_data_2",
            messageType: "std_msgs/String"
        });

        DiagnosticsData2Listener.subscribe(function (message) {
            setDiagnosticsData2((message as { data: string }).data);
        });

        initial_page_load = false;
    }

    return (null);
}