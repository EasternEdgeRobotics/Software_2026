import { Box, Button, Checkbox, FormGroup, FormControlLabel, Typography, AppBar, Stack } from "@mui/material";
import React, { useState, useEffect } from "react";
import { atom, useAtom } from "jotai";
import { grey, green, yellow, red } from "@mui/material/colors";
import { Task } from "../types/Task";
import taskJSON from "./tasks.json";
import { Row } from "react-bootstrap";
import { ROSIP } from "../../api/Atoms";
import ROSLIB, { Ros } from "roslib";

/* 
The purpose of the Mate2024TabletApp component is to provide the deck chief with a GUI which is to be used on a tablet that:

  1. Provides a checklist of tasks and subtasks that can be ticked/unticked 

  2. Provides a main timer, which can be started, paused and reset, permanently displayed as bar at the top of the screen

  3. Provides secondary timers, one for each task, which can be started paused and reset independently of the main timers and other task-specific timers

Notes:

  1. Testing on the usage device will be necessary to confirm expected behaviour
  
  2. It may be worthwhile to indicate point values within 'subTask' text ('tasks.json'), e.g., " - 10 points" at the end of "Trigger the release of the multi-function node's recovery float - 10 points"
*/

const taskPublisherAtom = atom<ROSLIB.Topic<ROSLIB.Message> | null>(null);

export default function Mate2024TabletApp() {
  const [ros, setRos] = React.useState<Ros>(new ROSLIB.Ros({}));
  const [RosIP] = useAtom(ROSIP);
  const [saved_tasks, setTasks] = useState<{ [key: string]: boolean }>({});
  const [, setTaskPublisher] = useAtom(taskPublisherAtom);
  const [rosConnected, setRosConnected] = React.useState(true);


  ros.on("error", () => {
    setRosConnected(false);
  }); // to prevent page breaking

  React.useEffect(() => {
    ros.connect(`ws://${RosIP}:9090`);
    setInterval(() => {
      if (!ros.isConnected) {
        setRos(new ROSLIB.Ros({}));
        ros.connect(`ws://${RosIP}:9090`);
      } else {
        setRosConnected(true);
      }
    }, 1000);
  }, []);


  useEffect(() => {
    const taskClient = new ROSLIB.Service({
      ros: ros,
      name: "/all_tasks",
      serviceType: "eer_interfaces/Config",
    });
    const request = new ROSLIB.ServiceRequest({});
    taskClient.callService(
      request,
      function (result) {
        console.log("All tasks:", result.result);
        setTasks(JSON.parse(result.result));
      },
      function (error) {
        console.log("Error calling service:", error);
      }
    );

    const task_publisher = new ROSLIB.Topic({
      ros: ros,
      name: "task_updates",
      messageType: "std_msgs/String",
    });


    task_publisher.subscribe((message) => {
      const data: { id: string; sender: string; status: boolean } = JSON.parse(
        (message as { data: string }).data
      );
      console.log("Received message UPDATE ", data);
      // if (data.sender === "tablet") return;

      setTasks((prevSaved) => ({
        ...prevSaved,
        [data.id]: data.status,
      }));
    });

    setTaskPublisher(task_publisher);
  }, [ros]);




  return (
    <FormGroup>
      <MainTimer />
      <div style={{ height: "260px" }}></div>
      {!rosConnected && ( // Add this block
        <div
          style={{
            backgroundColor: "red",
            color: "white",
            textAlign: "center",
          }}
        >
          ROS not connected
        </div>
      )}
      {Tasks({
        name: "TASK 1 : Coastal Pioneer Array",
        tasks: taskJSON.task1.tasks,
        saved: saved_tasks,
        setTasks,
      })}

      {Tasks({
        name: "TASK 2 : Deploy SMART cables",
        tasks: taskJSON.task2.tasks,
        saved: saved_tasks,
        setTasks,
      })}

      {Tasks({
        name: "TASK 3 : From the Red Sea to Tenesse",
        tasks: taskJSON.task3.tasks,
        saved: saved_tasks,
        setTasks,
      })}

      {Tasks({
        name: "TASK 4 : MATE Floats",
        tasks: taskJSON.task4.tasks,
        saved: saved_tasks,
        setTasks,
      })}
    </FormGroup>
  );
}

