#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

using namespace std;

enum class ButtonAction { 
    NONE, SURGE_FORWARD, SURGE_BACKWARD, SWAY_LEFT, SWAY_RIGHT, YAW_LEFT, YAW_RIGHT, HEAVE_UP, HEAVE_DOWN, ROLL_CW, ROLL_CCW, C
    ONFIGURATION_MODE, FAST_MODE, INVERT_CONTROLS,
    BRIGHTEN_LED0, DIM_LED0, BRIGHTEN_LED1, DIM_LED1,
    TURN_SERVO0_CW, TURN_SERVO0_CCW, TURN_SERVO1_CW, TURN_SERVO1_CCW, TURN_SERVO2_CW, TURN_SERVO2_CCW, TURN_SERVO3_CW, TURN_SERVO3_CCW, 
    USE_SERVO0_ANGLE_PRESET_0, USE_SERVO0_ANGLE_PRESET_1, USE_SERVO0_ANGLE_PRESET_2, 
    USE_SERVO1_ANGLE_PRESET_0, USE_SERVO1_ANGLE_PRESET_1, USE_SERVO1_ANGLE_PRESET_2, 
    USE_SERVO2_ANGLE_PRESET_0, USE_SERVO2_ANGLE_PRESET_1, USE_SERVO2_ANGLE_PRESET_2, 
    USE_SERVO3_ANGLE_PRESET_0, USE_SERVO3_ANGLE_PRESET_1, USE_SERVO3_ANGLE_PRESET_2, 
    TURN_PCDCM0_CW, TURN_PCDCM0_CCW, TURN_PCDCM1_CW, TURN_PCDCM1_CCW, 
    USE_PCDCM0_ANGLE_PRESET_0, USE_PCDCM0_ANGLE_PRESET_1, USE_PCDCM0_ANGLE_PRESET_2, 
    USE_PCDCM1_ANGLE_PRESET_0, USE_PCDCM1_ANGLE_PRESET_1, USE_PCDCM1_ANGLE_PRESET_2, 
    TURN_DCM0_FORWARD, TURN_DCM0_REVERSE, TURN_DCM1_FORWARD, TURN_DCM1_REVERSE, TURN_DCM2_FORWARD, TURN_DCM2_REVERSE, TURN_DCM3_FORWARD, TURN_DCM3_REVERSE,
    SIZE};

const char* buttonActionLabels[] = {
    "None", "Surge Forward", "Surge Backward", "Sway Left", "Sway Right", "Yaw Left", "Yaw Right", "Heave Up", "Heave Down", "Roll CW", "Roll CCW", "Configuration Mode",
    "Fast Mode", "Invert Controls",
    "Brighten LED 0", "Dim LED 0", "Brighten LED 1", "Dim LED 1",
    "Turn Servo 0 CW", "Turn Servo 0 CCW", "Turn Servo 1 CW", "Turn Servo 1 CCW", "Turn Servo 2 CW", "Turn Servo 2 CCW", "Turn Servo 3 CW", "Turn Servo 3 CCW",
    "Use Servo 0 Angle Preset 0", "Use Servo 0 Angle Preset 1", "Use Servo 0 Angle Preset 2", 
    "Use Servo 1 Angle Preset 0", "Use Servo 1 Angle Preset 1", "Use Servo 1 Angle Preset 2", 
    "Use Servo 2 Angle Preset 0", "Use Servo 2 Angle Preset 1", "Use Servo 2 Angle Preset 2",
    "Use Servo 3 Angle Preset 0", "Use Servo 3 Angle Preset 1", "Use Servo 3 Angle Preset 2",
    "Turn PCDCM 0 CW", "Turn PCDCM 0 CCW", "Turn PCDCM 1 CW", "Turn PCDCM 1 CCW",
    "Use PCDCM 0 Angle Preset 0", "Use PCDCM 0 Angle Preset 1", "Use PCDCM 0 Angle Preset 2",
    "Use PCDCM 1 Angle Preset 0", "Use PCDCM 1 Angle Preset 1", "Use PCDCM 1 Angle Preset 2",
    "Turn DCM 0 Forward", "Turn DCM 0 Reverse", "Turn DCM 1 Forward", "Turn DCM 1 Reverse"
};

