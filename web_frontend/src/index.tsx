import { ThemeProvider } from "@emotion/react";
import { Box, CssBaseline, createTheme } from "@mui/material";
import React from "react";
import ReactDOM from "react-dom/client";
import App from "./components/App";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import CameraTab from "./components/CameraTab";
import { Mate2024ScienceController } from "./mate_2024_science_tasks/pages/Mate2024ScienceController";
import Mate2024TabletApp from "./mate_2024_science_tasks/pages/Mate2024TabletApp";

//fonts
import "@fontsource/roboto/300.css";
import "@fontsource/roboto/400.css";
import "@fontsource/roboto/500.css";
import "@fontsource/roboto/700.css";
import { BotTab } from "./components/BotTab";

const root = ReactDOM.createRoot(document.getElementById("root") as HTMLElement);
root.render(
  <React.StrictMode>
    <ThemeProvider theme={createTheme({palette: {mode: "dark"}})}>
      <CssBaseline />
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<App />} />
          <Route path="/0" element={<Box sx={{display: "flex", alignItems: "center", height: "100vh"}}><CameraTab /></Box>} />
          <Route path="/1" element={<Box><br/><BotTab /></Box>} />
          <Route path="/mate_2024_science" element={<Mate2024ScienceController />} />
          <Route path="/mate_2024_tablet" element={<Mate2024TabletApp />} />
        </Routes>
      </BrowserRouter>
    </ThemeProvider>
  </React.StrictMode>
);
