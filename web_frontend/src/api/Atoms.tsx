import { atom } from "jotai";


// In the React Javascript Framework, normal Javascript variables still exist. However, a new type of variable is introduced which is called a state.
// When a state is changed, components using that state (the same way you'd use a normal variable) are updated.

// States are local to the scope of the function they are defined in. However, using the Atom states from the jotai library allows for the creating of global states
// Which can be accessed in multiple files


// Global State Definitions
export const CameraURLs = atom<string[]>(["", "", "", ""]);
export const ROSIP = atom<string>(localStorage.getItem("ROS_IP") || window.location.hostname);
export const IsROSConnected = atom<boolean>(false);

export const Mappings = atom<{ [controller: number]: { [type: string]: { [index: number]: string } } }>({ 0: {}, 1: {} }); // Current controller mappings
export const KeyboardMode = atom<boolean>(false);

export const ThrusterMultipliers = atom<number[]>([20, 0, 0, 0, 0,0 , 0]); // Power:0, Surge:1, Sway:2, Heave:3, Pitch:4, Roll:5, Yaw:6
export const ControllerInput = atom<(number | undefined)[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]); // Current controller input from pilot
export const PilotActions = atom<string[]>([ // Possible pilot inputs
  "None",
  "surge",
  "sway",
  "heave",
  "pitch",
  "roll",
  "yaw",
  "open_claw",
  "close_claw",
  "heave_up",
  "heave_down",
  "pitch_up",
  "pitch_down",
  "roll_cw",
  "roll_ccw",
  "turn_front_servo_cw",
  "turn_front_servo_ccw",
  "turn_back_servo_cw",
  "turn_back_servo_ccw",
  "brighten_led",
  "dim_led",
  "turn_stepper_cw",
  "turn_stepper_ccw",
  "read_outside_temperature_probe",
  "enter_auto_mode"
]);

export const KeyboardInputMap = atom<(string | number)[][]>([ // Possible pilot inputs
  ["w", "surge", 100],
  ["a", "sway", -100],
  ["s", "surge", -100],
  ["d", "sway", 100],
  ["q", "yaw", -100],
  ["e", "yaw", 100],
  ["r", "heave", 100],
  ["f", "heave", -100],
  ["t", "pitch", 100],
  ["g", "pitch", -100],
  ["y", "roll", 100],
  ["h", "roll", -100],
  ["z", "turn_front_servo_cw", 1],
  ["x", "turn_front_servo_ccw", 1],
  ["c", "brighten_led", 1],
  ["v", "dim_led", 1],
  ["b", "turn_back_servo_cw", 1],
  ["n", "turn_back_servo_ccw", 1],
  ["m", "read_outside_temperature_probe", 1],
  ["o", "open_claw", 1],
  ["p", "close_claw", 1],
  [",", "enter_auto_mode", 1]
])

export const CurrentProfile = atom<string>("Not Assigned"); // Current pilot profile
export const ProfilesList = atom<{ id: number, name: string, controller1: string, controller2: string }[]>([{ id: 0, name: "default", controller1: "null", controller2: "null" }]); // List of known pilot profiles

export const RequestingConfig = atom<{ state: number, name: string, controller1: string, controller2: string }>({ state: 2, name: "default", controller1: "null", controller2: "null" }); // Used for requesting mappings from OR loading mappings into the Database
export const RequestingProfilesList = atom<number>(2); // Used for deleting a certain profile in the database or requesting a list of profiles.
// See the ROS.tsx script for how the above three Atom states are used

// Shows up only on the BotTab
export const DiagnosticsData1 = atom<string>("No diagnostics data available");

// Shows up both on the BotTab and the CameraTab
export const DiagnosticsData2 = atom<string>("No diagnostics data available"); 