function Tasks(props: {
  name: string;
  tasks: Task[];
  saved?: { [key: string]: boolean };
  setTasks: React.Dispatch<React.SetStateAction<{ [key: string]: boolean }>>;
}) {

  return (
    <FormGroup style={{ paddingLeft: "10vw", paddingRight: "8vw" }}>
      <Row
        style={{
          display: "flex",
          justifyContent: "flex-start",
          alignItems: "center",
          marginBottom: "1vh",
        }}
      >
        <h1 style={{ marginTop: "0px", marginBottom: "0px" }}>{props.name}</h1>
        <div style={{ width: "2vw" }}></div>
        <Timer />
      </Row>
      {RenderTasks(
        props.tasks,
        undefined,
        props.name.split(":")[0].trim().replace(" ", "_"),
        props.saved,
        props.setTasks,
      )}
      <div style={{ height: "3vh" }}></div>
    </FormGroup>
  );
}

function RenderTasks(
  tasks: Task[],
  level = 0,
  id = "",
  saved?: { [key: string]: boolean },
  setTasks?: React.Dispatch<React.SetStateAction<{ [key: string]: boolean }>>,
) {
  const [task_publisher] = useAtom(taskPublisherAtom);

  return tasks.map((task, index) => {
    const currentID = id ? `${id}:${index + 1}` : `${index + 1}`;
    const currentState = saved ? (saved[currentID] ?? false) : false;

    return (
      <div
        key={currentID}
        style={{
          paddingLeft: `${level ** 0.5 * 2}em`,
          paddingBottom: "5px",
        }}
      >
        {!task.subTasks && (
          <FormControlLabel
            control={
              <Checkbox
                key={currentID}
                defaultChecked={task.checked}
                aria-checked={task.checked}
                checked={currentState}
                onChange={() => {
                  setTasks &&
                    setTasks((prevSaved) => ({
                      ...prevSaved,
                      [currentID]: !prevSaved[currentID],
                    }));

                  const message = new ROSLIB.Message({
                    data: JSON.stringify({
                      id: currentID,
                      status: !currentState,
                      sender: "tablet",
                    }),
                  });
                  task_publisher?.publish(message);
                }}
              />
            }
            label={
              <div
                style={{
                  fontSize: "max(calc(1.6vw), 15px)",
                  fontWeight: "lighter",
                }}
              >
                {task.name}
              </div>
            }
            style={{ fontSize: 20 }}
          />
        )}
        {task.subTasks && (
          <div
            style={{
              paddingLeft: "0px",
              wordBreak: "break-word",
              alignItems: "flex-start",
              fontWeight: "bold",
              fontSize: "max(calc(1.8vw), 17px)",
            }}
          >
            <Checkbox
              sx={{
                "color": grey[900],
                "&.Mui-checked": { color: grey[900] },
              }}
              disabled={true}
              style={{ paddingLeft: "0px", opacity: 0.5 }}
            />
            {task.name} :
          </div>
        )}
        {task.subTasks &&
          RenderTasks(
            task.subTasks,
            level + 1,
            currentID,
            saved,
            setTasks
            // task_publisher,
            // saved
          )}
      </div>
    );
  });
}


