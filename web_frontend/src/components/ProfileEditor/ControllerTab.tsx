import { Box, Divider, FormControl, Grid, InputLabel, MenuItem, Select, TextField } from "@mui/material";
import ButtonLabel from "./ButtonLabel";
import AxisDisplay from "./AxisDisplay";
import { useAtom } from "jotai";
import { Mappings, PilotActions } from "../../api/Atoms";
import { useState } from "react";

export default function ControllerTab(props: { controller: number; index: number }) {
    const [mappings, setMappings] = useAtom(Mappings);

    // The pilot actions are interperted once they get the to the RPi4 inside the enclosure
    const [pilotActions,] = useAtom(PilotActions);

    const [, reloadComponent] = useState<number>(0);

    return (
        <Box>
            <Divider sx={{ marginBottom: "8px" }}>Buttons</Divider>
            <Grid container spacing={1}>
                {navigator.getGamepads()[props.controller]?.buttons.map((_, index) => (
                    <Grid item xs={3}>
                        <FormControl fullWidth>
                            <ButtonLabel controller={props.controller} button={index} />
                            <Select value={mappings[props.index]["buttons"][index]} label={`Button ${index}`} onChange={(e) => {
                                const tmp = mappings;
                                tmp[props.index]["buttons"][index] = e.target.value as string;
                                setMappings(tmp);
                                reloadComponent(Math.random()); //i really dont know why this needs to be here but it does apparently
                            }} sx={{ width: "100%" }}>
                                {pilotActions.map((action) => {
                                    //Add menu item for every action
                                    return <MenuItem value={action}>{action}</MenuItem>;
                                })}
                            </Select>
                        </FormControl>
                    </Grid>
                ))}
            </Grid>
            <Divider sx={{ marginY: "8px" }}>Axes</Divider>
            <Grid container spacing={1}>
                {navigator.getGamepads()[props.controller]?.axes.map((_, index) => (
                    <Grid item xs={6}>
                        <Grid container spacing={1}>
                            <Grid item xs={8.25}>
                                <FormControl fullWidth>
                                    <InputLabel>Axis {index} Action</InputLabel>
                                    <Select defaultValue={mappings[props.index]["axes"][index]} label={`Axis ${index} Action`} onChange={(e) => {
                                        const tmp = mappings;
                                        tmp[props.index]["axes"][index] = e.target.value as string;
                                        setMappings(tmp);
                                        reloadComponent(Math.random()); //i really dont know why this needs to be here but it does apparently
                                    }}>
                                        {pilotActions.map((action) => {
                                            //Add menu item for every action
                                            return <MenuItem value={action}>{action}</MenuItem>;
                                        })}
                                    </Select>
                                </FormControl>
                            </Grid>
                            <Grid item xs={1}>
                                <AxisDisplay controller={props.controller} axis={index} />
                            </Grid>
                            <Grid item xs={2.75}>
                                <TextField InputProps={{ inputProps: { min: 0, max: 1, step: 0.1 } }} label="Deadzone" value={mappings[props.index]["deadzones"][index]} error={1 < Number(mappings[props.index]["deadzones"][index]) || 0 > Number(mappings[props.index]["deadzones"][index])} onChange={(e) => {
                                    //abs(Deadzone) should be !=0 and <1
                                    const tmp = mappings;
                                    if (!isNaN(Number(e.target.value)))
                                        tmp[props.index]["deadzones"][index] = e.target.value;
                                    else if (e.target.value == ".")
                                        tmp[props.index]["deadzones"][index] = "0.";
                                    setMappings(tmp);
                                    reloadComponent(Math.random());
                                }} />
                            </Grid>
                        </Grid>
                    </Grid>
                ))}
            </Grid>
        </Box>
    );
}