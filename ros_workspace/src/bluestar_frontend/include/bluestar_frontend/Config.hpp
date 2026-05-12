#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

using namespace std;

enum class ButtonAction { NONE, SURGE_FORWARD, SURGE_BACKWARD, SWAY_LEFT, SWAY_RIGHT, YAW_LEFT, YAW_RIGHT, HEAVE_UP, HEAVE_DOWN, 
    DC_MOTOR_1_CW, DC_MOTOR_1_CCW, DC_MOTOR_2_CW, DC_MOTOR_2_CCW, BRIGHTEN_LED_1, DIM_LED_1, BRIGHTEN_LED_2, DIM_LED_2,
    SERVO_1_CW, SERVO_1_CCW, SERVO_2_CW, SERVO_2_CCW, SERVO_3_CW, SERVO_3_CCW, SERVO_4_CW, SERVO_4_CCW, 
    FLIP_CAMERA_1_VERTICALLY, FLIP_CAMERA_2_VERTICALLY, FLIP_CAMERA_3_VERTICALLY, FLIP_CAMERA_4_VERTICALLY, 
    FLIP_CAMERA_1_HORIZONTALLY, FLIP_CAMERA_2_HORIZONTALLY, FLIP_CAMERA_3_HORIZONTALLY, FLIP_CAMERA_4_HORIZONTALLY, 
    USE_SERVO_ANGLE_PRESET_1, USE_SERVO_ANGLE_PRESET_2, USE_SERVO_ANGLE_PRESET_3, 
    ROLL_CW, ROLL_CCW, CONFIGURATION_MODE, FAST_MODE, INVERT_CONTROLS, SIZE};
const char* buttonActionLabels[] = { "None", "Surge Forward", "Surge Backward", "Sway Left", "Sway Right", "Yaw Left", "Yaw Right", "Heave Up", "Heave Down", 
    "DC Motor 1 CW", "DC Motor 1 CCW", "DC Motor 2 CW", "DC Motor 2 CCW", "Brighten LED 1", "Dim LED 1", "Brighten LED 2", "Dim LED 2", 
    "Servo 1 CW", "Servo 1 CCW", "Servo 2 CW", "Servo 2 CCW", "Servo 3 CW", "Servo 3 CCW", "Servo 4 CW", "Servo 4 CCW", 
    "Flip Camera 1 Vertically", "Flip Camera 2 Vertically", "Flip Camera 3 Vertically", "Flip Camera 4 Vertically", 
    "Flip Camera 1 Horizontally", "Flip Camera 2 Horizontally", "Flip Camera 3 Horizontally", "Flip Camera 4 Horizontally", 
    "Use Servo Angle Preset 1", "Use Servo Angle Preset 2", "Use Servo Angle Preset 3", 
    "Roll CW", "Roll CCW", "Configuration Mode", "Fast Mode", "Invert Controls"};
const char* buttonActionCodes[] = { "none", "surge_forward", "surge_backward", "sway_left", "sway_right", "yaw_left", "yaw_right", "heave_up", "heave_down", 
    "dc_motor_1_cw", "dc_motor_1_ccw", "dc_motor_2_cw", "dc_motor_2_ccw", "brighten_led_1", "dim_led_1", "brighten_led_2", "dim_led_2",
    "servo_1_cw", "servo_1_ccw", "servo_2_cw", "servo_2_ccw", "servo_3_cw", "servo_3_ccw", "servo_4_cw", "servo_4_ccw", 
    "flip_camera_1_vertically", "flip_camera_2_vertically", "flip_camera_3_vertically", "flip_camera_4_vertically",
    "flip_camera_1_horizontally", "flip_camera_2_horizontally", "flip_camera_3_horizontally", "flip_camera_4_horizontally", 
    "use_servo_angle_preset_1", "use_servo_angle_preset_2", "use_servo_angle_preset_3", 
    "roll_cw", "roll_ccw", "configuration_mode", "fast_mode", "invert_controls"};

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
        
        std::string controllerName;
        std::string controllerGuid;
        
        vector<ButtonAction> buttonActions;
        vector<AxisAction> axisActions;
};

class BlueStarConfig {
    public:
        std::array<char[64], 6> thruster_map = {"0", "1", "2", "3", "4", "5"};
        std::array<bool, 6> reverse_thrusters = {false, false, false, false, false, false};
        std::array<bool, 6> stronger_side_positive = {false, false, false, false, false, false};
        float thruster_stronger_side_attenuation_constant = 1.0f;
        float thruster_acceleration = 1.0f;
        std::array<int, 3> servo_1_preset_angles = {0, 127, 255};
        std::array<int, 3> servo_2_preset_angles = {0, 127, 255};
        char cam1ip[512];
        char cam2ip[512];
        char cam3ip[512]; 
        char cam4ip[512]; 

        char cam1_video_caps[1024];
        char cam1_audio_caps[1024];

        char cam2_video_caps[1024];
        char cam2_audio_caps[1024];

        char cam3_video_caps[1024];
        char cam3_audio_caps[1024];

        char cam4_video_caps[1024];
        char cam4_audio_caps[1024];
};