function MainTimer() {
  //  let timerOn = 0;
  //  const timerPaused = 0; // Necessary variable? Unless it's value is changed at any point, it must be kept to const
  // let timeOver = 0;
  const [startTime, setStartTime] = useState<Date | null>(null);
  const [time, setTime] = useState(0);

  useEffect(() => {
    let interval: NodeJS.Timeout;

    if (startTime) {
      interval = setInterval(() => {
        const now = new Date();
        const elapsed = now.getTime() - startTime.getTime();
        setTime(elapsed);
      }, 60);
    }

    return () => {
      if (interval) {
        clearInterval(interval);
      }
    };
  }, [startTime]);

  const start = () => {
    if (startTime) return;
    setStartTime(new Date());
  };

  const pause = () => {
    setStartTime(null);
  };

  const reset = () => {
    setStartTime(null);
    setTime(0);
  };

  const minutes = Math.floor(time / 60000);
  const seconds = Math.floor((time % 60000) / 1000);
  const milliseconds = Math.floor((time % 1000) / 10);

  const displayedMinutes = String(minutes).padStart(2, "0");
  const displayedSeconds = String(seconds).padStart(2, "0");
  const displayedMilliseconds = String(milliseconds).padStart(2, "0");

  return (
    <AppBar sx={{ bgcolor: "grey", textAlign: "center" }}>
      {/* Main Timer: Displayed Time */}
      <Typography component="div" id="timerDisplay" fontSize={50}>
        {`${displayedMinutes}:${displayedSeconds}:${displayedMilliseconds}`}
      </Typography>

      {/* Main Timer: Start Button */}
      <Button
        variant="contained"
        sx={{ background: green[400], color: grey[50] }}
        onClick={() => start()}
      >
        <Typography fontSize={25}>START</Typography>
      </Button>

      {/* Main Timer: Pause Button */}
      <Button
        variant="contained"
        sx={{ background: yellow[400], color: [grey[50]] }}
        onClick={() => pause()}
      >
        <Typography fontSize={25}>PAUSE</Typography>
      </Button>

      {/* Main Timer: Reset Button */}
      <Button
        variant="contained"
        sx={{ background: red[400], color: grey[50] }}
        onClick={() => reset()}
      >
        <Typography fontSize={25}>RESET</Typography>
      </Button>
    </AppBar>
  );
}

function Timer() {
  const [startTime, setStartTime] = useState<Date | null>(null);
  const [time, setTime] = useState(0);

  useEffect(() => {
    let interval: NodeJS.Timeout;

    if (startTime) {
      interval = setInterval(() => {
        const now = new Date();
        const elapsed = now.getTime() - startTime.getTime();
        setTime(elapsed);
      }, 60);
    }

    return () => {
      if (interval) {
        clearInterval(interval);
      }
    };
  }, [startTime]);

  const start = () => {
    if (startTime) return;
    setStartTime(new Date());
  };

  const pause = () => {
    setStartTime(null);
  };

  const reset = () => {
    setStartTime(null);
    setTime(0);
  };

  const minutes = Math.floor(time / 60000);
  const seconds = Math.floor((time % 60000) / 1000);
  const milliseconds = Math.floor((time % 1000) / 10);

  const displayedMinutes = String(minutes).padStart(2, "0");
  const displayedSeconds = String(seconds).padStart(2, "0");
  const displayedMilliseconds = String(milliseconds).padStart(2, "0");

  return (
    <Box sx={{ width: [400], textAlign: "left" }}>
      <Stack spacing={2} direction="row">
        <Button
          variant="outlined"
          sx={{ borderColor: green[900], color: green[900] }}
          onClick={() => start()}
        >
          START
        </Button>

        <Button
          variant="outlined"
          sx={{ borderColor: yellow[900], color: yellow[900] }}
          onClick={() => pause()}
        >
          PAUSE
        </Button>

        <Box textAlign="center" sx={{ width: [80] }}>
          <Typography component="div" fontSize={20} sx={{ color: grey[400] }}>
            {`${displayedMinutes}:${displayedSeconds}:${displayedMilliseconds}`}
          </Typography>
        </Box>

        <Button
          variant="outlined"
          sx={{ borderColor: red[900], color: red[900] }}
          onClick={reset}
        >
          RESET
        </Button>
      </Stack>
    </Box>
  );
}