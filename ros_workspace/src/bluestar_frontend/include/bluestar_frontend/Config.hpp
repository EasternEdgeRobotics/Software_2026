#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

using namespace std;

enum class ButtonAction { NONE, SURGE_FORWARD, SURGE_BACKWARD, SWAY_LEFT, SWAY_RIGHT, YAW_LEFT, YAW_RIGHT, HEAVE_UP, HEAVE_DOWN, 
    DC_MOTOR_1_CW, DC_MOTOR_1_CCW, DC_MOTOR_2_CW, DC_MOTOR_2_CCW, BRIGHTEN_LED_1, DIM_LED_1, BRIGHTEN_LED_2, DIM_LED_2,
    TURN_FRONT_SERVO_CW, TURN_FRONT_SERVO_CCW, TURN_BACK_SERVO_CW, TURN_BACK_SERVO_CCW, CONFIGURATION_MODE, FLIP_CAMERA_1_VERTICALLY, FLIP_CAMERA_2_VERTICALLY, FLIP_CAMERA_3_VERTICALLY, FLIP_CAMERA_4_VERTICALLY, 
    FLIP_CAMERA_1_HORIZONTALLY, FLIP_CAMERA_2_HORIZONTALLY, FLIP_CAMERA_3_HORIZONTALLY, FLIP_CAMERA_4_HORIZONTALLY, USE_SERVO_ANGLE_PRESET_1, USE_SERVO_ANGLE_PRESET_2, USE_SERVO_ANGLE_PRESET_3, 
    ROLL_CW, ROLL_CCW, FAST_MODE, INVERT_CONTROLS, SIZE};
const char* buttonActionLabels[] = { "None", "Surge Forward", "Surge Backward", "Sway Left", "Sway Right", "Yaw Left", "Yaw Right", "Heave Up", "Heave Down", 
    "DC Motor 1 Clockwise", "DC Motor 1 Counter Clockwise", "DC Motor 2 Clockwise", "DC Motor 2 Counter Clockwise", "Brighten LED 1", "Dim LED 1", "Brighten LED 2", "Dim LED 2", 
    "Turn Front Servo CW", "Turn Front Servo CCW", "Turn Back Servo CW", "Turn Back Servo CCW", "Configuration Mode", "Flip Camera 1 Vertically", "Flip Camera 2 Vertically", "Flip Camera 3 Vertically", "Flip Camera 4 Vertically", 
    "Flip Camera 1 Horizontally", "Flip Camera 2 Horizontally", "Flip Camera 3 Horizontally", "Flip Camera 4 Horizontally", "Use Servo Angle Preset 1", "Use Servo Angle Preset 2", "Use Servo Angle Preset 3", 
    "Roll CW", "Roll CCW", "Fast Mode", "Invert Controls"};
const char* buttonActionCodes[] = { "none", "surge_forward", "surge_backward", "sway_left", "sway_right", "yaw_left", "yaw_right", "heave_up", "heave_down", 
    "dc_motor_1_cw", "dc_motor_1_ccw", "dc_motor_2_cw", "dc_motor_2_ccw", "brighten_led_1", "dim_led_1", "brighten_led_2", "dim_led_2",
    "turn_front_servo_cw", "turn_front_servo_ccw", "turn_back_servo_cw", "turn_back_servo_ccw", "configuration_mode", "flip_camera_1_vertically", "flip_camera_2_vertically", "flip_camera_3_vertically", "flip_camera_4_vertically",
    "flip_camera_1_horizontally", "flip_camera_2_horizontally", "flip_camera_3_horizontally", "flip_camera_4_horizontally", "use_servo_angle_preset_1", "use_servo_angle_preset_2", "use_servo_angle_preset_3", 
    "roll_cw", "roll_ccw", "fast_mode", "invert_controls"};

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
        char cam1ip[64];
        char cam2ip[64];
        char cam3ip[64]; 
        char cam4ip[64]; 
        char name[64];
        float deadzone = 0.1;
        vector<ButtonAction> buttonActions;
        vector<AxisAction> axisActions;
};

class BlueStarConfig {
    public:
        char servo1SSHTarget[64];
        char servo2SSHTarget[64];
        std::array<char[64], 6> thruster_map = {"0", "1", "2", "3", "4", "5"};
        std::array<bool, 6> reverse_thrusters = {false, false, false, false, false};
        std::array<bool, 6> stronger_side_positive = {false, false, false, false, false};
        float thruster_stronger_side_attenuation_constant = 1.0f;
        float thruster_acceleration = 1.0f;
        std::array<int, 3> front_camera_preset_servo_angles = {0, 90, 180};
        std::array<int, 3> back_camera_preset_servo_angles = {0, 90, 180};
};