const char* buttonActionCodes[] = {
    "none", "surge_forward", "surge_backward", "sway_left", "sway_right", "yaw_left", "yaw_right", "heave_up", "heave_down", "roll_cw", "roll_ccw", "configuration_mode",
    "fast_mode", "invert_controls",
    "brighten_led0", "dim_led0", "brighten_led1", "dim_led1",
    "turn_servo0_cw", "turn_servo0_ccw", "turn_servo1_cw", "turn_servo1_ccw", "turn_servo2_cw", "turn_servo2_ccw", "turn_servo3_cw", "turn_servo3_ccw",
    "use_servo0_angle_preset_0", "use_servo0_angle_preset_1", "use_servo0_angle_preset_2",
    "use_servo1_angle_preset_0", "use_servo1_angle_preset_1", "use_servo1_angle_preset_2",
    "use_servo2_angle_preset_0", "use_servo2_angle_preset_1", "use_servo2_angle_preset_2",
    "use_servo3_angle_preset_0", "use_servo3_angle_preset_1", "use_servo3_angle_preset_2",
    "turn_pcdcm0_cw", "turn_pcdcm0_ccw", "turn_pcdcm1_cw", "turn_pcdcm1_ccw",
    "use_pcdcm0_angle_preset_0", "use_pcdcm0_angle_preset_1", "use_pcdcm0_angle_preset_2",
    "use_pcdcm1_angle_preset_0", "use_pcdcm1_angle_preset_1", "use_pcdcm1_angle_preset_2",
    "turn_dcm0_forward", "turn_dcm0_reverse", "turn_dcm1_forward", "turn_dcm1_reverse"
};

enum class AxisAction { NONE, SURGE, SWAY, YAW, ROLL, HEAVE, SIZE };
const char* axisActionLabels[] = { "None", "Surge", "Sway", "Yaw", "Roll" ,"Heave" };
const char* axisActionCodes[] = { "none", "surge", "sway", "yaw", "roll", "heave" };

ButtonAction stringToButtonAction(const string& action) {
    auto it = std::find_if(std::begin(buttonActionCodes), std::end(buttonActionCodes), [&](const char* code) { return strcmp(code, action.c_str()) == 0; });
    if (it != std::end(buttonActionCodes)) return static_cast<ButtonAction>(std::distance(std::begin(buttonActionCodes), it));
    return ButtonAction::NONE;
}

AxisAction stringToAxisAction(const string& action) {
    auto it = std::find_if(std::begin(axisActionCodes), std::end(axisActionCodes), [&](const char* code) { return strcmp(code, action.c_str()) == 0; });
    if (it != std::end(axisActionCodes)) return static_cast<AxisAction>(std::distance(std::begin(axisActionCodes), it));
    return AxisAction::NONE;
}

class UserConfig {
    public:
        char name[64];
        float deadzone = 0.1;
        vector<ButtonAction> buttonActions;
        vector<AxisAction> axisActions;
};

class BluestarConfig {
    public:
        /***************** Parameters used by backend *****************/
        std::array<char[64], 6> thruster_map = {"0", "1", "2", "3", "4", "5"};
        std::array<bool, 6> reverse_thrusters = {false, false, false, false, false};
        std::array<bool, 6> stronger_side_positive = {false, false, false, false, false};
        float thruster_stronger_side_attenuation_constant = 1.0f;

        /***************** Parameters used by frontend *****************/
        std::array<std::array<uint8_t, 3>, 4> preset_servo_angles = {{{0, 127, 255}, {0, 127, 255}, {0, 127, 255}, {0, 127, 255}}};
        std::array<std::array<uint8_t, 3>, 2> preset_precision_control_dc_motor_angles = {{{0, 127, 255}, {0, 127, 255}}};
        std::array<uint8_t, 4> dc_motor_speeds = {127, 127, 127, 127}; // Default speed for DC motors when controlled via buttons

        /***************** Parameters used by firmware *****************/
        uint8_t thruster_acceleration = 1;
        uint16_t thruster_timeout = 2500; // in milliseconds
        // Precision control DC motor mode will disable speed control for 2 out of the 4 DC motors and instead use position control
        // It is not set unless specified by the user or the config
        std::array<uint8_t, 2> set_precision_control_dc_motor_parameters = {{false, false}}; 
        std::array<uint8_t, 2> precision_control_dc_motor_numbers = {{0, 1}}; 
        std::array<uint8_t, 2> precision_control_dc_motor_control_loop_period = {{6, 6}}; // in ms
        std::array<float, 2> precision_control_dc_motor_proportional_gain = {{1, 1}}; 
        std::array<float, 2> precision_control_dc_motor_integral_gain = {{0, 0}};
        std::array<float, 2> precision_control_dc_motor_derivative_gain = {{0, 0}};
};