#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <nlohmann/json.hpp>
#include "Config.hpp"
#include "ROS.hpp"
#include <iostream>
#include <cstdint>
#include <memory>

using json = nlohmann::json;

UserConfig user_config;
BluestarConfig bluestar_config;
Power power;
Power previous_power_settings;
Power fast_mode_settings;

bool showConfigWindow = false;
bool showPilotWindow = false;

vector<string> names;
vector<string> configs;


uint8_t configuration_mode_thruster_number = 0;
bool configuration_mode = false;

bool keyboard_mode = false;
bool fast_mode = false;
bool invert_controls = false;

bool set_thruster_acceleration = false;
uint8_t thruster_acceleration = 1;
bool set_thruster_timeout = false;
uint16_t thruster_timeout = 2500; // in milliseconds

std::array<uint8_t, 4> preset_dc_motor_speeds = {127, 127, 127, 127}; 
std::array<uint8_t, 4> commanded_dc_motor_speeds = {127, 127, 127, 127}; 

std::array<bool, 2> set_precision_control_dc_motor_parameters = {false, false};
std::array<uint8_t, 2> precision_control_associated_dc_motor_numbers = {0, 1};
std::array<uint8_t, 2> precision_control_loop_period = {6, 6}; // in milliseconds
std::array<float, 2> precision_control_proportional_gain = {1.0f, 1.0f};
std::array<float, 2> precision_control_integral_gain = {0.0f, 0.0f};
std::array<float, 2> precision_control_derivative_gain = {0.0f, 0.0f};

bool brighten_led_latch[2] = {false, false};
bool dim_led_latch[2] = {false, false};
bool turn_servo_ccw_latch[4] = {false, false, false, false};
bool turn_servo_cw_latch[4] = {false, false, false, false};
bool turn_pcdcm_ccw_latch[2] = {false, false};
bool turn_pcdcm_cw_latch[2] = {false, false};
bool turn_dcm_reverse_latch[4] = {false, false, false, false};

bool fast_mode_latch = false;
bool invert_controls_latch = false;

// Predeclare function
void saveBluestarConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const BluestarConfig& bluestar_config);

