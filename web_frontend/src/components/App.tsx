import { Box, Button, Tab, Tabs } from "@mui/material";
import { ArrowUpRightSquare, Bot, Camera, FishIcon, Wrench } from "lucide-react";
import { useState } from "react";
import CameraTab from "./CameraTab";
import SettingsTab from "./SettingsTab";
import React from "react";
import SafetyDisclaimer from "./SafetyDisclaimer";
import { InitROS } from "../api/ROS";
import { BotTab } from "./BotTab";
import CarpTab from "./CarpTab";




// Navbar, calls to render each window, and a popout button

export function RenderTab(props: { tab: number }) {
    //return respective tab depending on which tab is chosen, or else return null (which should never happen)
    switch (props.tab) {
        case 0: return (<CameraTab />);
        case 1: return (<BotTab />);
        case 2: return (<SettingsTab />);
        case 3: return (<CarpTab />);
        default: return (null);
    }
}

export default function App() {
    const [tabIndex, setTabIndex] = useState<number>(0);
    
    return (
        <Box>
            <InitROS /> {/* Initialize ROS */}
            <SafetyDisclaimer /> {/* Display the safety disclaimer */}
            <Tabs value={tabIndex} onChange={(_, index) => { setTabIndex(index); }} centered> {/* Navbar on the top of the screen */}
                <Tab label={<Camera />} />
                <Tab label={<Bot />} />
                <Tab label={<Wrench />} />
                <Tab label ={<FishIcon />} />
            </Tabs>
            <br />
            {/* Popout window button, only for camera and bot tasks*/}
            <Box position="absolute" top="8px" right="8px">
                {/* To allow new tabs for popout, change index at the array below */}
                <Button variant="outlined" disabled={![0, 1].includes(tabIndex)} onClick={() => { window.open(`/${tabIndex}`, "", "popout"); }}><ArrowUpRightSquare /></Button>
            </Box>
            {/* Pass through the tab to the render function in order to render the component */}
            <RenderTab tab={tabIndex} />
        </Box>
    );
}