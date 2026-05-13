#include "Image.hpp"
#include <glad/glad.h>
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
#include "streaming/CameraStreamFactory.hpp"
#include <optional>
#include <stdexcept>

using json = nlohmann::json;

UserConfig user_config;
BlueStarConfig bluestar_config;
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

bool servo_1_cw_latch = false;
bool servo_2_cw_latch = false;
bool servo_3_cw_latch = false;
bool servo_4_cw_latch = false;
bool servo_1_ccw_latch = false;
bool servo_2_ccw_latch = false;
bool servo_3_ccw_latch = false;
bool servo_4_ccw_latch = false;

bool brighten_led_latch_1 = false;
bool brighten_led_latch_2 = false;
bool dim_led_latch_1 = false;
bool dim_led_latch_2 = false;
bool fast_mode_latch = false;
bool invert_controls_latch = false;

int LED_BRIGHTNESS_INCREMENT = 51; // 5 levels
int SERVO_FREQ_INCREMENT = 17; // 15 levels

// Predeclare function
void saveGlobalConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const BlueStarConfig& bluestar_config);


struct CliOptions {
    std::optional<std::string> userConfigName;
};

CliOptions parseCliOptions(int argc, char** argv) {
    CliOptions options;

    auto args = rclcpp::remove_ros_arguments(argc, argv);

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];

        if (arg == "--user-config") {
            if (i + 1 >= args.size()) {
                throw std::runtime_error("--user-config requires a value");
            }

            options.userConfigName = args[++i];
        } else if (arg == "--help") {
            std::cout
                << "BlueStar GUI options:\n"
                << "  --user-config NAME    Load user config from CLI\n";

            std::exit(0);
        }
    }

    return options;
}


// Userconfig loading
bool loadUserConfigFromJson(
    UserConfig& output,
    const std::string& configName,
    const std::string& configString
) {
    try {
        json configData = json::parse(configString);

        std::snprintf(
            output.name,
            sizeof(output.name),
            "%s",
            configData.value("name", configName).c_str()
        );

        output.deadzone = configData.value("deadzone", 0.1f);
        output.controllerName = configData.value("controller1", "");
        output.controllerGuid = configData.value("controller1_guid", "");

        output.show_co_pilot_window = configData.value("show_co_pilot_window", false);
        output.show_camera_window = configData.value("show_camera_window", false);

        showPilotWindow = output.show_co_pilot_window;
        showCameraWindow = output.show_camera_window;

        output.buttonActions.clear();
        output.axisActions.clear();

        if (configData.contains("mappings") &&
            configData["mappings"].contains("0")) {
            const auto& mappings = configData["mappings"]["0"];

            if (mappings.contains("buttons")) {
                for (auto& mapping : mappings["buttons"].items()) {
                    output.buttonActions.push_back(
                        stringToButtonAction(mapping.value())
                    );
                }
            }

            if (mappings.contains("axes")) {
                for (auto& mapping : mappings["axes"].items()) {
                    output.axisActions.push_back(
                        stringToAxisAction(mapping.value())
                    );
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load user config '" << configName
                  << "': " << e.what() << std::endl;
        return false;
    }
}

bool loadUserConfigByName(
    UserConfig& output,
    const std::string& configName,
    const std::vector<std::string>& names,
    const std::vector<std::string>& configs
) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == configName) {
            return loadUserConfigFromJson(output, names[i], configs[i]);
        }
    }

    std::cerr << "User config not found: " << configName << std::endl;
    return false;
}

bool validateUserConfig(
    const UserConfig& config,
    std::string& error
) {
    if (!glfwJoystickPresent(GLFW_JOYSTICK_1)) {
        error = "No controller connected as GLFW_JOYSTICK_1";
        return false;
    }

    const char* joystickName = glfwGetJoystickName(GLFW_JOYSTICK_1);
    const char* joystickGuid = glfwGetJoystickGUID(GLFW_JOYSTICK_1);

    if (!config.controllerGuid.empty()) {
        if (!joystickGuid || config.controllerGuid != joystickGuid) {
            error = "Controller GUID does not match config";
            return false;
        }
    } else if (!config.controllerName.empty()) {
        if (!joystickName || config.controllerName != joystickName) {
            error = "Controller name does not match config";
            return false;
        }
    } else {
        error = "Config does not contain controller identity metadata";
        return false;
    }

    int buttonCount = 0;
    int axisCount = 0;

    glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);
    glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axisCount);

    if (config.buttonActions.size() > static_cast<size_t>(buttonCount)) {
        error = "Config expects more buttons than GLFW_JOYSTICK_1 has";
        return false;
    }

    if (config.axisActions.size() > static_cast<size_t>(axisCount)) {
        error = "Config expects more axes than GLFW_JOYSTICK_1 has";
        return false;
    }

    error.clear();
    return true;
}


