import { InputLabel } from "@mui/material";
import { useEffect, useState } from "react";

export default function ButtonLabel(props: { controller: number, button: number }) {
    const [, reloadComponent] = useState<number>(0);
    useEffect(() => {
        setInterval(() => {
            reloadComponent(Math.random());
        }, 100);
    }, []);

    return (
        <InputLabel sx={{ color: navigator.getGamepads()[props.controller]?.buttons[props.button].pressed ? "#87CEEB" : "light-grey" }}>Button {props.button}</InputLabel>
    );
}