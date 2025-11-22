import { Button, Dialog, DialogActions, DialogContent, DialogContentText, DialogTitle } from "@mui/material";
import React from "react";

export default function SafetyDisclaimer() {
    const [open, setOpen] = React.useState<boolean>(true);

    return (
        <Dialog open={open}>
            <DialogTitle>Eastern Edge Safety Disclaimer</DialogTitle>
            <DialogContent>
                <DialogContentText component='span'>
                    Prior to using the Eastern Edge ROV control interface, please ensure the following:<br />
                    <ul>
                        <li>The tether is neatly coiled in company standard figure 8.</li>
                        <li>Vehicle power is switched in &#34;OFF&#34; position</li>
                        <li>Deck Crew is wearing Eye Protection, Personal Flotation Devices, and Work Boots.</li>
                        <li>Topside devices are fully charged, and a spare charger is brought.</li>
                        <li>All access hazards are removed.</li>
                    </ul>
                    After reading this, press the &quot;I Agree&quot; button to continue.
                </DialogContentText>
            </DialogContent>
            <DialogActions>
                <Button onClick={() => setOpen(false)}>I Agree</Button>
            </DialogActions>
        </Dialog>
    );
}