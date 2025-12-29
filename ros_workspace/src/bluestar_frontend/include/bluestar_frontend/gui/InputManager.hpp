#ifndef BLUESTAR_FRONTEND_GUI_INPUT_MANAGER_HPP
#define BLUESTAR_FRONTEND_GUI_INPUT_MANAGER_HPP

#include <array>

#include <GLFW/glfw3.h>

#include "bluestar_frontend/PilotActions.hpp"
#include "bluestar_frontend/core/RovState.hpp"
#include "bluestar_frontend/core/ConfigLoader.hpp"

namespace bluestar {

/**
 * Handles keyboard and controller input, translating them into ROV state changes.
 * 
 * This class is part of the GUI layer (not reusable by autonomy nodes).
 * It directly mutates the provided RovState based on input.
 */
class InputManager {
public:
    InputManager();

    /**
     * Process input from keyboard and/or controller.
     * Call once per frame before rendering.
     * 
     * @param window GLFW window handle for input polling
     * @param state ROV state to mutate based on input
     * @param user_config User config containing controller mappings
     * @param bluestar_config Bluestar config for preset values
     */
    void processInput(
        GLFWwindow* window,
        RovState& state,
        UserConfig& user_config,
        const BluestarConfig& bluestar_config
    );

    /**
     * Check if a joystick/controller is connected.
     */
    bool isControllerConnected() const;

private:
    // Process keyboard input when in keyboard mode
    void processKeyboard(GLFWwindow* window, RovState& state, const BluestarConfig& bluestar_config);

    // Process controller buttons
    void processControllerButtons(
        const unsigned char* buttons,
        int button_count,
        RovState& state,
        UserConfig& user_config,
        const BluestarConfig& bluestar_config
    );

    // Process controller analog axes
    void processControllerAxes(
        const float* axes,
        int axis_count,
        RovState& state,
        const UserConfig& user_config
    );

    // Apply a button action to the state
    void applyButtonAction(ButtonAction action, RovState& state, const BluestarConfig& bluestar_config);

    // Apply an axis action to the state (returns motion value -100 to +100)
    void applyAxisAction(AxisAction action, float axis_value, float deadzone, RovState& state);

    // Handle mode toggles with latching (single-fire per press)
    void handleModeToggles(RovState& state);

    // Handle latched commands (single-fire for servos, pcdcms, leds)
    void applyLatchedCommands(RovState& state);

    // Latch state for toggle buttons (prevents repeated firing while held)
    bool fast_mode_latch_ = false;
    bool invert_controls_latch_ = false;
    bool config_mode_latch_ = false;

    // Latch state for LEDs
    std::array<bool, 2> brighten_led_latch_ = {false, false};
    std::array<bool, 2> dim_led_latch_ = {false, false};

    // Latch state for servos
    std::array<bool, 4> turn_servo_cw_latch_ = {false, false, false, false};
    std::array<bool, 4> turn_servo_ccw_latch_ = {false, false, false, false};

    // Latch state for PCDCMs
    std::array<bool, 2> turn_pcdcm_cw_latch_ = {false, false};
    std::array<bool, 2> turn_pcdcm_ccw_latch_ = {false, false};

    // Pending toggle requests (set by input, processed after)
    bool pending_fast_mode_toggle_ = false;
    bool pending_invert_toggle_ = false;
    bool pending_config_mode_toggle_ = false;
};

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_GUI_INPUT_MANAGER_HPP
