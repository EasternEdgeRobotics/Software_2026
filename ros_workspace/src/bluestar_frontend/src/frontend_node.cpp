/**
 * Bluestar Frontend Node
 * 
 * Main entry point for the pilot GUI application.
 * Initializes ImGui/GLFW, loads configs, and runs the main render loop.
 */

#include <iostream>
#include <memory>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "rclcpp/rclcpp.hpp"

#include "bluestar_frontend/core/RovState.hpp"
#include "bluestar_frontend/core/ConfigLoader.hpp"
#include "bluestar_frontend/core/ROS.hpp"
#include "bluestar_frontend/gui/InputManager.hpp"
#include "bluestar_frontend/gui/UIRenderer.hpp"

using namespace bluestar;

namespace {

/**
 * Initialize GLFW and create a window.
 * Returns nullptr on failure.
 */
GLFWwindow* initializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "[Frontend] Failed to initialize GLFW" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge Bluestar GUI", nullptr, nullptr);
    if (!window) {
        std::cerr << "[Frontend] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    return window;
}

/**
 * Initialize ImGui with GLFW and OpenGL backends.
 */
void initializeImGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

/**
 * Clean up ImGui resources.
 */
void shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

/**
 * Load bluestar_config from fetched configs.
 */
BluestarConfig loadBluestarFromConfigs(
    const std::vector<std::pair<std::string, std::string>>& configs,
    RovState& state
) {
    for (const auto& [name, json_data] : configs) {
        if (name == "bluestar_config") {
            auto config = loadBluestarConfig(json_data);
            
            // Apply loaded firmware parameters to state
            state.thruster_config.acceleration = config.thruster_acceleration;
            state.thruster_config.timeout_ms = config.thruster_timeout_ms;
            state.thruster_config.set_acceleration = true;
            state.thruster_config.set_timeout = true;

            // Apply PCDCM parameters
            for (std::size_t i = 0; i < 2; ++i) {
                state.pcdcms.associated_motor[i] = config.pcdcm_motor_numbers[i];
                state.pcdcms.loop_period_ms[i] = config.pcdcm_loop_period_ms[i];
                state.pcdcms.proportional_gain[i] = config.pcdcm_proportional_gain[i];
                state.pcdcms.integral_gain[i] = config.pcdcm_integral_gain[i];
                state.pcdcms.derivative_gain[i] = config.pcdcm_derivative_gain[i];
                state.pcdcms.set_parameters[i] = true;
            }

            return config;
        }
    }
    return BluestarConfig{};
}

} // anonymous namespace

int main(int argc, char** argv) {
    // Initialize GLFW
    GLFWwindow* window = initializeGLFW();
    if (!window) {
        return -1;
    }

    // Initialize ImGui
    initializeImGui(window);

    // Initialize ROS2
    rclcpp::init(argc, argv);
    ROS ros;

    // Initialize state
    RovState state;
    state.fast_mode_power = {75, 75, 50, 75, 0, 30}; // Default fast mode settings

    // Fetch configs from config_manager
    auto configs = ros.fetchConfigs();
    BluestarConfig bluestar_config = loadBluestarFromConfigs(configs, state);
    UserConfig user_config;

    // Initialize input manager
    InputManager input_manager;

    // Initialize UI renderer with save callback
    UIRenderer ui_renderer([&ros](const std::string& name, const std::string& data) {
        ros.saveConfig(name, data);
    });
    ui_renderer.setAvailableConfigs(configs);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Reset transient commands from previous frame
        state.resetTransientCommands();

        // Process input (only if controller connected or keyboard mode)
        if (glfwJoystickPresent(GLFW_JOYSTICK_1) || state.modes.keyboard_mode) {
            input_manager.processInput(window, state, user_config, bluestar_config);
        }

        // Render UI
        ui_renderer.render(window, state, user_config, bluestar_config);

        // Publish ROS message
        ros.publishPilotInput(state);
        ros.spinOnce();

        // Render
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    shutdownImGui();
    glfwTerminate();
    rclcpp::shutdown();

    return 0;
}