int main(int argc, char **argv) {
    //initialize glfw, imgui, and rclcpp (ros)    
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge BlueStar GUI", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    #ifndef __APPLE__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    #endif

    // OpenGL info
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);

    // std::cerr << "GL_VERSION: "
    //         << (version ? reinterpret_cast<const char*>(version) : "null")
    //         << std::endl;
    // std::cerr << "GL_RENDERER: "
    //         << (renderer ? reinterpret_cast<const char*>(renderer) : "null")
    //         << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    rclcpp::init(argc, argv);

    CliOptions cliOptions;

    try {
        cliOptions = parseCliOptions(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "CLI error: " << e.what() << std::endl;
        return 1;
    }
    
    //create images
    unsigned int noSignal = loadEmbeddedTexture(nosignal_jpg, nosignal_jpg_len);

    //create ros nodes
    auto saveConfigNode = std::make_shared<SaveConfigPublisher>();
    auto pilotInputNode = std::make_shared<PilotInputPublisher>();

    //get configs from ros, then set the names and configs
    std::array<std::vector<std::string>, 2> configRes = getConfigs();
    names = configRes[0];
    configs = configRes[1];

    if (cliOptions.userConfigName.has_value()) {
        UserConfig cliUserConfig;

        if (loadUserConfigByName(
                cliUserConfig,
                *cliOptions.userConfigName,
                names,
                configs
            )) {
            std::string validationError;

            if (validateUserConfig(
                    cliUserConfig,
                    validationError
                )) {
                user_config = cliUserConfig;

                std::cout << "Loaded CLI user config: "
                        << *cliOptions.userConfigName << std::endl;
            } else {
                std::cerr << "Refusing to load CLI user config '"
                        << *cliOptions.userConfigName
                        << "': "
                        << validationError
                        << std::endl;
            }
        }
    }
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
            json configData = json::parse(configs[i]);
            if (!configData["thruster_acceleration"].is_null()) bluestar_config.thruster_acceleration = configData["thruster_acceleration"].get<float>();
            if (!configData["thruster_stronger_side_attenuation_constant"].is_null()) bluestar_config.thruster_stronger_side_attenuation_constant = configData["thruster_stronger_side_attenuation_constant"].get<float>();
            for (size_t i = 0; i < std::size(bluestar_config.thruster_map); i++){
                if (!configData["thruster_map"][i].is_null()) std::strncpy(bluestar_config.thruster_map[i], std::to_string(configData["thruster_map"][i].get<int>()).c_str(), sizeof(bluestar_config.thruster_map[i]));
                if (!configData["reverse_thrusters"][i].is_null()) bluestar_config.reverse_thrusters[i] = configData["reverse_thrusters"][i].get<bool>();
                if (!configData["stronger_side_positive"][i].is_null()) bluestar_config.stronger_side_positive[i] = configData["stronger_side_positive"][i].get<bool>();
            }
            for (size_t i = 0; i < std::size(bluestar_config.servo_1_preset_angles); i++){
                if (!configData["servo_1_preset_angles"][i].is_null()) bluestar_config.servo_1_preset_angles[i] = configData["servo_1_preset_angles"][i].get<int>();
            }
            for (size_t i = 0; i < std::size(bluestar_config.servo_2_preset_angles); i++){
                if (!configData["servo_2_preset_angles"][i].is_null()) bluestar_config.servo_2_preset_angles[i] = configData["servo_2_preset_angles"][i].get<int>();
            }

            if (!configData["cam1ip"].is_null()) std::snprintf(bluestar_config.cam1ip, sizeof(bluestar_config.cam1ip), "%s",configData["cam1ip"].get<std::string>().c_str());
            if (!configData["cam2ip"].is_null()) std::snprintf(bluestar_config.cam2ip, sizeof(bluestar_config.cam2ip), "%s",configData["cam2ip"].get<std::string>().c_str());
            if (!configData["cam3ip"].is_null()) std::snprintf(bluestar_config.cam3ip, sizeof(bluestar_config.cam3ip), "%s",configData["cam3ip"].get<std::string>().c_str());
            if (!configData["cam4ip"].is_null()) std::snprintf(bluestar_config.cam4ip, sizeof(bluestar_config.cam4ip), "%s",configData["cam4ip"].get<std::string>().c_str());

            if (!configData["cam1_video_caps"].is_null()) std::snprintf(bluestar_config.cam1_video_caps, sizeof(bluestar_config.cam1_video_caps), "%s",configData["cam1_video_caps"].get<std::string>().c_str());
            if (!configData["cam1_audio_caps"].is_null()) std::snprintf(bluestar_config.cam1_audio_caps, sizeof(bluestar_config.cam1_audio_caps), "%s",configData["cam1_audio_caps"].get<std::string>().c_str());

            if (!configData["cam2_video_caps"].is_null()) std::snprintf(bluestar_config.cam2_video_caps, sizeof(bluestar_config.cam2_video_caps), "%s",configData["cam2_video_caps"].get<std::string>().c_str());
            if (!configData["cam2_audio_caps"].is_null()) std::snprintf(bluestar_config.cam2_audio_caps, sizeof(bluestar_config.cam2_audio_caps), "%s",configData["cam2_audio_caps"].get<std::string>().c_str());

            if (!configData["cam3_video_caps"].is_null()) std::snprintf(bluestar_config.cam3_video_caps, sizeof(bluestar_config.cam3_video_caps), "%s",configData["cam3_video_caps"].get<std::string>().c_str());
            if (!configData["cam3_audio_caps"].is_null()) std::snprintf(bluestar_config.cam3_audio_caps, sizeof(bluestar_config.cam3_audio_caps), "%s",configData["cam3_audio_caps"].get<std::string>().c_str());

            if (!configData["cam4_video_caps"].is_null()) std::snprintf(bluestar_config.cam4_video_caps, sizeof(bluestar_config.cam4_video_caps), "%s",configData["cam4_video_caps"].get<std::string>().c_str());
            if (!configData["cam4_audio_caps"].is_null()) std::snprintf(bluestar_config.cam4_audio_caps, sizeof(bluestar_config.cam4_audio_caps), "%s",configData["cam4_audio_caps"].get<std::string>().c_str());
            break;
        }
    }

    Camera cam1(bluestar_config.cam1ip, bluestar_config.cam1_video_caps, bluestar_config.cam1_audio_caps, noSignal, 1);
    Camera cam2(bluestar_config.cam2ip, bluestar_config.cam2_video_caps, bluestar_config.cam2_audio_caps, noSignal, 2);
    Camera cam3(bluestar_config.cam3ip, bluestar_config.cam3_video_caps, bluestar_config.cam3_audio_caps, noSignal, 3);
    Camera cam4(bluestar_config.cam4ip, bluestar_config.cam4_video_caps, bluestar_config.cam4_audio_caps, noSignal, 4);

    cam1.start();
    cam2.start();
    cam3.start();
    cam4.start();

    int Servo1Angle = 127;
    int Servo2Angle = 127;
    int Servo3Angle = 127;
    int Servo4Angle = 127;

    int LED1Brightness = 0;
    int LED2Brightness = 0;

    // Uncomment these to keep the DC motors at a speed w/o holding the button
    // int dc_motor_1 = 0;
    // int dc_motor_2 = 0;

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
        int dc_motor_1 = 0;
        int dc_motor_2 = 0;
        bool LED_1_brighten_pressed = false;
        bool LED_2_brighten_pressed = false;
        bool LED_1_dim_pressed = false;
        bool LED_2_dim_pressed = false;
        bool flipCam1VerticallyButtonPressed = false;
        bool flipCam2VerticallyButtonPressed = false;
        bool flipCam3VerticallyButtonPressed = false;
        bool flipCam4VerticallyButtonPressed = false;
        bool flipCam1HorizontallyButtonPressed = false;
        bool flipCam2HorizontallyButtonPressed = false;
        bool flipCam3HorizontallyButtonPressed = false;
        bool flipCam4HorizontallyButtonPressed = false;
        bool Servo_1_CW_Pressed = false;
        bool Servo_1_CCW_Pressed = false;
        bool Servo_2_CW_Pressed = false;
        bool Servo_2_CCW_Pressed = false;
        bool Servo_3_CW_Pressed = false;
        bool Servo_3_CCW_Pressed = false;
        bool Servo_4_CW_Pressed = false;
        bool Servo_4_CCW_Pressed = false;
        bool fast_mode_toggle = false;
        bool invert_controls_toggle = false;

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
                if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) LED_1_brighten_pressed = true;
                if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) LED_1_dim_pressed = true;
                if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) LED_2_brighten_pressed = true;
                if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) LED_2_dim_pressed = true;
                if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) Servo_1_CW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) Servo_1_CCW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) Servo_2_CW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) Servo_2_CCW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) Servo_3_CW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) Servo_3_CCW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_KP_3) == GLFW_PRESS) Servo_4_CW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS) Servo_4_CCW_Pressed = true;
                if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) flipCam1VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) flipCam2VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) flipCam3VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) flipCam4VerticallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) flipCam1HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) flipCam2HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) flipCam3HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS) flipCam4HorizontallyButtonPressed = true;
                if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) fast_mode_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) invert_controls_toggle = true;
                if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
                    Servo1Angle = bluestar_config.servo_1_preset_angles[0]; 
                    Servo2Angle = bluestar_config.servo_2_preset_angles[0];
                }
                if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
                    Servo1Angle = bluestar_config.servo_1_preset_angles[1]; 
                    Servo2Angle = bluestar_config.servo_2_preset_angles[1];
                }
                if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
                    Servo1Angle = bluestar_config.servo_1_preset_angles[2]; 
                    Servo2Angle = bluestar_config.servo_2_preset_angles[2];
                }
                if (glfwGetKey(window, GLFW_KEY_KP_1) == GLFW_PRESS) dc_motor_1 = 127;
                if (glfwGetKey(window, GLFW_KEY_KP_2) == GLFW_PRESS) dc_motor_1 = 255;
                if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS) dc_motor_2 = 127;
                if (glfwGetKey(window, GLFW_KEY_KP_5) == GLFW_PRESS) dc_motor_2 = 255;
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
                        case ButtonAction::BRIGHTEN_LED_1:
                            LED_1_brighten_pressed = true;
                            break;
                        case ButtonAction::DIM_LED_1:
                            LED_1_dim_pressed = true;
                            break;
                        case ButtonAction::BRIGHTEN_LED_2:
                            LED_2_brighten_pressed = true;
                            break;
                        case ButtonAction::DIM_LED_2:
                            LED_2_dim_pressed = true;
                            break;
                        case ButtonAction::SERVO_1_CW:
                            Servo_1_CW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_1_CCW:
                            Servo_1_CCW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_2_CW:
                            Servo_2_CW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_2_CCW:
                            Servo_2_CCW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_3_CW:
                            Servo_3_CW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_3_CCW:
                            Servo_3_CCW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_4_CW:
                            Servo_4_CW_Pressed = true;
                            break;
                        case ButtonAction::SERVO_4_CCW:
                            Servo_4_CCW_Pressed = true;
                            break;
                        case ButtonAction::DC_MOTOR_1_CW:
                            dc_motor_1 = 127;
                            break;
                        case ButtonAction::DC_MOTOR_1_CCW:
                            dc_motor_1 = 255;
                            break;
                        case ButtonAction::DC_MOTOR_2_CW:
                            dc_motor_2 = 127;
                            break;
                        case ButtonAction::DC_MOTOR_2_CCW:
                            dc_motor_2 = 255;
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
                            Servo1Angle = bluestar_config.servo_1_preset_angles[0];
                            Servo2Angle = bluestar_config.servo_2_preset_angles[0];
                            break;
                        case ButtonAction::USE_SERVO_ANGLE_PRESET_2:
                            Servo1Angle = bluestar_config.servo_1_preset_angles[1];
                            Servo2Angle = bluestar_config.servo_2_preset_angles[1];
                            break;
                        case ButtonAction::USE_SERVO_ANGLE_PRESET_3:
                            Servo1Angle = bluestar_config.servo_1_preset_angles[2];
                            Servo2Angle = bluestar_config.servo_2_preset_angles[2];
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

            // These have to be pointers, for anyone looking at this in the future, i spent so many commits trying to figure out why i couldnt change the values, it looked like a 2010s minecraft letsplay series. -PC
            bool* servo_latches_cw[]  = {&servo_1_cw_latch,  &servo_2_cw_latch,  &servo_3_cw_latch,  &servo_4_cw_latch};
            bool* servo_latches_ccw[] = {&servo_1_ccw_latch, &servo_2_ccw_latch, &servo_3_ccw_latch, &servo_4_ccw_latch};
            int*  servo_angle_val[]   = {&Servo1Angle, &Servo2Angle, &Servo3Angle, &Servo4Angle};
            bool  servo_cw_state[]    = {Servo_1_CW_Pressed,  Servo_2_CW_Pressed,  Servo_3_CW_Pressed,  Servo_4_CW_Pressed};
            bool  servo_ccw_state[]   = {Servo_1_CCW_Pressed, Servo_2_CCW_Pressed, Servo_3_CCW_Pressed, Servo_4_CCW_Pressed};

            for (int i = 0; i < 4; i++) {
                if (servo_cw_state[i]) {
                    if (!*servo_latches_cw[i]) {
                        *servo_angle_val[i] = (*servo_angle_val[i] + SERVO_FREQ_INCREMENT > 255) ? 255 : *servo_angle_val[i] + SERVO_FREQ_INCREMENT;
                        *servo_latches_cw[i] = true;
                    }
                } else {
                    *servo_latches_cw[i] = false;
                }

                if (servo_ccw_state[i]) {
                    if (!*servo_latches_ccw[i]) {
                        *servo_angle_val[i] = (*servo_angle_val[i] < SERVO_FREQ_INCREMENT) ? 0 : *servo_angle_val[i] - SERVO_FREQ_INCREMENT;
                        *servo_latches_ccw[i] = true;
                    }
                } else {
                    *servo_latches_ccw[i] = false;
                }
            }

            bool* led_latch_bri[]  = {&brighten_led_latch_1,  &brighten_led_latch_2};
            bool* led_latch_dim[] = {&dim_led_latch_1, &dim_led_latch_2};
            int*  led_brightness_val[]   = {&LED1Brightness, &LED2Brightness};
            bool  led_bri_state[]    = {LED_1_brighten_pressed,  LED_2_brighten_pressed};
            bool  led_dim_state[]   = {LED_1_dim_pressed, LED_2_dim_pressed};

            for (int i = 0; i < 2; i++) {
                if (led_bri_state[i]) {
                    if (!*led_latch_bri[i]) {
                        *led_brightness_val[i] = (*led_brightness_val[i] + LED_BRIGHTNESS_INCREMENT > 255) ? 255 : *led_brightness_val[i] + LED_BRIGHTNESS_INCREMENT;
                        *led_latch_bri[i] = true;
                    }
                } else {
                    *led_latch_bri[i] = false;
                }

                if (led_dim_state[i]) {
                    if (!*led_latch_dim[i]) {
                        *led_brightness_val[i] = (*led_brightness_val[i] < LED_BRIGHTNESS_INCREMENT) ? 0 : *led_brightness_val[i] - LED_BRIGHTNESS_INCREMENT;
                        *led_latch_dim[i] = true;
                    }
                } else {
                    *led_latch_dim[i] = false;
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
        }

        if (invert_controls)
        {
            surge = -surge;
            yaw = -yaw;
        }
        
        // THIS MUST MATCH THE ORDER IN ROS.hpp -PC
        pilotInputNode->sendInput(power, surge, sway, heave, yaw, roll, 
            dc_motor_1, dc_motor_2, LED1Brightness, LED2Brightness, 
            Servo1Angle, Servo2Angle, Servo3Angle, Servo4Angle,
            configuration_mode, configuration_mode_thruster_number);
        

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
                        if (names[i] == "bluestar_config")
                        {
                            continue;
                        }
                        if (ImGui::MenuItem(names[i].c_str())) {
                            loadUserConfigFromJson(user_config, names[i], configs[i]);
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
            
            // Servo debug
            // ImGui::SameLine();
            // ImGui::Text("Servo 1: %.1d", Servo1Angle);
            // ImGui::SameLine();
            // ImGui::Text("Servo 2: %.1d", Servo2Angle);
            // ImGui::SameLine();
            // ImGui::Text("Servo 3: %.1d", Servo3Angle);
            // ImGui::SameLine();
            // ImGui::Text("Servo 4: %.1d", Servo4Angle);
            
            // LED debug
            // ImGui::SameLine();
            // ImGui::Text("LED 1: %.1d", LED1Brightness);
            // ImGui::SameLine();
            // ImGui::Text("LED 2: %.1d", LED2Brightness);
            // ImGui::SameLine();

            //fps counter
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }

        //user_config window
        if (showConfigWindow) {
            ImGui::SetNextWindowSize(ImVec2(750, 400), ImGuiCond_FirstUseEver); // Prevents the window from not being resizeable
            ImGui::Begin("Config Editor", &showConfigWindow);
            if (ImGui::BeginTabBar("Config Tabs")) {
                if (ImGui::BeginTabItem("Cameras")) {

                    #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                        constexpr int cameraColumnCount = 4;
                    #else
                        constexpr int cameraColumnCount = 2;
                    #endif

                    ImGui::Text("URLs must include all elements. (ie http://192.168.137.200:8889/cam/whep)");

                    #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                        ImGui::Text("Video and audio caps can be left blank if they match defaults. (H264 w/ no audio)");
                    #endif
                    if (ImGui::BeginTable("Camera Table", cameraColumnCount, ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchSame |
                                ImGuiTableFlags_Resizable)) {

                        ImGui::TableSetupColumn("Number", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_WidthStretch);

                        #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                            ImGui::TableSetupColumn(
                                "Video Caps",
                                ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableSetupColumn(
                                "Audio Caps",
                                ImGuiTableColumnFlags_WidthStretch);
                        #endif

                        ImGui::TableHeadersRow();

                        char* cameraUrls[] = {
                            bluestar_config.cam1ip,
                            bluestar_config.cam2ip,
                            bluestar_config.cam3ip,
                            bluestar_config.cam4ip,
                        };

                        for (size_t cam = 0; cam < 4; ++cam) {
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Camera %zu", cam + 1);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::SetNextItemWidth(-FLT_MIN); // -FLT_MIN Sets it to be as wide as the col
                            ImGui::InputText(
                                (std::string("##camera_url_") + std::to_string(cam)).c_str(),
                                cameraUrls[cam],
                                512);

                            #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                                char* cameravcap[] = {
                                    bluestar_config.cam1_video_caps,
                                    bluestar_config.cam2_video_caps,
                                    bluestar_config.cam3_video_caps,
                                    bluestar_config.cam4_video_caps,
                                };

                                char* cameraacap[] = {
                                    bluestar_config.cam1_audio_caps,
                                    bluestar_config.cam2_audio_caps,
                                    bluestar_config.cam3_audio_caps,
                                    bluestar_config.cam4_audio_caps,
                                };

                                ImGui::TableSetColumnIndex(2);
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::InputText(
                                    (std::string("##camera_video_caps_") + std::to_string(cam)).c_str(),
                                    cameravcap[cam],
                                    1024);

                                ImGui::TableSetColumnIndex(3);
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::InputText(
                                    (std::string("##camera_audio_caps_") + std::to_string(cam)).c_str(),
                                    cameraacap[cam],
                                    1024);
                            #endif
                        }

                        ImGui::EndTable();
                    }

                    ImGui::EndTabItem();

                }
                if (ImGui::BeginTabItem("BlueStar Config")){
                    if (!(keyboard_mode || glfwJoystickPresent(GLFW_JOYSTICK_1)))
                    {
                        ImGui::Text("Connect controller or enable keyboard mode to configure BlueStar");
                    }
                    else
                    {
                        bool configuration_mode_checkbox = configuration_mode;
                        ImGui::Checkbox("Configuration Mode (Toggle to Save)", &configuration_mode_checkbox);
                        if (!configuration_mode_checkbox && configuration_mode)
                        {
                            // The backend uses the negative edge of configuration_mode to load the new config
                            // Thus, on the negative edge of configuration_mode, we should save the new global config
                            saveGlobalConfig(saveConfigNode, bluestar_config);
                        }
                        if (configuration_mode_checkbox) { 
                            if (ImGui::BeginTable("Thrusters", 4, ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Name");
                                ImGui::TableSetupColumn("Index");
                                ImGui::TableSetupColumn("Reverse");
                                ImGui::TableSetupColumn("Stronger Side Positive");
                                ImGui::TableHeadersRow();

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("For Star (Forward Right)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##for_star", bluestar_config.thruster_map[0], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##for_star_rev_thruster", &bluestar_config.reverse_thrusters[0]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##for_star_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[0]);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("For Port (Forward Left)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##for_port", bluestar_config.thruster_map[1], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##for_port_rev_thruster", &bluestar_config.reverse_thrusters[1]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##for_port_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[1]);
                                
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("Aft star (Back Right)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##aft_star", bluestar_config.thruster_map[2], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##aft_star_rev_thruster", &bluestar_config.reverse_thrusters[2]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##aft_star_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[2]);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("Aft Port (Back Left)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##aft_port", bluestar_config.thruster_map[3], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##aft_port_rev_thruster", &bluestar_config.reverse_thrusters[3]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##aft_port_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[3]);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("Star Top (Right Top)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##star_top", bluestar_config.thruster_map[4], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##star_top_rev_thruster", &bluestar_config.reverse_thrusters[4]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##star_top_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[4]);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("Port Top (Left Top)");
                                ImGui::TableSetColumnIndex(1);
                                ImGui::InputText("##Port_top", bluestar_config.thruster_map[5], 64);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Checkbox("##port_top_rev_thruster", &bluestar_config.reverse_thrusters[5]);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Checkbox("##port_top_thruster_stronger_size_positive", &bluestar_config.stronger_side_positive[5]);

                                ImGui::EndTable();
                            }
                            
                            ImGui::Text("The thruster acceleration determines how fast thrusters ramp up to the commanded speed");

                            ImGui::Text("Thruster Acceleration");
                            ImGui::SameLine();
                            ImGui::SliderFloat("##thruster_acceleration", &bluestar_config.thruster_acceleration, 0.1f, 1.0f, "%.05f", ImGuiSliderFlags_AlwaysClamp);
                            
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

                            if (ImGui::TreeNode("Modify Preset Servo Angles"))
                            {   
                                if (ImGui::BeginTable("Servo Angle Config Table", 3, ImGuiTableFlags_Borders |
                                        ImGuiTableFlags_RowBg |
                                        ImGuiTableFlags_Resizable)) {
                                    ImGui::TableSetupColumn("Preset");
                                    ImGui::TableSetupColumn("Servo 1");
                                    ImGui::TableSetupColumn("Servo 2");
                                    ImGui::TableHeadersRow();
                                    
                                    auto& s1 = bluestar_config.servo_1_preset_angles;
                                    auto& s2 = bluestar_config.servo_2_preset_angles;

                                    size_t row_count = std::max(s1.size(), s2.size());

                                    for (size_t row = 0; row < row_count; ++row) {
                                        ImGui::TableNextRow();

                                        ImGui::TableSetColumnIndex(0);
                                        ImGui::Text("Preset Angle %ld", row + 1);

                                        ImGui::TableSetColumnIndex(1);
                                        if (row < s1.size()) {
                                            ImGui::SliderInt(
                                                (std::string("##preset_servo_1_angle_") + std::to_string(row + 1)).c_str(),
                                                &bluestar_config.servo_1_preset_angles[row],
                                                0, 255, "%d", ImGuiSliderFlags_AlwaysClamp
                                            );
                                        } else {
                                            ImGui::TextUnformatted("-");
                                        }

                                        ImGui::TableSetColumnIndex(2);
                                        if (row < s2.size()) {
                                            ImGui::SliderInt(
                                                (std::string("##preset_servo_2_angle_") + std::to_string(row + 1)).c_str(),
                                                &bluestar_config.servo_2_preset_angles[row],
                                                0, 255, "%d", ImGuiSliderFlags_AlwaysClamp
                                            );
                                        } else {
                                            ImGui::TextUnformatted("-");
                                        }
                                    }
                                    ImGui::EndTable();

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
                        ImGui::SeparatorText("Movement");
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
                        ImGui::Text("V - Fast Mode");
                        ImGui::SeparatorText("LEDs");
                        ImGui::Text("Z - Brighten LED 1");
                        ImGui::Text("X - Dim LED 1");
                        ImGui::Text("J - Brighten LED 2");
                        ImGui::Text("K - Dim LED 2");
                        ImGui::SeparatorText("DC Motors");
                        ImGui::Text("Numpad 1 - DC Motor 1 CW");
                        ImGui::Text("Numpad 2 - DC Motor 1 CCW");
                        ImGui::Text("Numpad 4 - DC Motor 2 CW");
                        ImGui::Text("Numpad 5 - DC Motor 2 CCW");
                        ImGui::SeparatorText("Servos");
                        ImGui::Text("Right Arrow - Turn Servo 1 Clockwise");
                        ImGui::Text("Left Arrow - Turn Servo 1 Counter-Clockwise");
                        ImGui::Text("Page Up - Turn Servo 2 Clockwise");
                        ImGui::Text("Page Down - Turn Servo 2 Counter-Clockwise");
                        ImGui::Text("Up Arrow - Turn Servo 3 Clockwise");
                        ImGui::Text("Down Arrow - Turn Servo 3 Counter-Clockwise");
                        ImGui::Text("Numpad 3 - Turn Servo 4 Clockwise");
                        ImGui::Text("Numpad 6 - Turn Servo 4 Counter-Clockwise");
                        ImGui::Text("5 - Use Servo Preset Angle 1");
                        ImGui::Text("6 - Use Servo Preset Angle 2");
                        ImGui::Text("7 - Use Servo Preset Angle 3");
                        ImGui::SeparatorText("Camera Controls");
                        ImGui::Text("I - Flip Camera 1 Vertically");
                        ImGui::Text("O - Flip Camera 2 Vertically");
                        ImGui::Text("P - Flip Camera 3 Vertically");
                        ImGui::Text("[ - Flip Camera 4 Vertically");
                        ImGui::Text("B - Flip Camera 1 Horizontally");
                        ImGui::Text("N - Flip Camera 2 Horizontally");
                        ImGui::Text("M - Flip Camera 3 Horizontally");
                        ImGui::Text(", - Flip Camera 4 Horizontally");
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

                    if (ImGui::BeginTable("Windows", 2, ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Window");
                        ImGui::TableSetupColumn("Show");
                        ImGui::TableHeadersRow();

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Camera Window");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Checkbox("##config_window_show_camera", &user_config.show_camera_window);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Co-Pilot Window");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Checkbox("##config_window_show_co_pilot", &user_config.show_co_pilot_window);
                        
                        ImGui::EndTable();
                    }

                    if (!glfwJoystickPresent(GLFW_JOYSTICK_1)) {
                        ImGui::Text("Controller must be connected");
                    } else if (ImGui::Button("Save")) {
                        json configJson;
                        configJson["name"] = user_config.name;

                        const char* controllerName = glfwGetJoystickName(GLFW_JOYSTICK_1);
                        const char* controllerGuid = glfwGetJoystickGUID(GLFW_JOYSTICK_1);

                        configJson["controller1"] = controllerName ? controllerName : "";
                        configJson["controller1_guid"] = controllerGuid ? controllerGuid : "";
                        
                        configJson["show_co_pilot_window"] = user_config.show_co_pilot_window;
                        configJson["show_camera_window"] = user_config.show_camera_window;

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

                if (ImGui::BeginTabItem("About GUI")) {
                    ImGui::Text("Eastern Edge");
                    ImGui::Spacing();
                    ImGui::Text("BlueStar GUI");
                    ImGui::Text("MATE ROV 2026");

                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::Text("Camera Backend:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted(cameraStreamBackendName());

                    ImGui::Text("OpenGL Version:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted((version ? reinterpret_cast<const char*>(version) : "null"));

                    ImGui::Text("OpenGL Renderer:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted((renderer ? reinterpret_cast<const char*>(renderer) : "null"));
                    
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

    cam1.stop();
    cam2.stop();
    cam3.stop();
    cam4.stop();

    glfwTerminate();

    rclcpp::shutdown();
    return 0;
}

void saveGlobalConfig(std::shared_ptr<SaveConfigPublisher> saveConfigNode, const BlueStarConfig& bluestar_config) {
    json configJson;
    configJson["name"] = "bluestar_config";
    configJson["thruster_acceleration"] = bluestar_config.thruster_acceleration;
    configJson["thruster_stronger_side_attenuation_constant"] = bluestar_config.thruster_stronger_side_attenuation_constant;

    for (size_t i = 0; i < std::size(bluestar_config.thruster_map); i++) {
        configJson["thruster_map"][i] = std::stoi(bluestar_config.thruster_map[i]);
        configJson["reverse_thrusters"][i] = bluestar_config.reverse_thrusters[i];
        configJson["stronger_side_positive"][i] = bluestar_config.stronger_side_positive[i];
    }

    for (size_t i = 0; i < std::size(bluestar_config.servo_1_preset_angles); i++) {
        configJson["servo_1_preset_angles"][i] = bluestar_config.servo_1_preset_angles[i];
    }

    for (size_t i = 0; i < std::size(bluestar_config.servo_2_preset_angles); i++) {
        configJson["servo_2_preset_angles"][i] = bluestar_config.servo_2_preset_angles[i];
    }

    configJson["cam1ip"] = bluestar_config.cam1ip;
    configJson["cam2ip"] = bluestar_config.cam2ip;
    configJson["cam3ip"] = bluestar_config.cam3ip;
    configJson["cam4ip"] = bluestar_config.cam4ip;

    configJson["cam1_video_caps"] = bluestar_config.cam1_video_caps;
    configJson["cam1_audio_caps"] = bluestar_config.cam1_audio_caps;

    configJson["cam2_video_caps"] = bluestar_config.cam2_video_caps;
    configJson["cam2_audio_caps"] = bluestar_config.cam2_audio_caps;

    configJson["cam3_video_caps"] = bluestar_config.cam3_video_caps;
    configJson["cam3_audio_caps"] = bluestar_config.cam3_audio_caps;

    configJson["cam4_video_caps"] = bluestar_config.cam4_video_caps;
    configJson["cam4_audio_caps"] = bluestar_config.cam4_audio_caps;

    saveConfigNode->saveConfig("bluestar_config", configJson.dump());
}