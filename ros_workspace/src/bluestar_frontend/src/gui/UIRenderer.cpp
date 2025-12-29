#include "bluestar_frontend/gui/UIRenderer.hpp"
#include "bluestar_frontend/PilotActions.hpp"

#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace bluestar {

UIRenderer::UIRenderer(SaveConfigCallback save_callback)
    : save_callback_(std::move(save_callback)) {};

void UIRenderer::setAvailableConfigs(std::vector<std::pair<std::string, std::string>> configs) {
    available_configs_ = std::move(configs);
}

void UIRenderer::render(
    GLFWwindow* window,
    RovState& state,
    UserConfig& user_config,
    BluestarConfig& bluestar_config
) {
    renderMenuBar(state, user_config);
    
    if (show_config_window_) {
        renderConfigWindow(window, state, user_config, bluestar_config);
    }
    
    if (show_pilot_window_) {
        renderPilotWindow(state);
    }
}

void UIRenderer::renderMenuBar(RovState& state, UserConfig& user_config) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Config")) {
            if (ImGui::MenuItem("Open Config Editor")) {
                show_config_window_ = true;
            }
            if (ImGui::BeginMenu("Load UserConfig")) {
                for (const auto& [name, json_data] : available_configs_) {
                    if (name == "bluestar_config") {
                        continue;
                    }
                    if (ImGui::MenuItem(name.c_str())) {
                        auto loaded = loadUserConfig(json_data, name);
                        if (loaded) {
                            user_config = *loaded;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Pilot")) {
            if (ImGui::MenuItem("Open Piloting Menu")) {
                show_pilot_window_ = true;
            }
            ImGui::EndMenu();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, state.modes.fast_mode ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
        ImGui::Text(state.modes.fast_mode ? " FAST MODE ON" : "FAST MODE OFF");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, state.modes.invert_controls ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
        ImGui::Text(state.modes.invert_controls ? "INVERT CONTROLS ON" : "INVERT CONTROLS OFF");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::Checkbox("Keyboard Mode", &state.modes.keyboard_mode);

        // FPS counter
        ImGui::SameLine(ImGui::GetWindowWidth() - 100);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        ImGui::EndMainMenuBar();
    }
}

void UIRenderer::renderConfigWindow(
    GLFWwindow* window,
    RovState& state,
    UserConfig& user_config,
    BluestarConfig& bluestar_config
) {
    ImGui::Begin("Config Editor", &show_config_window_);
    
    if (ImGui::BeginTabBar("Config Tabs")) {
        if (ImGui::BeginTabItem("Bluestar Config")) {
            renderBluestarConfigTab(window, state, bluestar_config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls (User)")) {
            renderControlsTab(window, user_config, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Save User Config")) {
            renderSaveUserConfigTab(window, user_config);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void UIRenderer::renderBluestarConfigTab(
    GLFWwindow* window,
    RovState& state,
    BluestarConfig& bluestar_config
) {
    if (!(state.modes.keyboard_mode || glfwJoystickPresent(GLFW_JOYSTICK_1))) {
        ImGui::Text("Connect controller or enable keyboard mode to configure Bluestar");
        return;
    }

    // Sync checkbox with state
    config_mode_checkbox_ = state.thruster_config.enabled;
    
    ImGui::Checkbox("Thruster Configuration Mode (Toggle to Save)", &config_mode_checkbox_);
    
    // On negative edge, save config
    if (!config_mode_checkbox_ && state.thruster_config.enabled) {
        if (save_callback_) {
            save_callback_("bluestar_config", saveBluestarConfig(bluestar_config));
        }
    }
    state.thruster_config.enabled = config_mode_checkbox_;

    if (config_mode_checkbox_) {
        const char* thruster_names[] = {
            "For Star (Forward Right)",
            "For Port (Forward Left)",
            "Aft Star (Back Right)",
            "Aft Port (Back Left)",
            "Star Top (Right Top)",
            "Port Top (Left Top)"
        };

        for (std::size_t i = 0; i < 6; ++i) {
            ImGui::Text("%s", thruster_names[i]);
            ImGui::SameLine();
            
            char label[32];
            std::snprintf(label, sizeof(label), "##thruster_map_%zu", i);
            int map_value = bluestar_config.thruster_map[i];
            if (ImGui::SliderInt(label, &map_value, 0, static_cast<int>(bluestar_config.thruster_map.size()) - 1, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                bluestar_config.thruster_map[i] = map_value;
            }
            
            ImGui::SameLine();
            std::snprintf(label, sizeof(label), "Reverse##rev_%zu", i);
            ImGui::Checkbox(label, &bluestar_config.reverse_thrusters[i]);
            
            ImGui::SameLine();
            std::snprintf(label, sizeof(label), "Stronger Side Positive##ssp_%zu", i);
            ImGui::Checkbox(label, &bluestar_config.stronger_side_positive[i]);
        }

        ImGui::Text("The thruster acceleration determines how fast thrusters ramp up to the commanded speed");

        // Note: The thruster_acceleration and thruster_timeout_ms require `set` buttons because they need to be
        // explicitly send down to the firmware

        int accel_temp = static_cast<int>(bluestar_config.thruster_acceleration);
        ImGui::Text("Thruster Acceleration");
        ImGui::SameLine();
        if (ImGui::SliderInt("##thruster_acceleration", &accel_temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            bluestar_config.thruster_acceleration = static_cast<uint8_t>(accel_temp);
        }
        if (ImGui::Button("Set Thruster Acceleration")) {
            state.thruster_config.set_acceleration = true;
            state.thruster_config.acceleration = bluestar_config.thruster_acceleration;
        }

        int timeout_temp = static_cast<int>(bluestar_config.thruster_timeout_ms);
        ImGui::Text("Set Thruster Timeout (ms)");
        ImGui::SameLine();
        if (ImGui::SliderInt("##thruster_timeout", &timeout_temp, 0, 65535, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            bluestar_config.thruster_timeout_ms = static_cast<uint16_t>(timeout_temp);
        }
        if (ImGui::Button("Set Thruster Timeout")) {
            state.thruster_config.set_timeout = true;
            state.thruster_config.timeout_ms = bluestar_config.thruster_timeout_ms;
        }

        ImGui::Text("The stronger side attenuation constant attenuates the power of the thruster on a given side");
        ImGui::Text("Thruster Stronger Side Attenuation Constant");
        ImGui::SameLine();
        ImGui::SliderFloat("##attenuation", &bluestar_config.thruster_stronger_side_attenuation_constant, 0.0f, 2.0f, "%.005f", ImGuiSliderFlags_AlwaysClamp);

        const char* thruster_numbers[] = {"0", "1", "2", "3", "4", "5"};
        ImGui::Text("Configuration Mode Thruster Number");
        ImGui::SameLine();
        if (ImGui::Combo("##thruster_number", &selected_thruster_, thruster_numbers, IM_ARRAYSIZE(thruster_numbers))) {
            state.thruster_config.selected_thruster = static_cast<uint8_t>(selected_thruster_);
        }

        if (ImGui::TreeNode("Modify Preset Servo and PCDCM Angles")) {
            for (std::size_t i = 0; i < bluestar_config.preset_servo_angles.size(); ++i) {
                for (std::size_t j = 0; j < bluestar_config.preset_servo_angles[i].size(); ++j) {
                    ImGui::Text("Servo %zu Preset Angle %zu", i + 1, j + 1);
                    ImGui::SameLine();
                    int temp = static_cast<int>(bluestar_config.preset_servo_angles[i][j]);
                    char label[64];
                    std::snprintf(label, sizeof(label), "##servo_%zu_preset_%zu", i + 1, j + 1);
                    if (ImGui::SliderInt(label, &temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                        bluestar_config.preset_servo_angles[i][j] = static_cast<uint8_t>(temp);
                    }
                }
            }
            for (std::size_t i = 0; i < 2; ++i) {
                for (std::size_t j = 0; j < 3; ++j) {
                    ImGui::Text("PCDCM %zu Preset Angle %zu", i + 1, j + 1);
                    ImGui::SameLine();
                    int temp = static_cast<int>(bluestar_config.preset_pcdcm_angles[i][j]);
                    char label[64];
                    std::snprintf(label, sizeof(label), "##pcdcm_%zu_preset_%zu", i + 1, j + 1);
                    if (ImGui::SliderInt(label, &temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                        bluestar_config.preset_pcdcm_angles[i][j] = static_cast<uint8_t>(temp);
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    if (ImGui::TreeNode("Modify Preset DC Motor Speeds")) {
        for (std::size_t i = 0; i < bluestar_config.dc_motor_speeds.size(); ++i) {
            ImGui::Text("DC Motor %zu Preset Speed", i + 1);
            ImGui::SameLine();
            int temp = static_cast<int>(bluestar_config.dc_motor_speeds[i]);
            char label[64];
            std::snprintf(label, sizeof(label), "##dcm_%zu_speed", i + 1);
            if (ImGui::SliderInt(label, &temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                bluestar_config.dc_motor_speeds[i] = static_cast<uint8_t>(temp);
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Modify PCDCM Parameters")) {
        ImGui::Text("Choose Associated DC Motor Numbers (0-3)");
        for (std::size_t i = 0; i < state.pcdcms.associated_motor.size(); ++i) {
            ImGui::Text("Motor %zu", i + 1);
            ImGui::SameLine();
            int temp = static_cast<int>(state.pcdcms.associated_motor[i]);
            char label[64];
            std::snprintf(label, sizeof(label), "##pcdcm_%zu_motor", i + 1);
            if (ImGui::SliderInt(label, &temp, 0, 3, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                state.pcdcms.associated_motor[i] = static_cast<uint8_t>(temp);
            }
        }

        ImGui::Text("Precision Control Loop Period (ms)");
        for (std::size_t i = 0; i < state.pcdcms.associated_motor.size(); ++i) {
            ImGui::SameLine();
            int temp = static_cast<int>(state.pcdcms.loop_period_ms[i]);
            char label[64];
            std::snprintf(label, sizeof(label), "##loop_period_%zu", i + 1);
            if (ImGui::SliderInt(label, &temp, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                state.pcdcms.loop_period_ms[i] = static_cast<uint8_t>(temp);
            }
        }

        ImGui::Text("Precision Control Proportional Gain");
        for (std::size_t i = 0; i < state.pcdcms.associated_motor.size(); ++i) {
            ImGui::SameLine();
            char label[64];
            std::snprintf(label, sizeof(label), "##p_gain_%zu", i + 1);
            ImGui::SliderFloat(label, &state.pcdcms.proportional_gain[i], 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::Text("Precision Control Integral Gain");
        for (std::size_t i = 0; i < state.pcdcms.associated_motor.size(); ++i) {
            ImGui::SameLine();
            char label[64];
            std::snprintf(label, sizeof(label), "##i_gain_%zu", i + 1);
            ImGui::SliderFloat(label, &state.pcdcms.integral_gain[i], 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::Text("Precision Control Derivative Gain");
        for (std::size_t i = 0; i < state.pcdcms.associated_motor.size(); ++i) {
            ImGui::SameLine();
            char label[64];
            std::snprintf(label, sizeof(label), "##d_gain_%zu", i + 1);
            ImGui::SliderFloat(label, &state.pcdcms.derivative_gain[i], 0.0f, 10.0f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::TreePop();
    }

    for (std::size_t i = 0; i < 2; ++i) {
        char label[64];
        std::snprintf(label, sizeof(label), "Set PCDCM Parameters for PCDCM %zu", i + 1);
        if (ImGui::Button(label)) {
            state.pcdcms.set_parameters[i] = true;
        }
    }
}

void UIRenderer::renderControlsTab(GLFWwindow* window, UserConfig& user_config, const RovState& state) {
    if (state.modes.keyboard_mode) {
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
        ImGui::Text("N - Turn All PCDCMs Clockwise");
        ImGui::Text("M - Turn All PCDCMs Counter-Clockwise");
        ImGui::Text("V - Toggle Fast Mode");
        ImGui::Text("SPACE - Toggle Invert Controls");
        ImGui::Text("O - Spin DC Motors Forward At the Preset Speeds");
        ImGui::Text("P - Spin DC Motors Reverse At the Preset Speeds");
        ImGui::Text("1 - Set power to 0%%");
        ImGui::Text("2 - Set power to 50%%");
        ImGui::Text("3 - Set power to 0%%");
        ImGui::Text("4 - Set power to 50%%, Heave to 75%%, and Yaw to 30%%");
        ImGui::Text("5 - Use Servo Preset Angle 1 For All Servos");
        ImGui::Text("6 - Use Servo Preset Angle 2 For All Servos");
        ImGui::Text("7 - Use Servo Preset Angle 3 For All Servos");
        ImGui::Text("8 - Use PCDCM Preset Angle 1 For All PCDCMs");
        ImGui::Text("9 - Use PCDCM Preset Angle 2 For All PCDCMs");
        ImGui::Text("0 - Use PCDCM Preset Angle 3 For All PCDCMs");
    }

    if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
        int button_count = 0;
        int axis_count = 0;
        const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &button_count);
        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);

        ImGui::Text("Connected Controller: %s", glfwGetJoystickName(GLFW_JOYSTICK_1));
        ImGui::Text("Axis Deadzone");
        ImGui::SameLine();
        ImGui::SliderFloat("##deadzone", &user_config.deadzone, 0.f, 1.f);

        ImGui::SeparatorText("Buttons");
        for (int i = 0; i < button_count && i < static_cast<int>(user_config.button_actions.size()); ++i) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, buttons[i] ? 0 : 1, 1, 1));
            ImGui::Text("Button %d", i);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            
            char label[32];
            std::snprintf(label, sizeof(label), "##button%d", i);
            int current = static_cast<int>(user_config.button_actions[i]);
            if (ImGui::Combo(label, &current, BUTTON_ACTION_LABELS, static_cast<int>(ButtonAction::COUNT))) {
                user_config.button_actions[i] = static_cast<ButtonAction>(current);
            }
        }

        ImGui::SeparatorText("Axes");
        for (int i = 0; i < axis_count && i < static_cast<int>(user_config.axis_actions.size()); ++i) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1 - std::abs(axes[i]), 1, 1));
            ImGui::Text("Axis %d", i);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            
            char label[32];
            std::snprintf(label, sizeof(label), "##axis%d", i);
            int current = static_cast<int>(user_config.axis_actions[i]);
            if (ImGui::Combo(label, &current, AXIS_ACTION_LABELS, static_cast<int>(AxisAction::COUNT))) {
                user_config.axis_actions[i] = static_cast<AxisAction>(current);
            }
        }
    } else {
        ImGui::Text("No Controller Connected.");
    }
}

void UIRenderer::renderSaveUserConfigTab(GLFWwindow* window, UserConfig& user_config) {
    static char name_buffer[64] = "";
    
    // Sync buffer with config name
    if (name_buffer[0] == '\0' && !user_config.name.empty()) {
        std::strncpy(name_buffer, user_config.name.c_str(), sizeof(name_buffer) - 1);
    }

    ImGui::Text("User Config Name");
    ImGui::SameLine();
    ImGui::InputText("##configName", name_buffer, sizeof(name_buffer));

    if (!glfwJoystickPresent(GLFW_JOYSTICK_1)) {
        ImGui::Text("Controller must be connected");
    } else if (ImGui::Button("Save")) {
        if (name_buffer == "") {
            std::string time_str = std::to_string(std::time(nullptr));
            std::strncpy(name_buffer, time_str.c_str(), sizeof(name_buffer) - 1);
            name_buffer[sizeof(name_buffer) - 1] = '\0'; // Re-add null-termination
        }
        user_config.name = name_buffer;
        if (save_callback_) {
            save_callback_(user_config.name, saveUserConfig(user_config));
        }
    }
}

void UIRenderer::renderPilotWindow(RovState& state) {
    ImGui::Begin("Co-Pilot Window", &show_pilot_window_);

    ImGui::SliderInt("Power", &state.power.power, 0, 100);
    ImGui::SliderInt("Surge", &state.power.surge, 0, 100);
    ImGui::SliderInt("Sway", &state.power.sway, 0, 100);
    ImGui::SliderInt("Heave", &state.power.heave, 0, 100);
    ImGui::SliderInt("Roll", &state.power.roll, 0, 100);
    ImGui::SliderInt("Yaw", &state.power.yaw, 0, 100);

    ImGui::SeparatorText("Keybinds");
    ImGui::Text("1 - Set all to 0%%");
    ImGui::Text("2 - Set all to 50%%");
    ImGui::Text("3 - Set all to 0%%, set Heave and Power to 100%%");
    ImGui::Text("4 - Set all to 50%%, Heave to 75%%, and Yaw to 30%%");
    ImGui::Text("V (Toggle) - Fast mode");

    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        state.power = {0, 0, 0, 0, 0, 0};
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        state.power = {50, 50, 50, 50, 50, 50};
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        state.power = {100, 0, 0, 100, 0, 0};
    }
    if (ImGui::IsKeyPressed(ImGuiKey_4)) {
        state.power = {50, 50, 50, 75, 50, 30};
    }

    ImGui::SeparatorText("Fast Mode Adjustment");
    ImGui::SliderInt("Fast Mode Power", &state.fast_mode_power.power, 0, 100);
    ImGui::SliderInt("Fast Mode Surge", &state.fast_mode_power.surge, 0, 100);
    ImGui::SliderInt("Fast Mode Sway", &state.fast_mode_power.sway, 0, 100);
    ImGui::SliderInt("Fast Mode Heave", &state.fast_mode_power.heave, 0, 100);
    ImGui::SliderInt("Fast Mode Roll", &state.fast_mode_power.roll, 0, 100);
    ImGui::SliderInt("Fast Mode Yaw", &state.fast_mode_power.yaw, 0, 100);

    ImGui::End();
}

} // namespace bluestar
