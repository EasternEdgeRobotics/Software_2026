#include "Image.hpp"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <nlohmann/json.hpp>
#include "Config.hpp"
#include "ROS.hpp"
#include "Camera.hpp"
#include <iostream>
#include <cstdint>
#include <memory>
#include "images/logo.h"
#include "images/nosignal.h"

using json = nlohmann::json;

UserConfig user_config;
WaterwitchConfig waterwitch_config;
Power power;
Power previous_power_settings;
Power fast_mode_settings;

bool showConfigWindow = false;
bool showCameraWindow = false;
bool showPilotWindow = false;

vector<string> names;
vector<string> configs;

int configuration_mode_thruster_number = 0;
bool configuration_mode = false;
bool keyboard_mode = false;
bool fast_mode = false;
bool invert_controls = false;

bool flipCam1VerticallyButtonPressedLatch = false;
bool flipCam2VerticallyButtonPressedLatch = false;
bool flipCam3VerticallyButtonPressedLatch = false;
bool flipCam4VerticallyButtonPressedLatch = false;
bool flipCam1HorizontallyButtonPressedLatch = false;
bool flipCam2HorizontallyButtonPressedLatch = false;
bool flipCam3HorizontallyButtonPressedLatch = false;
bool flipCam4HorizontallyButtonPressedLatch = false;

bool bilge_pump_on = false;
int bilge_pump_speed = 255;

bool bilge_pump_latch = false;
bool brighten_led_latch = false;
bool dim_led_latch = false;
bool fast_mode_latch = false;
bool invert_controls_latch = false;

// Predeclare function
void saveGlobalConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const WaterwitchConfig& waterwitch_config);

