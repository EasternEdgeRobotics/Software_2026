import { useAtom } from "jotai";
import { CameraURLs, ROSIP, ProfilesList, CurrentProfile, RequestingProfilesList, RequestingConfig } from "../api/Atoms";
import { Box } from "@mui/system";
import { Button, Divider, FormControl, Grid, InputLabel, Select, TextField, MenuItem } from "@mui/material";
import { Trash2 } from "lucide-react";
import ProfileEditor from "./ProfileEditor";
import { useState } from "react";

export default function SettingsTab() {

    const [URLs, setURLs] = useAtom(CameraURLs);
    const [RosIP, setRosIP] = useAtom(ROSIP);

    const [editorOpen, setEditorOpen] = useState<boolean>(false);

    const [currentProfile, setCurrentProfile] = useAtom(CurrentProfile);
    const [profilesList,] = useAtom(ProfilesList);

    const [, setRequestingProfilesList] = useAtom(RequestingProfilesList);
    const [, setRequestingConfig] = useAtom(RequestingConfig);

    const setCameraIP = (camera: number, ip: string) => {
        setURLs(URLs.map((item, index) => index === camera ? ip : item));
    };

    const save = () => {
        setURLs(URLs.map(ip => !ip.startsWith("http://") && !ip.startsWith("https://") ? "http://" + ip : ip));
        const settings: { [id: string]: string[] } = {};
        settings.CameraURLs = URLs.map(ip => !ip.startsWith("http://") && !ip.startsWith("https://") ? "http://" + ip : ip);
        localStorage.setItem("ROS_IP", RosIP);
    };

    // The loadProfile function iterates through each profile, checking if that profile is the current profile and which controller is recognized
    const loadProfile = (profileName: string) => {
        const controllers_recognized = [false, false];
        let current_profile_index = -1;

        for (let i = 0; i < profilesList.length; i++) {
            if (profilesList[i].name == profileName) {
                current_profile_index = i;
            }
        }

        if (current_profile_index == -1) {
            console.log("Can't find current profile in the profiles list!");
            return;
        }
        for (let controllerIndex = 0; controllerIndex < navigator.getGamepads().length; controllerIndex++) {
            if (navigator.getGamepads()[controllerIndex] == null) {
                continue;
            }
            // Check if either controller is currently being seen by the browser
            if (profilesList[current_profile_index].controller1 == navigator.getGamepads()[controllerIndex]?.id && !controllers_recognized[0]) { //  !controllers_recognized[0] handles the case where controllers 1 and 2 have the same name
                console.log("Controller 1 recognized");
                controllers_recognized[0] = true;
            }
            if (profilesList[current_profile_index].controller2 == navigator.getGamepads()[controllerIndex]?.id) {
                console.log("Controller 2 recognized");
                controllers_recognized[1] = true;
            }
        }
        // Recieve latest bindings for this profile
        setRequestingConfig({
            state: 1, name: profileName,
            controller1: (controllers_recognized[0]) ? "recognized" : "null",
            controller2: (controllers_recognized[1]) ? "recognized" : "null"
        });
    };

    return (
        <Box>
            <Divider>ROS</Divider><br />
            <Box display="flex" justifyContent="center">
                {/* TODO: may make this a part of the web servers env? */}
                <TextField label="ROS Bridge IP (Must Refresh After Changing)" variant="outlined" sx={{ width: "40%" }} value={RosIP} onChange={(e) => setRosIP(e.target.value)} />
            </Box>
            <br /><Divider>Cameras</Divider><br />
            <Grid container spacing={2} padding={1}>
                <Grid item xs={6}>
                    <TextField label="Camera 1 URL" variant="outlined" sx={{ width: "100%" }} value={URLs[0]} onChange={(e) => setCameraIP(0, e.target.value)} />
                </Grid>
                <Grid item xs={6}>
                    <TextField label="Camera 2 URL" variant="outlined" sx={{ width: "100%" }} value={URLs[1]} onChange={(e) => setCameraIP(1, e.target.value)} />
                </Grid>
                <Grid item xs={6}>
                    <TextField label="Camera 3 URL" variant="outlined" sx={{ width: "100%" }} value={URLs[2]} onChange={(e) => setCameraIP(2, e.target.value)} />
                </Grid>
                <Grid item xs={6}>
                    <TextField label="Camera 4 URL" variant="outlined" sx={{ width: "100%" }} value={URLs[3]} onChange={(e) => setCameraIP(3, e.target.value)} />
                </Grid>
            </Grid>
            <Box position="absolute" bottom="8px" left="10%" width="80%">
                <Button variant="contained" sx={{ width: "100%" }} onClick={() => { save(); }}>Save Settings</Button>
            </Box>
            <br /><Divider>Profiles</Divider><br />
            <Box display="flex" justifyContent="center">
                <Grid container width="50%" spacing={1}>
                    <Grid item xs={11}>
                        <FormControl fullWidth >
                            <InputLabel>Profile</InputLabel>
                            <Select value={currentProfile} label="Profile" onClick={() => { setRequestingProfilesList(1); }} onChange={(e) => {
                                setCurrentProfile(e.target.value);
                                loadProfile(e.target.value);
                                return;
                            }} sx={{ width: "100%" }}>
                                {profilesList.map((profile) => {
                                    // Add menu item for every profile
                                    return <MenuItem key={profile.name} value={profile.name}>{profile.name}</MenuItem>;
                                })
                                }
                            </Select>
                        </FormControl>
                    </Grid>
                    <Grid item xs={1}>
                        <Button variant="outlined" sx={{ height: "56px", width: "100%" }} onClick={() => { setRequestingProfilesList(0); }}><Trash2 /></Button>
                    </Grid>
                    <Grid item xs={12}>
                        <Button variant="contained" sx={{ height: "56px", width: "100%" }} onClick={() => {
                            if (currentProfile != "") {
                                setEditorOpen(true);
                            }
                        }}>Profile Editor</Button>
                        <ProfileEditor open={editorOpen} onClose={() => setEditorOpen(false)} />
                    </Grid>
                </Grid>
            </Box>
        </Box>
    );
}