int main(int argc, char **argv) {
    //initialize glfw, imgui, and rclcpp (ros)    
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge Bluestar GUI", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    rclcpp::init(argc, argv);
    
    //create ros nodes
    auto saveConfigNode = std::make_shared<SaveConfigPublisher>();
    auto pilotInputNode = std::make_shared<PilotInputPublisher>();

    //get configs from ros, then set the names and configs
    std::array<std::vector<std::string>, 2> configRes = getConfigs();
    names = configRes[0];
    configs = configRes[1];

    // Set the default fast mode settings
    fast_mode_settings.power = 75;
    fast_mode_settings.surge = 75;
    fast_mode_settings.sway = 50;
    fast_mode_settings.heave = 75;
    fast_mode_settings.roll = 0;
    fast_mode_settings.yaw = 30;

    // Load the bluestar config
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == "bluestar_config")
        {
            json configJSON = json::parse(configs[i]);
            if (!configJSON["thruster_stronger_side_attenuation_constant"].is_null()) bluestar_config.thruster_stronger_side_attenuation_constant = configJSON["thruster_stronger_side_attenuation_constant"].get<float>();
            for (size_t i = 0; i < std::size(bluestar_config.thruster_map); i++){
                if (!configJSON["thruster_map"][i].is_null()) std::strncpy(bluestar_config.thruster_map[i], std::to_string(configJSON["thruster_map"][i].get<int>()).c_str(), sizeof(bluestar_config.thruster_map[i]));
                if (!configJSON["reverse_thrusters"][i].is_null()) bluestar_config.reverse_thrusters[i] = configJSON["reverse_thrusters"][i].get<bool>();
                if (!configJSON["stronger_side_positive"][i].is_null()) bluestar_config.stronger_side_positive[i] = configJSON["stronger_side_positive"][i].get<bool>();
            }

            for (size_t i = 0; i < std::size(bluestar_config.preset_servo_angles); i++){
                for (size_t j = 0; j < std::size(bluestar_config.preset_servo_angles[i]); j++){
                    if (!configJSON["preset_servo_angles"][i][j].is_null()) bluestar_config.preset_servo_angles[i][j] = configJSON["preset_servo_angles"][i][j].get<uint8_t>();
                }
            }
            for (size_t i = 0; i < std::size(bluestar_config.preset_precision_control_dc_motor_angles); i++){
                for (size_t j = 0; j < std::size(bluestar_config.preset_precision_control_dc_motor_angles[i]); j++){
                    if (!configJSON["preset_precision_control_dc_motor_angles"][i][j].is_null()) bluestar_config.preset_precision_control_dc_motor_angles[i][j] = configJSON["preset_precision_control_dc_motor_angles"][i][j].get<uint8_t>();
                }
            }
            for (size_t i = 0; i < std::size(bluestar_config.dc_motor_speeds); i++){
                if (!configJSON["dc_motor_speeds"][i].is_null()) bluestar_config.dc_motor_speeds[i] = configJSON["dc_motor_speeds"][i].get<uint8_t>();
                preset_dc_motor_speeds[i] = bluestar_config.dc_motor_speeds[i];
            }

            if (!configJSON["thruster_acceleration"].is_null()) {
                bluestar_config.thruster_acceleration = configJSON["thruster_acceleration"].get<uint8_t>();
                set_thruster_acceleration = true;
                thruster_acceleration = bluestar_config.thruster_acceleration;
            }
            if (!configJSON["thruster_timeout"].is_null()) {
                bluestar_config.thruster_timeout = configJSON["thruster_timeout"].get<uint16_t>();
                set_thruster_timeout = true;
                thruster_timeout = bluestar_config.thruster_timeout;
            }
            for (size_t i = 0; i < std::size(bluestar_config.set_precision_control_dc_motor_parameters); i++){
                if (!configJSON["precision_control_dc_motor_numbers"][i].is_null()) bluestar_config.precision_control_dc_motor_numbers[i] = configJSON["precision_control_dc_motor_numbers"][i].get<uint8_t>();
                if (!configJSON["precision_control_dc_motor_control_loop_period"][i].is_null()) bluestar_config.precision_control_dc_motor_control_loop_period[i] = configJSON["precision_control_dc_motor_control_loop_period"][i].get<uint8_t>();
                if (!configJSON["precision_control_dc_motor_proportional_gain"][i].is_null()) bluestar_config.precision_control_dc_motor_proportional_gain[i] = configJSON["precision_control_dc_motor_proportional_gain"][i].get<float>();
                if (!configJSON["precision_control_dc_motor_integral_gain"][i].is_null()) bluestar_config.precision_control_dc_motor_integral_gain[i] = configJSON["precision_control_dc_motor_integral_gain"][i].get<float>();
                if (!configJSON["precision_control_dc_motor_derivative_gain"][i].is_null()) bluestar_config.precision_control_dc_motor_derivative_gain[i] = configJSON["precision_control_dc_motor_derivative_gain"][i].get<float>();
                set_precision_control_dc_motor_parameters[i] = true;
            }   
            break;
        }
    }

    //render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int surge = 0;
        int sway = 0;
        int heave = 0;
        int roll = 0;
        int yaw = 0;
        std::array<bool, 2> brighten_led = {false, false};
        std::array<bool, 2> dim_led = {false, false};
        std::array<bool, 4> turn_servo_ccw = {false, false, false, false};
        std::array<bool, 4> turn_servo_cw = {false, false, false, false};
        std::array<bool, 2> turn_pcdcm_ccw = {false, false};
        std::array<bool, 2> turn_pcdcm_cw = {false, false};
        std::array<bool, 4> turn_dcm_reverse = {false, false, false, false};
        bool fast_mode_toggle = false;
        bool invert_controls_toggle = false;
        std::array<bool, 4> set_servo_angle = {false, false, false, false};
        std::array<bool, 2> set_pcdcm_angle = {false, false};
        std::array<uint8_t, 4> servo_angle = {127, 127, 127, 127};
        std::array<uint8_t, 2> pcdcm_angle = {127, 127};

        //control loop
        if (glfwJoystickPresent(GLFW_JOYSTICK_1) || keyboard_mode)
        {
            if (keyboard_mode) {
                // Keyboard input handling
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) surge += 100;
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) surge -= 100;
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) sway -= 100;
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) sway += 100;
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) yaw -= 100;
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) yaw += 100;
                if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) roll += 100;
                if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) roll -= 100;
                if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) heave += 100;
                if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) heave -= 100;
                if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) brighten_led = {true, true};
                if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) dim_led = {true, true};
                if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) turn_servo_cw = {true, true, true, true};
                if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) turn_servo_ccw = {true, true, true, true};
                if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) turn_pcdcm_cw = {true, true};
                if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) turn_pcdcm_ccw = {true, true};
                if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) fast_mode_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) invert_controls_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
                    commanded_dc_motor_speeds = preset_dc_motor_speeds;
                    turn_dcm_reverse = {false, false, false, false};
                }
                if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
                    commanded_dc_motor_speeds = preset_dc_motor_speeds;
                    turn_dcm_reverse = {true, true, true, true};
                }
                if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
                    set_servo_angle = {true, true, true, true};
                    servo_angle = {bluestar_config.preset_servo_angles[0][0], bluestar_config.preset_servo_angles[1][0], bluestar_config.preset_servo_angles[2][0], bluestar_config.preset_servo_angles[3][0]}; 
                }
                if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
                    set_servo_angle = {true, true, true, true};
                    servo_angle = {bluestar_config.preset_servo_angles[0][1], bluestar_config.preset_servo_angles[1][1], bluestar_config.preset_servo_angles[2][1], bluestar_config.preset_servo_angles[3][1]}; 
                }
                if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
                    set_servo_angle = {true, true, true, true};
                    servo_angle = {bluestar_config.preset_servo_angles[0][2], bluestar_config.preset_servo_angles[1][2], bluestar_config.preset_servo_angles[2][2], bluestar_config.preset_servo_angles[3][2]}; 
                }
                if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
                    set_pcdcm_angle = {true, true};
                    pcdcm_angle = {bluestar_config.preset_precision_control_dc_motor_angles[0][0], bluestar_config.preset_precision_control_dc_motor_angles[1][0]}; 
                }
                if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
                    set_pcdcm_angle = {true, true};
                    pcdcm_angle = {bluestar_config.preset_precision_control_dc_motor_angles[0][1], bluestar_config.preset_precision_control_dc_motor_angles[1][1]}; 
                }
                if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
                    set_pcdcm_angle = {true, true};
                    pcdcm_angle = {bluestar_config.preset_precision_control_dc_motor_angles[0][2], bluestar_config.preset_precision_control_dc_motor_angles[1][2]}; 
                }
            }
            if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
                int buttonCount, axisCount;
                const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);
                const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axisCount);
                if (buttonCount > 0 && user_config.buttonActions.empty()) user_config.buttonActions = vector<ButtonAction>(buttonCount, ButtonAction::NONE);
                if (axisCount > 0 && user_config.axisActions.empty()) user_config.axisActions = vector<AxisAction>(axisCount, AxisAction::NONE);
                for (int i = 0; i < buttonCount; i++) {
                    if (!buttons[i]) continue;
                    switch (user_config.buttonActions[i]) { 
                        case ButtonAction::SURGE_BACKWARD:
                            surge -= 100;
                            break;
                        case ButtonAction::SURGE_FORWARD:
                            surge += 100;
                            break;
                        case ButtonAction::SWAY_LEFT:
                            sway -= 100;
                            break;
                        case ButtonAction::SWAY_RIGHT:
                            sway += 100;
                            break;
                        case ButtonAction::HEAVE_DOWN:
                            heave -= 100;
                            break;
                        case ButtonAction::HEAVE_UP:
                            heave += 100;
                            break;
                        case ButtonAction::YAW_LEFT:
                            yaw -= 100;
                            break;
                        case ButtonAction::YAW_RIGHT:
                            yaw += 100;
                            break;
                        case ButtonAction::ROLL_CW:
                            roll += 100;
                            break;
                        case ButtonAction::ROLL_CCW:
                            roll -= 100;
                            break;

                        case ButtonAction::CONFIGURATION_MODE:
                            // Can either be set by user input for through the GUI
                            configuration_mode = !configuration_mode;
                            break;
                        case ButtonAction::FAST_MODE:
                            fast_mode_toggle = true;
                            break;  
                        case ButtonAction::INVERT_CONTROLS:
                            invert_controls_toggle = true;
                            break;

                        case ButtonAction::BRIGHTEN_LED0:
                            brighten_led[0] = true;
                            break;
                        case ButtonAction::DIM_LED0:
                            dim_led[0] = true;
                            break;
                        case ButtonAction::BRIGHTEN_LED1:
                            brighten_led[1] = true;
                            break;
                        case ButtonAction::DIM_LED1:
                            dim_led[1] = true;
                            break;

                        case ButtonAction::TURN_SERVO0_CW:
                            turn_servo_cw[0] = true;
                            break;  
                        case ButtonAction::TURN_SERVO0_CCW:
                            turn_servo_ccw[0] = true;
                            break;
                        case ButtonAction::TURN_SERVO1_CW:
                            turn_servo_cw[1] = true;
                            break;
                        case ButtonAction::TURN_SERVO1_CCW:
                            turn_servo_ccw[1] = true;
                            break;
                        case ButtonAction::TURN_SERVO2_CW:
                            turn_servo_cw[2] = true;
                            break;
                        case ButtonAction::TURN_SERVO2_CCW:
                            turn_servo_ccw[2] = true;
                            break;
                        case ButtonAction::TURN_SERVO3_CW:
                            turn_servo_cw[3] = true;
                            break;
                        case ButtonAction::TURN_SERVO3_CCW:
                            turn_servo_ccw[3] = true;
                            break;

                        case ButtonAction::USE_SERVO0_ANGLE_PRESET_0:
                            set_servo_angle[0] = true;
                            servo_angle[0] = bluestar_config.preset_servo_angles[0][0];
                            break;
                        case ButtonAction::USE_SERVO0_ANGLE_PRESET_1:
                            set_servo_angle[0] = true;
                            servo_angle[0] = bluestar_config.preset_servo_angles[0][1];
                            break;
                        case ButtonAction::USE_SERVO0_ANGLE_PRESET_2:
                            set_servo_angle[0] = true;
                            servo_angle[0] = bluestar_config.preset_servo_angles[0][2];
                            break;
                        case ButtonAction::USE_SERVO1_ANGLE_PRESET_0:
                            set_servo_angle[1] = true;
                            servo_angle[1] = bluestar_config.preset_servo_angles[1][0];
                            break;
                        case ButtonAction::USE_SERVO1_ANGLE_PRESET_1:
                            set_servo_angle[1] = true;
                            servo_angle[1] = bluestar_config.preset_servo_angles[1][1];
                            break;
                        case ButtonAction::USE_SERVO1_ANGLE_PRESET_2:
                            set_servo_angle[1] = true;
                            servo_angle[1] = bluestar_config.preset_servo_angles[1][2];
                            break;
                        case ButtonAction::USE_SERVO2_ANGLE_PRESET_0:
                            set_servo_angle[2] = true;
                            servo_angle[2] = bluestar_config.preset_servo_angles[2][0];
                            break;
                        case ButtonAction::USE_SERVO2_ANGLE_PRESET_1:
                            set_servo_angle[2] = true;
                            servo_angle[2] = bluestar_config.preset_servo_angles[2][1];
                            break;
                        case ButtonAction::USE_SERVO2_ANGLE_PRESET_2:
                            set_servo_angle[2] = true;
                            servo_angle[2] = bluestar_config.preset_servo_angles[2][2];
                            break;
                        case ButtonAction::USE_SERVO3_ANGLE_PRESET_0:
                            set_servo_angle[3] = true;
                            servo_angle[3] = bluestar_config.preset_servo_angles[3][0];
                            break;
                        case ButtonAction::USE_SERVO3_ANGLE_PRESET_1:
                            set_servo_angle[3] = true;
                            servo_angle[3] = bluestar_config.preset_servo_angles[3][1];
                            break;
                        case ButtonAction::USE_SERVO3_ANGLE_PRESET_2:
                            set_servo_angle[3] = true;
                            servo_angle[3] = bluestar_config.preset_servo_angles[3][2];
                            break;
                        
                        case ButtonAction::TURN_PCDCM0_CW:
                            turn_pcdcm_cw[0] = true;
                            break;
                        case ButtonAction::TURN_PCDCM0_CCW:
                            turn_pcdcm_ccw[0] = true;
                            break;
                        case ButtonAction::TURN_PCDCM1_CW:
                            turn_pcdcm_cw[1] = true;
                            break;
                        case ButtonAction::TURN_PCDCM1_CCW:
                            turn_pcdcm_ccw[1] = true;
                            set_pcdcm_angle[0] = true;
                            pcdcm_angle[0] = bluestar_config.preset_precision_control_dc_motor_angles[0][0];
                            break;
                        case ButtonAction::USE_PCDCM0_ANGLE_PRESET_1:
                            set_pcdcm_angle[0] = true;
                            pcdcm_angle[0] = bluestar_config.preset_precision_control_dc_motor_angles[0][1];
                            break;
                        case ButtonAction::USE_PCDCM0_ANGLE_PRESET_2:
                            set_pcdcm_angle[0] = true;
                            pcdcm_angle[0] = bluestar_config.preset_precision_control_dc_motor_angles[0][2];
                            break;
                        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_0:
                            set_pcdcm_angle[1] = true;
                            pcdcm_angle[1] = bluestar_config.preset_precision_control_dc_motor_angles[1][0];
                            break;
                        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_1:
                            set_pcdcm_angle[1] = true;
                            pcdcm_angle[1] = bluestar_config.preset_precision_control_dc_motor_angles[1][1];
                            break;
                        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_2:
                            set_pcdcm_angle[1] = true;
                            pcdcm_angle[1] = bluestar_config.preset_precision_control_dc_motor_angles[1][2];
                            break;

                        case ButtonAction::TURN_DCM0_FORWARD:
                            commanded_dc_motor_speeds[0] = preset_dc_motor_speeds[0];
                            turn_dcm_reverse[0] = false;
                            break;
                        case ButtonAction::TURN_DCM0_REVERSE:
                            commanded_dc_motor_speeds[0] = preset_dc_motor_speeds[0];
                            turn_dcm_reverse[0] = true;
                            break;
                        case ButtonAction::TURN_DCM1_FORWARD:
                            commanded_dc_motor_speeds[1] = preset_dc_motor_speeds[1];
                            turn_dcm_reverse[1] = false;
                            break;
                        case ButtonAction::TURN_DCM1_REVERSE:
                            commanded_dc_motor_speeds[1] = preset_dc_motor_speeds[1];
                            turn_dcm_reverse[1] = true;  
                            break;
                        case ButtonAction::TURN_DCM2_FORWARD:
                            commanded_dc_motor_speeds[2] = preset_dc_motor_speeds[2];
                            turn_dcm_reverse[2] = false;
                            break;
                        case ButtonAction::TURN_DCM2_REVERSE:
                            commanded_dc_motor_speeds[2] = preset_dc_motor_speeds[2];
                            turn_dcm_reverse[2] = true;
                            break;
                        case ButtonAction::TURN_DCM3_FORWARD:
                            commanded_dc_motor_speeds[3] = preset_dc_motor_speeds[3];
                            turn_dcm_reverse[3] = false;
                            break;
                        case ButtonAction::TURN_DCM3_REVERSE:
                            commanded_dc_motor_speeds[3] = preset_dc_motor_speeds[3];
                            turn_dcm_reverse[3] = true;
                            break;

                        default:
                            break;
                    }
                }
                


                for (size_t i = 0; i < user_config.axisActions.size(); i++) {
                    if (std::abs(axes[i]) <= user_config.deadzone) continue;
                    switch (user_config.axisActions[i]) {
                        case AxisAction::SURGE:
                            if (surge == 0) surge = -(int)(axes[i]*100);
                            break;
                        case AxisAction::SWAY:
                            if (sway == 0) sway = -(int)(axes[i]*100);
                            break;
                        case AxisAction::YAW:
                            if (yaw == 0) yaw = -(int)(axes[i]*100);
                            break;
                        case AxisAction::ROLL:
                            if (roll == 0) roll = -(int)(axes[i]*100);
                            break;
                        case AxisAction::HEAVE:
                            if (heave == 0) heave = -(int)(axes[i]*100);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (invert_controls_toggle) {
                if (!invert_controls_latch) invert_controls = !invert_controls;
                invert_controls_latch = true;
            } else {
                invert_controls_latch = false;
            }
            if (fast_mode_toggle) {
                if (!fast_mode_latch) {
                    fast_mode = !fast_mode;
                    if (fast_mode) {
                        previous_power_settings = power;
                        power = fast_mode_settings;
                    }
                    else {
                        power = previous_power_settings;
                    }
                }
                fast_mode_latch = true;
            } else {
                fast_mode_latch = false;
            }

            for (int i = 0; i < 4; i++) {
                if (turn_servo_ccw[i]) {
                    if (turn_servo_ccw_latch[i]) turn_servo_ccw[i] = false;
                    else turn_servo_ccw_latch[i] = true; 
                } else {
                    turn_servo_ccw_latch[i] = false;
                }
                if (turn_servo_cw[i]) {
                    if (turn_servo_cw_latch[i]) turn_servo_cw[i] = false;
                    else turn_servo_cw_latch[i] = true; 
                } else {
                    turn_servo_cw_latch[i] = false;
                }
                if (turn_dcm_reverse[i]) {
                    if (turn_dcm_reverse_latch[i]) turn_dcm_reverse[i] = false;
                    else turn_dcm_reverse_latch[i] = true; 
                } else {
                    turn_dcm_reverse_latch[i] = false;
                }
            }

            for (int i = 0; i < 2; i++) {
                if (turn_pcdcm_ccw[i]) {
                    if (turn_pcdcm_ccw_latch[i]) turn_pcdcm_ccw[i] = false;
                    else turn_pcdcm_ccw_latch[i] = true; 
                } else {
                    turn_pcdcm_ccw_latch[i] = false;
                }
                if (turn_pcdcm_cw[i]) {
                    if (turn_pcdcm_cw_latch[i]) turn_pcdcm_cw[i] = false;
                    else turn_pcdcm_cw_latch[i] = true; 
                } else {
                    turn_pcdcm_cw_latch[i] = false;
                }
            }
        }

        if (invert_controls)
        {
            surge = -surge;
            yaw = -yaw;
        }

        pilotInputNode->sendInput(power, surge, sway, heave, yaw, roll, configuration_mode, configuration_mode_thruster_number,
                                    set_thruster_acceleration, thruster_acceleration, 
                                    set_thruster_timeout, thruster_timeout,
                                    commanded_dc_motor_speeds, turn_dcm_reverse,
                                    brighten_led, dim_led,
                                    set_servo_angle, servo_angle,
                                    turn_servo_ccw, turn_servo_cw,
                                    set_precision_control_dc_motor_parameters, precision_control_associated_dc_motor_numbers,
                                    precision_control_loop_period, precision_control_proportional_gain,
                                    precision_control_integral_gain, precision_control_derivative_gain,
                                    set_pcdcm_angle, pcdcm_angle,
                                    turn_pcdcm_ccw, turn_pcdcm_cw
                                );
        set_thruster_acceleration = false;
        set_thruster_timeout = false;
        set_servo_angle = {false, false, false, false};
        set_pcdcm_angle = {false, false};
        commanded_dc_motor_speeds = {0, 0, 0, 0};
        set_precision_control_dc_motor_parameters = {false, false};

        //top menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Config")) {
                if (ImGui::MenuItem("Open Config Editor")) {
                    showConfigWindow = true;
                }
                if (ImGui::BeginMenu("Load UserConfig")) {
                    for (size_t i = 0; i < names.size(); i++) {
                        if (names[i] == "bluestar_config")
                        {
                            continue;
                        }
                        if (ImGui::MenuItem(names[i].c_str())) {
                            json configJSON = json::parse(configs[i]);
                            user_config.deadzone = configJSON.value("deadzone", 0.1f);
                            user_config.buttonActions.clear();
                            for (auto& mapping : configJSON["mappings"]["0"]["buttons"].items()) {
                                user_config.buttonActions.push_back(stringToButtonAction(mapping.value()));
                            }
                            user_config.axisActions.clear();
                            for (auto& mapping : configJSON["mappings"]["0"]["axes"].items()) {
                                user_config.axisActions.push_back(stringToAxisAction(mapping.value()));
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();                
            }
            if (ImGui::BeginMenu("Pilot")) {
                if (ImGui::MenuItem("Open Piloting Menu")) {
                    showPilotWindow = true;
                }
                ImGui::EndMenu();
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, fast_mode ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
            ImGui::Text(fast_mode ? " FAST MODE ON" : "FAST MODE OFF");
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, invert_controls ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
            ImGui::Text(invert_controls ?  "INVERT CONTROLS ON" : "INVERT CONTROLS OFF");
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::Checkbox("Keyboard Mode", &keyboard_mode);

            //fps counter
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }

        //user_config window
        if (showConfigWindow) {
            ImGui::Begin("Config Editor", &showConfigWindow);
            if (ImGui::BeginTabBar("Config Tabs")) {
                if (ImGui::BeginTabItem("Bluestar Config")){
                    if (!(keyboard_mode || glfwJoystickPresent(GLFW_JOYSTICK_1)))
                    {
                        ImGui::Text("Connect controller or enable keyboard mode to configure Bluestar");
                    }
                    else
                    {
                        bool configuration_mode_checkbox = configuration_mode;
                        ImGui::Checkbox("Thruster Configuration Mode (Toggle to Save)", &configuration_mode_checkbox);
                        if (!configuration_mode_checkbox && configuration_mode)
                        {
                            // The backend uses the negative edge of configuration_mode to load the new config
                            // Thus, on the negative edge of configuration_mode, we should save the new global config
                            saveBluestarConfig(saveConfigNode, bluestar_config);
                        }
                        if (configuration_mode_checkbox) { 
                            ImGui::Text("For Star (Forward Right)");
                            ImGui::SameLine();
                            ImGui::InputText("##for_star", bluestar_config.thruster_map[0], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##for_star_rev_thruster", &bluestar_config.reverse_thrusters[0]);
                            ImGui::SameLine();

                            ImGui::Checkbox("Stronger Side Positive##for_star_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[0]);
        
                            ImGui::Text("For Port (Forward Left)");
                            ImGui::SameLine();
                            ImGui::InputText("##for_port", bluestar_config.thruster_map[1], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##for_port_rev_thruster", &bluestar_config.reverse_thrusters[1]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##for_port_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[1]);
        
                            ImGui::Text("Aft star (Back Right)");
                            ImGui::SameLine();
                            ImGui::InputText("##aft_star", bluestar_config.thruster_map[2], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##aft_star_rev_thruster", &bluestar_config.reverse_thrusters[2]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##aft_star_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[2]);
        
                            ImGui::Text("Aft Port (Back Left)");
                            ImGui::SameLine();
                            ImGui::InputText("##aft_port", bluestar_config.thruster_map[3], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##aft_port_rev_thruster", &bluestar_config.reverse_thrusters[3]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##aft_port_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[3]);
        
                            ImGui::Text("Star Top (Right Top)");
                            ImGui::SameLine();
                            ImGui::InputText("##star_top", bluestar_config.thruster_map[4], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##star_top_rev_thruster", &bluestar_config.reverse_thrusters[4]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##star_top_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[4]);
        
                            ImGui::Text("Port Top (Left Top)");
                            ImGui::SameLine();
                            ImGui::InputText("##Port_top", bluestar_config.thruster_map[5], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##port_top_rev_thruster", &bluestar_config.reverse_thrusters[5]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##port_top_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[5]);
                            
                            ImGui::Text("The thruster acceleration determines how fast thrusters ramp up to the commanded speed");

                            ImGui::Text("Thruster Acceleration");
                            ImGui::SameLine();
                            int thruster_acceleration_temp = static_cast<int>(bluestar_config.thruster_acceleration);
                            ImGui::SliderInt("##thruster_acceleration", &thruster_acceleration_temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
                            bluestar_config.thruster_acceleration = static_cast<uint8_t>(thruster_acceleration_temp);
                            if (ImGui::Button("Set Thruster Acceleration")) {
                                set_thruster_acceleration = true;
                                thruster_acceleration = static_cast<uint8_t>(bluestar_config.thruster_acceleration);
                            }

                            ImGui::Text("Set Thruster Timeout (ms)");
                            ImGui::SameLine();
                            int thruster_timeout_temp = static_cast<int>(bluestar_config.thruster_timeout);
                            ImGui::SliderInt("##thruster_timeout", &thruster_timeout_temp, 0, 65535, "%d", ImGuiSliderFlags_AlwaysClamp);
                            bluestar_config.thruster_timeout = static_cast<uint16_t>(thruster_timeout_temp);
                            if (ImGui::Button("Set Thruster Timeout")) {
                                set_thruster_timeout = true;
                                thruster_timeout = static_cast<uint16_t>(bluestar_config.thruster_timeout);
                            }
                            
                            ImGui::Text("The stronger side attentuation constant attenuates the power of the thruster on a given side");

                            ImGui::Text("Thruster Stronger Side Attenuation Constant");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##thruster_stronger_side_attenuation_constant", &bluestar_config.thruster_stronger_side_attenuation_constant, 0.0f, 2.0f, "%.005f", ImGuiSliderFlags_AlwaysClamp);

                            const char* thrusterNumbers[] = { "0", "1", "2", "3", "4", "5" };
                            static int currentThrusterNumber = 0;
                            ImGui::Text("Configuration Mode Thruster Number");
                            ImGui::SameLine();
                            if (ImGui::Combo("##thruster_number", &currentThrusterNumber, thrusterNumbers, IM_ARRAYSIZE(thrusterNumbers))) {
                                configuration_mode_thruster_number = currentThrusterNumber;
                            }
                            
                            if (ImGui::TreeNode("Modify Preset Servo and PCDCM Angles")) {
                                for (size_t i = 0; i < std::size(bluestar_config.preset_servo_angles); ++i) {
                                    for (size_t j = 0; j < std::size(bluestar_config.preset_servo_angles[i]); ++j) {
                                        ImGui::Text("Servo %ld Preset Angle %ld", i + 1, j + 1);
                                        ImGui::SameLine();
                                        ImGui::SliderInt(
                                            (std::string("##servo_") + std::to_string(i + 1) + std::string("_preset_angle_") + std::to_string(j + 1)).c_str(),
                                            &bluestar_config.preset_servo_angles[i][j],
                                            0, 255, "%d", ImGuiSliderFlags_AlwaysClamp
                                        );
                                    }
                                }
                                for (size_t i = 0; i < std::size(bluestar_config.preset_precision_control_dc_motor_angles); ++i) {
                                    for (size_t j = 0; j < std::size(bluestar_config.preset_precision_control_dc_motor_angles[i]); ++j) {
                                        ImGui::Text("PCDCM %ld Preset Angle %ld", i + 1, j + 1);
                                        ImGui::SameLine();
                                        ImGui::SliderInt(
                                            (std::string("##pcdcm_") + std::to_string(i + 1) + std::string("_preset_angle_") + std::to_string(j + 1)).c_str(),
                                            &bluestar_config.preset_precision_control_dc_motor_angles[i][j],
                                            0, 255, "%d", ImGuiSliderFlags_AlwaysClamp
                                        );
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }

                        if (ImGui::TreeNode("Modify Preset DC Motor Speeds")) {
                            for (size_t i = 0; i < preset_dc_motor_speeds.size(); ++i) {
                                ImGui::Text("DC Motor %ld Preset Speed", i + 1);
                                ImGui::SameLine();
                                ImGui::SliderInt(
                                    (std::string("##dcm_") + std::to_string(i + 1) + std::string("_preset_speed")).c_str(),
                                    &preset_dc_motor_speeds[i],
                                    0, 255, "%d", ImGuiSliderFlags_AlwaysClamp
                                );
                            }
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNode("Modify PCDCM Parameters")) {
                            ImGui::Text("Choose Associated DC Motor Numbers (0-3)");
                            for (size_t i = 0; i < precision_control_associated_dc_motor_numbers.size(); ++i) {
                                ImGui::Text("Motor %ld", i + 1);
                                ImGui::SameLine();
                                ImGui::SliderInt(
                                    (std::string("##pcdcm_") + std::to_string(i + 1) + std::string("_associated_dc_motor_number")).c_str(), 
                                    &precision_control_associated_dc_motor_numbers[i],
                                    0, 3, "%d", ImGuiSliderFlags_AlwaysClamp
                                );
                            }
                            // TODO: Handle setting two motors to the same PCDCM number
                            ImGui::Text("Precision Control Loop Period (ms)");
                            ImGui::SameLine();
                            ImGui::SliderInt("##precision_control_loop_period", &precision_control_loop_period, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
                            ImGui::Text("Precision Control Proportional Gain");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##precision_control_proportional_gain", &precision_control_proportional_gain, 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
                            ImGui::Text("Precision Control Integral Gain");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##precision_control_integral_gain", &precision_control_integral_gain, 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
                            ImGui::Text("Precision Control Derivative Gain");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##precision_control_derivative_gain", &precision_control_derivative_gain, 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
                            ImGui::TreePop();
                        }

                        for (size_t i = 0; i < std::size(set_precision_control_dc_motor_parameters); ++i) {
                            if (ImGui::Button(("Set PCDCM Parameters for PCDCM " + std::to_string(i+1)).c_str())) {
                                set_precision_control_dc_motor_parameters[i] = true;
                            }
                        }

                        configuration_mode = configuration_mode_checkbox;
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Controls (User)")) {
                    if (keyboard_mode)
                    {
                        ImGui::SeparatorText("Keyboard Bindings");
                        ImGui::Text("W - Surge Forward");
                        ImGui::Text("S - Surge Backward");
                        ImGui::Text("A - Sway Left");
                        ImGui::Text("D - Sway Right");
                        ImGui::Text("Q - Yaw Left");
                        ImGui::Text("E - Yaw Right");
                        ImGui::Text("T - Roll CW");
                        ImGui::Text("G - Roll CCW");
                        ImGui::Text("R - Heave Up");
                        ImGui::Text("F - Heave Down");
                        ImGui::Text("Z - Brighten All LEDs");
                        ImGui::Text("X - Dim All LEDs");
                        ImGui::Text("Right Arrow - Turn All Servos Clockwise");
                        ImGui::Text("Left Arrow - Turn All Servos Counter-Clockwise");
                        ImGui::Text("Page Up - Turn All PCDCMs Clockwise");
                        ImGui::Text("Page Down - Turn All PCDCMs Counter-Clockwise");
                        ImGui::Text("V - Toggle Fast Mode");
                        ImGui::Text("SPACE - Toggle Invert Controls");
                        ImGui::Text("1 - Spin DC Motors Forward At the Preset Speeds");
                        ImGui::Text("2 - Spin DC Motors Reverse At the Preset Speeds");
                        ImGui::Text("4 - Use Servo Preset Angle 1 For All Servos");
                        ImGui::Text("5 - Use Servo Preset Angle 2 For All Servos");
                        ImGui::Text("6 - Use Servo Preset Angle 3 For All Servos");
                        ImGui::Text("7 - Use PCDCM Preset Angle 1 For All PCDCMs");
                        ImGui::Text("8 - Use PCDCM Preset Angle 2 For All PCDCMs");
                        ImGui::Text("9 - Use PCDCM Preset Angle 3 For All PCDCMs");
                        
                    }
                    if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
                        int buttonCount, axisCount;
                        const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);
                        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axisCount);
                        ImGui::Text("Connected Controller: %s", glfwGetJoystickName(GLFW_JOYSTICK_1));
                        ImGui::Text("Axis Deadzone");
                        ImGui::SameLine();
                        ImGui::SliderFloat("##deadzone", &user_config.deadzone, 0.f, 1.f);
                        ImGui::SeparatorText("Buttons");
                        for (int i = 0; i < buttonCount; i++) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, !buttons[i], 1, 1));
                            ImGui::Text("%s", (std::string("Button ") + std::to_string(i)).c_str());
                            ImGui::PopStyleColor();
                            ImGui::SameLine(); 
                            if (ImGui::Combo((std::string("##button") + std::to_string(i)).c_str(), reinterpret_cast<int*>(&user_config.buttonActions[i]), buttonActionLabels, static_cast<int>(ButtonAction::SIZE))) {

                            }
                        }
                        ImGui::SeparatorText("Axes");
                        for (int i = 0; i < axisCount; i++) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1-abs(axes[i]), 1, 1));
                            ImGui::Text("%s", (std::string("Axis ") + std::to_string(i)).c_str());
                            ImGui::PopStyleColor();
                            ImGui::SameLine();
                            if (ImGui::Combo((std::string("##axis") + std::to_string(i)).c_str(), reinterpret_cast<int*>(&user_config.axisActions[i]), axisActionLabels, static_cast<int>(AxisAction::SIZE))) {
                                
                            }
                        }
                    } else {
                        ImGui::Text("No Controller Connected.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Save User Config")) {
                    ImGui::Text("User Config Name");
                    ImGui::SameLine(); 
                    ImGui::InputText("##configName", user_config.name, 64);
                    if (!glfwJoystickPresent(GLFW_JOYSTICK_1)) {
                        ImGui::Text("Controller must be connected");
                    } else if (ImGui::Button("Save")) {
                        json configJSON;
                        configJSON["name"] = user_config.name;
                        configJSON["controller1"] = glfwGetJoystickName(GLFW_JOYSTICK_1);
                        configJSON["controller2"] = "null";
                        configJSON["deadzone"] = user_config.deadzone;
                        for (size_t i = 0; i < user_config.axisActions.size(); i++) {
                            configJSON["mappings"]["0"]["deadzones"][std::to_string(i)] = user_config.deadzone; //for compatability with react gui
                        }
                        for (size_t i = 0; i < user_config.buttonActions.size(); i++) {
                            configJSON["mappings"]["0"]["buttons"][i] = buttonActionCodes[static_cast<int>(user_config.buttonActions[i])];
                        }
                        for (size_t i = 0; i < user_config.axisActions.size(); i++) {
                            configJSON["mappings"]["0"]["axes"][i] = axisActionCodes[static_cast<int>(user_config.axisActions[i])];
                        }
                        saveConfigNode->saveConfig(string(user_config.name), configJSON.dump());
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        //co-pilot controls window
        if (showPilotWindow) {
            ImGui::Begin("Co-Pilot Window", &showPilotWindow);
            
            ImGui::SliderInt("Power", &power.power, 0, 100);
            ImGui::SliderInt("Surge", &power.surge, 0, 100);
            ImGui::SliderInt("Sway", &power.sway, 0, 100);
            ImGui::SliderInt("Heave", &power.heave, 0, 100);
            ImGui::SliderInt("Roll", &power.roll, 0, 100);
            ImGui::SliderInt("Yaw", &power.yaw, 0, 100);

            ImGui::SeparatorText("Keybinds");
            ImGui::Text("1 - Set all to 0%%");
            ImGui::Text("2 - Set all to 50%%");
            ImGui::Text("3 - Set all to 0%%, set Heave and Power to 100%%");
            ImGui::Text("4 - Set all to 50%%, Heave to 75%% and Yaw to 30%%");
            ImGui::Text("V (Toggle) - Fast mode");
            if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                power.power = 0;
                power.surge = 0;
                power.sway = 0;
                power.heave = 0;
                power.roll = 0;
                power.yaw = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                power.power = 50;
                power.surge = 50;
                power.sway = 50;
                power.heave = 50;
                power.roll = 50;
                power.yaw = 50;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_3)) {
                power.power = 100;
                power.surge = 0;
                power.sway = 0;
                power.heave = 100;
                power.roll = 0;
                power.yaw = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_4)) {
                power.power = 50;
                power.surge = 50;
                power.sway = 50;
                power.heave = 75;
                power.roll = 50;
                power.yaw = 30;
            }

            ImGui::SeparatorText("Fast Mode Adjustment");
            ImGui::SliderInt("Fast Mode Power", &fast_mode_settings.power, 0, 100);
            ImGui::SliderInt("Fast Mode Surge", &fast_mode_settings.surge, 0, 100);
            ImGui::SliderInt("Fast Mode Sway", &fast_mode_settings.sway, 0, 100);
            ImGui::SliderInt("Fast Mode Heave", &fast_mode_settings.heave, 0, 100);
            ImGui::SliderInt("Fast Mode Roll", &fast_mode_settings.roll, 0, 100);
            ImGui::SliderInt("Fast Mode Yaw", &fast_mode_settings.yaw, 0, 100);

            ImGui::End();
        }

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    rclcpp::shutdown();
    return 0;
}

void saveBluestarConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const BluestarConfig& bluestar_config) {
    json configJSON;
    configJSON["name"] = "bluestar_config";
    configJSON["thruster_stronger_side_attenuation_constant"] = bluestar_config.thruster_stronger_side_attenuation_constant;

    for (size_t i = 0; i < std::size(bluestar_config.thruster_map); i++) {
        configJSON["thruster_map"][i] = std::stoi(bluestar_config.thruster_map[i]);
        configJSON["reverse_thrusters"][i] = bluestar_config.reverse_thrusters[i];
        configJSON["stronger_side_positive"][i] = bluestar_config.stronger_side_positive[i];
    }

    for (size_t i = 0; i < std::size(bluestar_config.preset_servo_angles); i++){
        for (size_t j = 0; j < std::size(bluestar_config.preset_servo_angles[i]); j++){
            configJSON["preset_servo_angles"][i][j] = bluestar_config.preset_servo_angles[i][j];
        }
    }

    for (size_t i = 0; i < std::size(bluestar_config.preset_precision_control_dc_motor_angles); i++){
        for (size_t j = 0; j < std::size(bluestar_config.preset_precision_control_dc_motor_angles[i]); j++){
            configJSON["preset_precision_control_dc_motor_angles"][i][j] = bluestar_config.preset_precision_control_dc_motor_angles[i][j];
        }
    }

    for (size_t i = 0; i < std::size(bluestar_config.dc_motor_speeds); i++){
        configJSON["dc_motor_speeds"][i] = bluestar_config.dc_motor_speeds[i];
    }

    configJSON["thruster_acceleration"] = bluestar_config.thruster_acceleration;
    configJSON["thruster_timeout"] = bluestar_config.thruster_timeout;

    for (size_t i = 0; i < std::size(bluestar_config.set_precision_control_dc_motor_parameters); i++){
        configJSON["precision_control_dc_motor_numbers"][i] = precision_control_associated_dc_motor_numbers[i];
        configJSON["precision_control_dc_motor_control_loop_period"][i] = precision_control_loop_period[i];
        configJSON["precision_control_dc_motor_proportional_gain"][i] = precision_control_proportional_gain[i];
        configJSON["precision_control_dc_motor_integral_gain"][i] = precision_control_integral_gain[i];
        configJSON["precision_control_dc_motor_derivative_gain"][i] = precision_control_derivative_gain[i];
    }   

    saveConfigNode->saveConfig("bluestar_config", configJSON.dump());
}