int main(int argc, char **argv) {
    //initialize glfw, imgui, and rclcpp (ros)    
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge Waterwitch GUI", NULL, NULL);
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
    
    //create images
    unsigned int noSignal = loadEmbeddedTexture(nosignal_jpg, nosignal_jpg_len);

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

    // Load the waterwitch config
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == "waterwitch_config")
        {
            json configData = json::parse(configs[i]);
            if (!configData["servos"][0].is_null()) std::strncpy(waterwitch_config.servo1SSHTarget, configData["servos"][0].get<std::string>().c_str(), sizeof(waterwitch_config.servo1SSHTarget));
            if (!configData["servos"][1].is_null()) std::strncpy(waterwitch_config.servo2SSHTarget, configData["servos"][1].get<std::string>().c_str(), sizeof(waterwitch_config.servo2SSHTarget));
            if (!configData["thruster_acceleration"].is_null()) waterwitch_config.thruster_acceleration = configData["thruster_acceleration"].get<float>();
            if (!configData["thruster_stronger_side_attenuation_constant"].is_null()) waterwitch_config.thruster_stronger_side_attenuation_constant = configData["thruster_stronger_side_attenuation_constant"].get<float>();
            for (size_t i = 0; i < std::size(waterwitch_config.thruster_map); i++){
                if (!configData["thruster_map"][i].is_null()) std::strncpy(waterwitch_config.thruster_map[i], std::to_string(configData["thruster_map"][i].get<int>()).c_str(), sizeof(waterwitch_config.thruster_map[i]));
                if (!configData["reverse_thrusters"][i].is_null()) waterwitch_config.reverse_thrusters[i] = configData["reverse_thrusters"][i].get<bool>();
                if (!configData["stronger_side_positive"][i].is_null()) waterwitch_config.stronger_side_positive[i] = configData["stronger_side_positive"][i].get<bool>();
            }
            for (size_t i = 0; i < std::size(waterwitch_config.front_camera_preset_servo_angles); i++){
                if (!configData["front_camera_preset_servo_angles"][i].is_null()) waterwitch_config.front_camera_preset_servo_angles[i] = configData["front_camera_preset_servo_angles"][i].get<int>();
            }
            for (size_t i = 0; i < std::size(waterwitch_config.back_camera_preset_servo_angles); i++){
                if (!configData["back_camera_preset_servo_angles"][i].is_null()) waterwitch_config.back_camera_preset_servo_angles[i] = configData["back_camera_preset_servo_angles"][i].get<int>();
            }
            break;
        }
    }

    Camera cam1(user_config.cam1ip, noSignal);
    Camera cam2(user_config.cam2ip, noSignal);
    Camera cam3(user_config.cam3ip, noSignal);
    Camera cam4(user_config.cam4ip, noSignal);

    cam1.start();
    cam2.start();
    cam3.start();
    cam4.start();

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
        bool brightenLED = false;
        bool dimLED = false;
        bool turnFrontServoCw = false;
        bool turnFrontServoCcw = false;
        bool turnBackServoCw = false;
        bool turnBackServoCcw = false;
        bool flipCam1VerticallyButtonPressed = false;
        bool flipCam2VerticallyButtonPressed = false;
        bool flipCam3VerticallyButtonPressed = false;
        bool flipCam4VerticallyButtonPressed = false;
        bool flipCam1HorizontallyButtonPressed = false;
        bool flipCam2HorizontallyButtonPressed = false;
        bool flipCam3HorizontallyButtonPressed = false;
        bool flipCam4HorizontallyButtonPressed = false;
        bool bilge_pump_toggle = false;
        bool fast_mode_toggle = false;
        bool invert_controls_toggle = false;

        // Note: A servo angle of -1 means that the exact angle is unset
        int frontServoAngle = -1;
        int backServoAngle = -1;

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
                if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) brightenLED = true;
                if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) dimLED = true;
                if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) turnFrontServoCw = true;
                if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) turnFrontServoCcw = true;
                if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) turnBackServoCw = true;
                if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) turnBackServoCcw = true;
                if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) flipCam1VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) flipCam2VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) flipCam3VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) flipCam4VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) flipCam1HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) flipCam2HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) flipCam3HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) bilge_pump_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) fast_mode_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS) flipCam4HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) invert_controls_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
                    frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[0]; 
                    backServoAngle = waterwitch_config.back_camera_preset_servo_angles[0];
                }
                if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
                    frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[1]; 
                    backServoAngle = waterwitch_config.back_camera_preset_servo_angles[1];
                }
                if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
                    frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[2]; 
                    backServoAngle = waterwitch_config.back_camera_preset_servo_angles[2];
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
                        case ButtonAction::BRIGHTEN_LED:
                            brightenLED = true;
                            break;
                        case ButtonAction::DIM_LED:
                            dimLED = true;
                            break;
                        case ButtonAction::TOGGLE_BILGE_PUMP:
                            bilge_pump_toggle = true;
                            break;
                        case ButtonAction::TURN_FRONT_SERVO_CW:
                            turnFrontServoCw = true;
                            break;
                        case ButtonAction::TURN_FRONT_SERVO_CCW:
                            turnFrontServoCcw = true;
                            break;
                        case ButtonAction::TURN_BACK_SERVO_CW:
                            turnBackServoCw = true;
                            break;
                        case ButtonAction::TURN_BACK_SERVO_CCW:
                            turnBackServoCcw = true;
                            break;
                        case ButtonAction::CONFIGURATION_MODE:
                            // Can either be set by user input for through the GUI
                            configuration_mode = !configuration_mode;
                            break;
                        case ButtonAction::FLIP_CAMERA_1_VERTICALLY:
                            flipCam1VerticallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_2_VERTICALLY:
                            flipCam2VerticallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_3_VERTICALLY:
                            flipCam3VerticallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_4_VERTICALLY:
                            flipCam4VerticallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_1_HORIZONTALLY:
                            flipCam1HorizontallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_2_HORIZONTALLY:
                            flipCam2HorizontallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_3_HORIZONTALLY:
                            flipCam3HorizontallyButtonPressed = true;
                            break;
                        case ButtonAction::FLIP_CAMERA_4_HORIZONTALLY:
                            flipCam4HorizontallyButtonPressed = true;
                            break;
                        case ButtonAction::FAST_MODE:
                            fast_mode_toggle = true;
                            break;  
                        case ButtonAction::USE_SERVO_ANGLE_PRESET_1:
                            frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[0];
                            backServoAngle = waterwitch_config.back_camera_preset_servo_angles[0];
                            break;
                        case ButtonAction::USE_SERVO_ANGLE_PRESET_2:
                            frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[1];
                            backServoAngle = waterwitch_config.back_camera_preset_servo_angles[1];
                            break;
                        case ButtonAction::USE_SERVO_ANGLE_PRESET_3:
                            frontServoAngle = waterwitch_config.front_camera_preset_servo_angles[2];
                            backServoAngle = waterwitch_config.back_camera_preset_servo_angles[2];
                            break;
                        case ButtonAction::INVERT_CONTROLS:
                            invert_controls_toggle = true;
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
                
            // We only want this input to be registered once per button hit 
            // rather than toggling every frame as long as button is pressed
            if (flipCam1VerticallyButtonPressed) {
                if (!flipCam1VerticallyButtonPressedLatch) cam1.flip_vertically();
                flipCam1VerticallyButtonPressedLatch = true;
            } else {
                flipCam1VerticallyButtonPressedLatch = false;
            }
            if (flipCam2VerticallyButtonPressed) {
                if (!flipCam2VerticallyButtonPressedLatch) cam2.flip_vertically();
                flipCam2VerticallyButtonPressedLatch = true;
            } else {
                flipCam2VerticallyButtonPressedLatch = false;
            }
            if (flipCam3VerticallyButtonPressed) {
                if (!flipCam3VerticallyButtonPressedLatch) cam3.flip_vertically();
                flipCam3VerticallyButtonPressedLatch = true;
            } else {
                flipCam3VerticallyButtonPressedLatch = false;
            }
            if (flipCam4VerticallyButtonPressed) {
                if (!flipCam4VerticallyButtonPressedLatch) cam4.flip_vertically();
                flipCam4VerticallyButtonPressedLatch = true;
            } else {
                flipCam4VerticallyButtonPressedLatch = false;
            }
            if (flipCam1HorizontallyButtonPressed) {
                if (!flipCam1HorizontallyButtonPressedLatch) cam1.flip_horizontally();
                flipCam1HorizontallyButtonPressedLatch = true;
            } else {
                flipCam1HorizontallyButtonPressedLatch = false;
            }
            if (flipCam2HorizontallyButtonPressed) {
                if (!flipCam2HorizontallyButtonPressedLatch) cam2.flip_horizontally();
                flipCam2HorizontallyButtonPressedLatch = true;
            } else {
                flipCam2HorizontallyButtonPressedLatch = false;
            }
            if (flipCam3HorizontallyButtonPressed) {
                if (!flipCam3HorizontallyButtonPressedLatch) cam3.flip_horizontally();
                flipCam3HorizontallyButtonPressedLatch = true;
            } else {
                flipCam3HorizontallyButtonPressedLatch = false;
            }
            if (flipCam4HorizontallyButtonPressed) {
                if (!flipCam4HorizontallyButtonPressedLatch) cam4.flip_horizontally();
                flipCam4HorizontallyButtonPressedLatch = true;
            } else {
                flipCam4HorizontallyButtonPressedLatch = false;
            }

            
            if (bilge_pump_toggle) {
                if (!bilge_pump_latch) bilge_pump_on = !bilge_pump_on;
                bilge_pump_latch = true;
            } else {
                bilge_pump_latch = false;
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
            if (brightenLED) {
                brightenLED = !brighten_led_latch;
                brighten_led_latch = true;
            } else {
                brighten_led_latch = false;
            }
            if (dimLED) {
                dimLED = !dim_led_latch;
                dim_led_latch = true;
            } else {
                dim_led_latch = false;
            }
        }

        uint8_t effective_bilge_pump_speed = bilge_pump_on ? bilge_pump_speed : 0;

        if (invert_controls)
        {
            surge = -surge;
            yaw = -yaw;
        }

        pilotInputNode->sendInput(power, surge, sway, heave, yaw, roll, brightenLED, dimLED, turnFrontServoCw,
            turnFrontServoCcw, turnBackServoCw, turnBackServoCcw, configuration_mode, frontServoAngle, 
            backServoAngle, configuration_mode_thruster_number, effective_bilge_pump_speed);
        

        //top menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Cameras")) {
                if (ImGui::MenuItem("Open Camera Window")) {
                    showCameraWindow = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Config")) {
                if (ImGui::MenuItem("Open Config Editor")) {
                    showConfigWindow = true;
                }
                if (ImGui::BeginMenu("Load UserConfig")) {
                    for (size_t i = 0; i < names.size(); i++) {
                        if (names[i] == "waterwitch_config")
                        {
                            continue;
                        }
                        if (ImGui::MenuItem(names[i].c_str())) {
                            json configData = json::parse(configs[i]);
                            if (!configData["cameras"][0].is_null()) std::strncpy(user_config.cam1ip, configData["cameras"][0].get<std::string>().c_str(), sizeof(user_config.cam1ip));
                            if (!configData["cameras"][1].is_null()) std::strncpy(user_config.cam2ip, configData["cameras"][1].get<std::string>().c_str(), sizeof(user_config.cam2ip));
                            if (!configData["cameras"][2].is_null()) std::strncpy(user_config.cam3ip, configData["cameras"][2].get<std::string>().c_str(), sizeof(user_config.cam3ip));
                            if (!configData["cameras"][3].is_null()) std::strncpy(user_config.cam4ip, configData["cameras"][3].get<std::string>().c_str(), sizeof(user_config.cam3ip));
                            user_config.deadzone = configData.value("deadzone", 0.1f);
                            user_config.buttonActions.clear();
                            for (auto& mapping : configData["mappings"]["0"]["buttons"].items()) {
                                user_config.buttonActions.push_back(stringToButtonAction(mapping.value()));
                            }
                            user_config.axisActions.clear();
                            for (auto& mapping : configData["mappings"]["0"]["axes"].items()) {
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
            ImGui::PushStyleColor(ImGuiCol_Text, bilge_pump_on ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
            ImGui::Text(bilge_pump_on ? " BILGE PUMP ON" : " BILGE PUMP OFF");
            ImGui::PopStyleColor();

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

            ImGui::SameLine();
            if (ImGui::Button("Cam1 Screenshot")) {
                if (!cam1.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam1");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam2 Screenshot")) {                
                if (!cam2.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam2");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam3 Screenshot")) {                
                if (!cam3.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam3");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam4 Screenshot")) {                
                if (!cam4.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam4");
            }

            //fps counter
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }

        //user_config window
        if (showConfigWindow) {
            ImGui::Begin("Config Editor", &showConfigWindow);
            if (ImGui::BeginTabBar("Config Tabs")) {
                if (ImGui::BeginTabItem("Cameras (User)")) {
                    ImGui::Text("Camera 1 URL");
                    ImGui::SameLine(); 
                    ImGui::InputText("##camera1", user_config.cam1ip, 64);
                    ImGui::Text("Camera 2 URL");
                    ImGui::SameLine(); 
                    ImGui::InputText("##camera2", user_config.cam2ip, 64);
                    ImGui::Text("Camera 3 URL");
                    ImGui::SameLine(); 
                    ImGui::InputText("##camera3", user_config.cam3ip, 64);
                    ImGui::Text("Camera 4 URL");
                    ImGui::SameLine(); 
                    ImGui::InputText("##camera4", user_config.cam4ip, 64);
                    ImGui::EndTabItem();
                    
                }
                if (ImGui::BeginTabItem("Waterwitch Config")){
                    if (!(keyboard_mode || glfwJoystickPresent(GLFW_JOYSTICK_1)))
                    {
                        ImGui::Text("Connect controller or enable keyboard mode to configure Waterwitch");
                    }
                    else
                    {
                        bool configuration_mode_checkbox = configuration_mode;
                        ImGui::Checkbox("Configuration Mode (Toggle to Save)", &configuration_mode_checkbox);
                        if (!configuration_mode_checkbox && configuration_mode)
                        {
                            // The backend uses the negative edge of configuration_mode to load the new config
                            // Thus, on the negative edge of configuration_mode, we should save the new global config
                            saveGlobalConfig(saveConfigNode, waterwitch_config);
                        }
                        if (configuration_mode_checkbox) { 
                            ImGui::Text("For Star (Forward Right)");
                            ImGui::SameLine();
                            ImGui::InputText("##for_star", waterwitch_config.thruster_map[0], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##for_star_rev_thruster", &waterwitch_config.reverse_thrusters[0]);
                            ImGui::SameLine();

                            ImGui::Checkbox("Stronger Side Positive##for_star_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[0]);
        
                            ImGui::Text("For Port (Forward Left)");
                            ImGui::SameLine();
                            ImGui::InputText("##for_port", waterwitch_config.thruster_map[1], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##for_port_rev_thruster", &waterwitch_config.reverse_thrusters[1]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##for_port_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[1]);
        
                            ImGui::Text("Aft star (Back Right)");
                            ImGui::SameLine();
                            ImGui::InputText("##aft_star", waterwitch_config.thruster_map[2], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##aft_star_rev_thruster", &waterwitch_config.reverse_thrusters[2]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##aft_star_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[2]);
        
                            ImGui::Text("Aft Port (Back Left)");
                            ImGui::SameLine();
                            ImGui::InputText("##aft_port", waterwitch_config.thruster_map[3], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##aft_port_rev_thruster", &waterwitch_config.reverse_thrusters[3]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##aft_port_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[3]);
        
                            ImGui::Text("Star Top (Right Top)");
                            ImGui::SameLine();
                            ImGui::InputText("##star_top", waterwitch_config.thruster_map[4], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##star_top_rev_thruster", &waterwitch_config.reverse_thrusters[4]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##star_top_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[4]);
        
                            ImGui::Text("Port Top (Left Top)");
                            ImGui::SameLine();
                            ImGui::InputText("##Port_top", waterwitch_config.thruster_map[5], 64);
                            ImGui::SameLine();
                            ImGui::Checkbox("Reverse Thruster##port_top_rev_thruster", &waterwitch_config.reverse_thrusters[5]);
                            ImGui::SameLine();
                            ImGui::Checkbox("Stronger Side Positive##port_top_thruster_stronger_size_positive", &waterwitch_config.stronger_side_positive[5]);
                            
                            ImGui::Text("The thruster acceleration determines how fast thrusters ramp up to the commanded speed");

                            ImGui::Text("Thruster Acceleration");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##thruster_acceleration", &waterwitch_config.thruster_acceleration, 0.1f, 1.0f, "%.05f", ImGuiSliderFlags_AlwaysClamp);
                            
                            ImGui::Text("The stronger side attentuation constant attenuates the power of the thruster on a given side");

                            ImGui::Text("Thruster Stronger Side Attenuation Constant");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##thruster_stronger_side_attenuation_constant", &waterwitch_config.thruster_stronger_side_attenuation_constant, 0.0f, 2.0f, "%.005f", ImGuiSliderFlags_AlwaysClamp);

                            const char* thrusterNumbers[] = { "0", "1", "2", "3", "4", "5" };
                            static int currentThrusterNumber = 0;
                            ImGui::Text("Configuration Mode Thruster Number");
                            ImGui::SameLine();
                            if (ImGui::Combo("##thruster_number", &currentThrusterNumber, thrusterNumbers, IM_ARRAYSIZE(thrusterNumbers))) {
                                configuration_mode_thruster_number = currentThrusterNumber;
                            }
                            
                            ImGui::Text("Servo 1 (Front Servo) SSH Target");
                            ImGui::SameLine();
                            ImGui::InputText("##servo1", waterwitch_config.servo1SSHTarget, 64);
                            ImGui::Text("Servo 2 (Front Servo) SSH Target");
                            ImGui::SameLine();
                            ImGui::InputText("##servo2", waterwitch_config.servo2SSHTarget, 64);

                            if (ImGui::TreeNode("Modify Preset Servo Angles"))
                            {
                                for (size_t i = 0; i < std::size(waterwitch_config.front_camera_preset_servo_angles); ++i) {
                                    ImGui::Text("Front Camera Preset Servo Angle %ld", i + 1);
                                    ImGui::SameLine();
                                    ImGui::SliderInt(
                                        (std::string("##front_camera_preset_servo_angle_") + std::to_string(i + 1)).c_str(),
                                        &waterwitch_config.front_camera_preset_servo_angles[i],
                                        0, 180, "%d", ImGuiSliderFlags_AlwaysClamp
                                    );
                                }
                                for (size_t i = 0; i < std::size(waterwitch_config.back_camera_preset_servo_angles); ++i) {
                                    ImGui::Text("Back Camera Preset Servo Angle %ld", i + 1);
                                    ImGui::SameLine();
                                    ImGui::SliderInt(
                                        (std::string("##back_camera_preset_servo_angle_") + std::to_string(i + 1)).c_str(),
                                        &waterwitch_config.back_camera_preset_servo_angles[i],
                                        0, 180, "%d", ImGuiSliderFlags_AlwaysClamp
                                    );
                                }
                                ImGui::TreePop();
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
                        ImGui::Text("SPACE - Invert Controls (Surge, Sway, Roll)");
                        ImGui::Text("Z - Brighten LED");
                        ImGui::Text("X - Dim LED");
                        ImGui::Text("C - Toggle Bilge Pump");
                        ImGui::Text("Right Arrow - Turn Front Servo Clockwise");
                        ImGui::Text("Left Arrow - Turn Front Servo Counter-Clockwise");
                        ImGui::Text("Page Up - Turn Back Servo Clockwise");
                        ImGui::Text("Page Down - Turn Back Servo Counter-Clockwise");
                        ImGui::Text("I - Flip Camera 1 Vertically");
                        ImGui::Text("O - Flip Camera 2 Vertically");
                        ImGui::Text("P - Flip Camera 3 Vertically");
                        ImGui::Text("] - Flip Camera 4 Vertically");
                        ImGui::Text("B - Flip Camera 1 Horizontally");
                        ImGui::Text("N - Flip Camera 2 Horizontally");
                        ImGui::Text("M - Flip Camera 3 Horizontally");
                        ImGui::Text("V - Fast Mode");
                        ImGui::Text("5 - Use Servo Preset Angle 1");
                        ImGui::Text("6 - Use Servo Preset Angle 2");
                        ImGui::Text("7 - Use Servo Preset Angle 3");
                        
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
                        json configJson;
                        configJson["name"] = user_config.name;
                        configJson["cameras"][0] = user_config.cam1ip;
                        configJson["cameras"][1] = user_config.cam2ip;
                        configJson["cameras"][2] = user_config.cam3ip;
                        configJson["cameras"][3] = user_config.cam4ip;
                        configJson["controller1"] = glfwGetJoystickName(GLFW_JOYSTICK_1);
                        configJson["controller2"] = "null";
                        configJson["deadzone"] = user_config.deadzone;
                        for (size_t i = 0; i < user_config.axisActions.size(); i++) {
                            configJson["mappings"]["0"]["deadzones"][std::to_string(i)] = user_config.deadzone; //for compatability with react gui
                        }
                        for (size_t i = 0; i < user_config.buttonActions.size(); i++) {
                            configJson["mappings"]["0"]["buttons"][i] = buttonActionCodes[static_cast<int>(user_config.buttonActions[i])];
                        }
                        for (size_t i = 0; i < user_config.axisActions.size(); i++) {
                            configJson["mappings"]["0"]["axes"][i] = axisActionCodes[static_cast<int>(user_config.axisActions[i])];
                        }
                        saveConfigNode->saveConfig(string(user_config.name), configJson.dump());
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        //camera window
        if (showCameraWindow) {
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, (io.DisplaySize.y - ImGui::GetFrameHeight())));
            ImGui::SetNextWindowPos(ImVec2{0, ImGui::GetFrameHeight()});
            ImGui::Begin("Camera Window", &showCameraWindow, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 availPos = ImGui::GetContentRegionAvail();

            ImGui::SetCursorPos(ImVec2(windowPos.x+8, windowPos.y+8));
            cam1.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2((availPos.x-24)/2+16, windowPos.y+8));
            cam2.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2(windowPos.x+8, (availPos.y-24)/2+35));
            cam3.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2((availPos.x-24)/2+16, (availPos.y-24)/2+35));
            cam4.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));

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
            ImGui::SliderInt("Bilge Pump Speed", &bilge_pump_speed, 0, 255);

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

void saveGlobalConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const WaterwitchConfig& waterwitch_config) {
    json configJson;
    configJson["name"] = "waterwitch_config";
    configJson["servos"][0] = waterwitch_config.servo1SSHTarget;
    configJson["servos"][1] = waterwitch_config.servo2SSHTarget;
    configJson["thruster_acceleration"] = waterwitch_config.thruster_acceleration;
    configJson["thruster_stronger_side_attenuation_constant"] = waterwitch_config.thruster_stronger_side_attenuation_constant;

    for (size_t i = 0; i < std::size(waterwitch_config.thruster_map); i++) {
        configJson["thruster_map"][i] = std::stoi(waterwitch_config.thruster_map[i]);
        configJson["reverse_thrusters"][i] = waterwitch_config.reverse_thrusters[i];
        configJson["stronger_side_positive"][i] = waterwitch_config.stronger_side_positive[i];
    }

    for (size_t i = 0; i < std::size(waterwitch_config.front_camera_preset_servo_angles); i++) {
        configJson["front_camera_preset_servo_angles"][i] = waterwitch_config.front_camera_preset_servo_angles[i];
    }

    for (size_t i = 0; i < std::size(waterwitch_config.back_camera_preset_servo_angles); i++) {
        configJson["back_camera_preset_servo_angles"][i] = waterwitch_config.back_camera_preset_servo_angles[i];
    }

    saveConfigNode->saveConfig("waterwitch_config", configJson.dump());
}