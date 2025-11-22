import { Box, CircularProgress } from "@mui/material";
import { useEffect, useState } from "react";

export default function AxisDisplay(props: { controller: number, axis: number }) {
    const [, reloadComponent] = useState<number>(0);
    useEffect(() => {
        setInterval(() => {
            reloadComponent(Math.random());
        }, 100);
    }, []);

    return (
        <Box display="flex" alignItems="center" marginBottom="6px">
            <CircularProgress variant="determinate" value={(navigator.getGamepads()[props.controller]?.axes[props.axis] ?? 0) * 50 + 50} sx={{ width: "100%", marginBottom: "4px" }} />
        </Box>
    );
}