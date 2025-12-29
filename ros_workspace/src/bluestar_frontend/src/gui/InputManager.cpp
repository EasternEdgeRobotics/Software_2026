#include "bluestar_frontend/gui/InputManager.hpp"

#include <cmath>

namespace bluestar {

InputManager::InputManager() = default;

void InputManager::processInput(
    GLFWwindow* window,
    RovState& state,
    UserConfig& user_config,
    const BluestarConfig& bluestar_config
) {
    // Reset pending toggles
    pending_fast_mode_toggle_ = false;
    pending_invert_toggle_ = false;
    pending_config_mode_toggle_ = false;

    // Process keyboard if in keyboard mode
    if (state.modes.keyboard_mode) {
        processKeyboard(window, state, bluestar_config);
    }

    // Process controller if connected
    if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
        int button_count = 0;
        int axis_count = 0;
        const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &button_count);
        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);

        // Initialize user config vectors if needed
        if (button_count > 0 && user_config.button_actions.empty()) {
            user_config.button_actions.resize(button_count, ButtonAction::NONE);
        }
        if (axis_count > 0 && user_config.axis_actions.empty()) {
            user_config.axis_actions.resize(axis_count, AxisAction::NONE);
        }

        processControllerButtons(buttons, button_count, state, user_config, bluestar_config);
        processControllerAxes(axes, axis_count, state, user_config);
    }

    // Handle mode toggles with latching
    handleModeToggles(state);

    // Apply latched commands
    applyLatchedCommands(state);

    // Apply invert controls if active
    if (state.modes.invert_controls) {
        state.motion.surge = -state.motion.surge;
        state.motion.yaw = -state.motion.yaw;
    }
}

bool InputManager::isControllerConnected() const {
    return glfwJoystickPresent(GLFW_JOYSTICK_1) != 0;
}

void InputManager::processKeyboard(GLFWwindow* window, RovState& state, const BluestarConfig& bluestar_config) {
    // Motion controls
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.motion.surge += 100;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.motion.surge -= 100;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.motion.sway -= 100;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.motion.sway += 100;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) state.motion.yaw -= 100;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) state.motion.yaw += 100;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) state.motion.roll += 100;
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) state.motion.roll -= 100;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) state.motion.heave += 100;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) state.motion.heave -= 100;

    // LED controls (all LEDs together)
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        state.leds.brighten = {true, true};
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        state.leds.dim = {true, true};
    }

    // Servo controls (all servos together)
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        state.servos.turn_cw = {true, true, true, true};
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        state.servos.turn_ccw = {true, true, true, true};
    }

    // PCDCM controls (all together)
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
        state.pcdcms.turn_cw = {true, true};
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        state.pcdcms.turn_ccw = {true, true};
    }

    // Mode toggles
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        pending_fast_mode_toggle_ = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        pending_invert_toggle_ = true;
    }

    // DC motor controls (all motors together)
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        state.dc_motors.speed = bluestar_config.dc_motor_speeds;
        state.dc_motors.reverse = {false, false, false, false};
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        state.dc_motors.speed = bluestar_config.dc_motor_speeds;
        state.dc_motors.reverse = {true, true, true, true};
    }

    // Servo presets (keys 5, 6, 7)
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        state.servos.set_angle = {true, true, true, true};
        for (std::size_t i = 0; i < 4; ++i) {
            state.servos.angle[i] = bluestar_config.preset_servo_angles[i][0];
        }
    }
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
        state.servos.set_angle = {true, true, true, true};
        for (std::size_t i = 0; i < 4; ++i) {
            state.servos.angle[i] = bluestar_config.preset_servo_angles[i][1];
        }
    }
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
        state.servos.set_angle = {true, true, true, true};
        for (std::size_t i = 0; i < 4; ++i) {
            state.servos.angle[i] = bluestar_config.preset_servo_angles[i][2];
        }
    }

    // PCDCM presets (keys 8, 9, 0)
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
        state.pcdcms.set_angle = {true, true};
        for (std::size_t i = 0; i < 2; ++i) {
            state.pcdcms.angle[i] = bluestar_config.preset_pcdcm_angles[i][0];
        }
    }
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
        state.pcdcms.set_angle = {true, true};
        for (std::size_t i = 0; i < 2; ++i) {
            state.pcdcms.angle[i] = bluestar_config.preset_pcdcm_angles[i][1];
        }
    }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        state.pcdcms.set_angle = {true, true};
        for (std::size_t i = 0; i < 2; ++i) {
            state.pcdcms.angle[i] = bluestar_config.preset_pcdcm_angles[i][2];
        }
    }
}

void InputManager::processControllerButtons(
    const unsigned char* buttons,
    int button_count,
    RovState& state,
    UserConfig& user_config,
    const BluestarConfig& bluestar_config
) {
    for (int i = 0; i < button_count && i < static_cast<int>(user_config.button_actions.size()); ++i) {
        if (buttons[i]) {
            applyButtonAction(user_config.button_actions[i], state, bluestar_config);
        }
    }
}

void InputManager::processControllerAxes(
    const float* axes,
    int axis_count,
    RovState& state,
    const UserConfig& user_config
) {
    for (int i = 0; i < axis_count && i < static_cast<int>(user_config.axis_actions.size()); ++i) {
        applyAxisAction(user_config.axis_actions[i], axes[i], user_config.deadzone, state);
    }
}

void InputManager::applyButtonAction(ButtonAction action, RovState& state, const BluestarConfig& bluestar_config) {
    switch (action) {
        // Motion (digital - full magnitude)
        case ButtonAction::SURGE_FORWARD: state.motion.surge += 100; break;
        case ButtonAction::SURGE_BACKWARD: state.motion.surge -= 100; break;
        case ButtonAction::SWAY_LEFT: state.motion.sway -= 100; break;
        case ButtonAction::SWAY_RIGHT: state.motion.sway += 100; break;
        case ButtonAction::YAW_LEFT: state.motion.yaw -= 100; break;
        case ButtonAction::YAW_RIGHT: state.motion.yaw += 100; break;
        case ButtonAction::HEAVE_UP: state.motion.heave += 100; break;
        case ButtonAction::HEAVE_DOWN: state.motion.heave -= 100; break;
        case ButtonAction::ROLL_CW: state.motion.roll += 100; break;
        case ButtonAction::ROLL_CCW: state.motion.roll -= 100; break;

        // Mode toggles
        case ButtonAction::CONFIGURATION_MODE: pending_config_mode_toggle_ = true; break;
        case ButtonAction::FAST_MODE: pending_fast_mode_toggle_ = true; break;
        case ButtonAction::INVERT_CONTROLS: pending_invert_toggle_ = true; break;

        // LEDs
        case ButtonAction::BRIGHTEN_LED0: state.leds.brighten[0] = true; break;
        case ButtonAction::DIM_LED0: state.leds.dim[0] = true; break;
        case ButtonAction::BRIGHTEN_LED1: state.leds.brighten[1] = true; break;
        case ButtonAction::DIM_LED1: state.leds.dim[1] = true; break;

        // Servos - turn
        case ButtonAction::TURN_SERVO0_CW: state.servos.turn_cw[0] = true; break;
        case ButtonAction::TURN_SERVO0_CCW: state.servos.turn_ccw[0] = true; break;
        case ButtonAction::TURN_SERVO1_CW: state.servos.turn_cw[1] = true; break;
        case ButtonAction::TURN_SERVO1_CCW: state.servos.turn_ccw[1] = true; break;
        case ButtonAction::TURN_SERVO2_CW: state.servos.turn_cw[2] = true; break;
        case ButtonAction::TURN_SERVO2_CCW: state.servos.turn_ccw[2] = true; break;
        case ButtonAction::TURN_SERVO3_CW: state.servos.turn_cw[3] = true; break;
        case ButtonAction::TURN_SERVO3_CCW: state.servos.turn_ccw[3] = true; break;

        // Servos - presets
        case ButtonAction::USE_SERVO0_ANGLE_PRESET_0:
            state.servos.set_angle[0] = true;
            state.servos.angle[0] = bluestar_config.preset_servo_angles[0][0];
            break;
        case ButtonAction::USE_SERVO0_ANGLE_PRESET_1:
            state.servos.set_angle[0] = true;
            state.servos.angle[0] = bluestar_config.preset_servo_angles[0][1];
            break;
        case ButtonAction::USE_SERVO0_ANGLE_PRESET_2:
            state.servos.set_angle[0] = true;
            state.servos.angle[0] = bluestar_config.preset_servo_angles[0][2];
            break;
        case ButtonAction::USE_SERVO1_ANGLE_PRESET_0:
            state.servos.set_angle[1] = true;
            state.servos.angle[1] = bluestar_config.preset_servo_angles[1][0];
            break;
        case ButtonAction::USE_SERVO1_ANGLE_PRESET_1:
            state.servos.set_angle[1] = true;
            state.servos.angle[1] = bluestar_config.preset_servo_angles[1][1];
            break;
        case ButtonAction::USE_SERVO1_ANGLE_PRESET_2:
            state.servos.set_angle[1] = true;
            state.servos.angle[1] = bluestar_config.preset_servo_angles[1][2];
            break;
        case ButtonAction::USE_SERVO2_ANGLE_PRESET_0:
            state.servos.set_angle[2] = true;
            state.servos.angle[2] = bluestar_config.preset_servo_angles[2][0];
            break;
        case ButtonAction::USE_SERVO2_ANGLE_PRESET_1:
            state.servos.set_angle[2] = true;
            state.servos.angle[2] = bluestar_config.preset_servo_angles[2][1];
            break;
        case ButtonAction::USE_SERVO2_ANGLE_PRESET_2:
            state.servos.set_angle[2] = true;
            state.servos.angle[2] = bluestar_config.preset_servo_angles[2][2];
            break;
        case ButtonAction::USE_SERVO3_ANGLE_PRESET_0:
            state.servos.set_angle[3] = true;
            state.servos.angle[3] = bluestar_config.preset_servo_angles[3][0];
            break;
        case ButtonAction::USE_SERVO3_ANGLE_PRESET_1:
            state.servos.set_angle[3] = true;
            state.servos.angle[3] = bluestar_config.preset_servo_angles[3][1];
            break;
        case ButtonAction::USE_SERVO3_ANGLE_PRESET_2:
            state.servos.set_angle[3] = true;
            state.servos.angle[3] = bluestar_config.preset_servo_angles[3][2];
            break;

        // PCDCMs - turn
        case ButtonAction::TURN_PCDCM0_CW: state.pcdcms.turn_cw[0] = true; break;
        case ButtonAction::TURN_PCDCM0_CCW: state.pcdcms.turn_ccw[0] = true; break;
        case ButtonAction::TURN_PCDCM1_CW: state.pcdcms.turn_cw[1] = true; break;
        case ButtonAction::TURN_PCDCM1_CCW: state.pcdcms.turn_ccw[1] = true; break;

        // PCDCMs - presets
        case ButtonAction::USE_PCDCM0_ANGLE_PRESET_0:
            state.pcdcms.set_angle[0] = true;
            state.pcdcms.angle[0] = bluestar_config.preset_pcdcm_angles[0][0];
            break;
        case ButtonAction::USE_PCDCM0_ANGLE_PRESET_1:
            state.pcdcms.set_angle[0] = true;
            state.pcdcms.angle[0] = bluestar_config.preset_pcdcm_angles[0][1];
            break;
        case ButtonAction::USE_PCDCM0_ANGLE_PRESET_2:
            state.pcdcms.set_angle[0] = true;
            state.pcdcms.angle[0] = bluestar_config.preset_pcdcm_angles[0][2];
            break;
        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_0:
            state.pcdcms.set_angle[1] = true;
            state.pcdcms.angle[1] = bluestar_config.preset_pcdcm_angles[1][0];
            break;
        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_1:
            state.pcdcms.set_angle[1] = true;
            state.pcdcms.angle[1] = bluestar_config.preset_pcdcm_angles[1][1];
            break;
        case ButtonAction::USE_PCDCM1_ANGLE_PRESET_2:
            state.pcdcms.set_angle[1] = true;
            state.pcdcms.angle[1] = bluestar_config.preset_pcdcm_angles[1][2];
            break;

        // DC Motors
        case ButtonAction::TURN_DCM0_FORWARD:
            state.dc_motors.speed[0] = bluestar_config.dc_motor_speeds[0];
            state.dc_motors.reverse[0] = false;
            break;
        case ButtonAction::TURN_DCM0_REVERSE:
            state.dc_motors.speed[0] = bluestar_config.dc_motor_speeds[0];
            state.dc_motors.reverse[0] = true;
            break;
        case ButtonAction::TURN_DCM1_FORWARD:
            state.dc_motors.speed[1] = bluestar_config.dc_motor_speeds[1];
            state.dc_motors.reverse[1] = false;
            break;
        case ButtonAction::TURN_DCM1_REVERSE:
            state.dc_motors.speed[1] = bluestar_config.dc_motor_speeds[1];
            state.dc_motors.reverse[1] = true;
            break;
        case ButtonAction::TURN_DCM2_FORWARD:
            state.dc_motors.speed[2] = bluestar_config.dc_motor_speeds[2];
            state.dc_motors.reverse[2] = false;
            break;
        case ButtonAction::TURN_DCM2_REVERSE:
            state.dc_motors.speed[2] = bluestar_config.dc_motor_speeds[2];
            state.dc_motors.reverse[2] = true;
            break;
        case ButtonAction::TURN_DCM3_FORWARD:
            state.dc_motors.speed[3] = bluestar_config.dc_motor_speeds[3];
            state.dc_motors.reverse[3] = false;
            break;
        case ButtonAction::TURN_DCM3_REVERSE:
            state.dc_motors.speed[3] = bluestar_config.dc_motor_speeds[3];
            state.dc_motors.reverse[3] = true;
            break;

        default:
            break;
    }
}

void InputManager::applyAxisAction(AxisAction action, float axis_value, float deadzone, RovState& state) {
    if (std::abs(axis_value) <= deadzone) {
        return;
    }

    // Invert and scale to -100 to +100
    int value = -static_cast<int>(axis_value * 100);

    // Do not use axis value if the user already changed a value by button action
    switch (action) {
        case AxisAction::SURGE:
            if (state.motion.surge == 0) state.motion.surge = value;
            break;
        case AxisAction::SWAY:
            if (state.motion.sway == 0) state.motion.sway = value;
            break;
        case AxisAction::YAW:
            if (state.motion.yaw == 0) state.motion.yaw = value;
            break;
        case AxisAction::ROLL:
            if (state.motion.roll == 0) state.motion.roll = value;
            break;
        case AxisAction::HEAVE:
            if (state.motion.heave == 0) state.motion.heave = value;
            break;
        default:
            break;
    }
}

void InputManager::handleModeToggles(RovState& state) {
    // Fast mode toggle with latching
    if (pending_fast_mode_toggle_) {
        if (!fast_mode_latch_) {
            state.modes.fast_mode = !state.modes.fast_mode;
        }
        fast_mode_latch_ = true;
    } else {
        fast_mode_latch_ = false;
    }

    // Invert controls toggle with latching
    if (pending_invert_toggle_) {
        if (!invert_controls_latch_) {
            state.modes.invert_controls = !state.modes.invert_controls;
        }
        invert_controls_latch_ = true;
    } else {
        invert_controls_latch_ = false;
    }

    // Configuration mode toggle with latching
    if (pending_config_mode_toggle_) {
        if (!config_mode_latch_) {
            state.thruster_config.enabled = !state.thruster_config.enabled;
        }
        config_mode_latch_ = true;
    } else {
        config_mode_latch_ = false;
    }
}

void InputManager::applyLatchedCommands(RovState& state) {
    // Servo turn commands - latch to prevent repeated firing
    for (std::size_t i = 0; i < 4; ++i) {
        if (state.servos.turn_cw[i]) {
            if (turn_servo_cw_latch_[i]) {
                state.servos.turn_cw[i] = false;
            } else {
                turn_servo_cw_latch_[i] = true;
            }
        } else {
            turn_servo_cw_latch_[i] = false;
        }

        if (state.servos.turn_ccw[i]) {
            if (turn_servo_ccw_latch_[i]) {
                state.servos.turn_ccw[i] = false;
            } else {
                turn_servo_ccw_latch_[i] = true;
            }
        } else {
            turn_servo_ccw_latch_[i] = false;
        }
    }

    // PCDCM turn commands - latch to prevent repeated firing
    for (std::size_t i = 0; i < 2; ++i) {
        if (state.pcdcms.turn_cw[i]) {
            if (turn_pcdcm_cw_latch_[i]) {
                state.pcdcms.turn_cw[i] = false;
            } else {
                turn_pcdcm_cw_latch_[i] = true;
            }
        } else {
            turn_pcdcm_cw_latch_[i] = false;
        }

        if (state.pcdcms.turn_ccw[i]) {
            if (turn_pcdcm_ccw_latch_[i]) {
                state.pcdcms.turn_ccw[i] = false;
            } else {
                turn_pcdcm_ccw_latch_[i] = true;
            }
        } else {
            turn_pcdcm_ccw_latch_[i] = false;
        }
    }

    // LED commands - latch to prevent repeated firing
    for (std::size_t i = 0; i < 2; ++i) {
        if (state.leds.brighten[i]) {
            if (brighten_led_latch_[i]) {
                state.leds.brighten[i] = false;
            } else {
                brighten_led_latch_[i] = true;
            }
        } else {
            brighten_led_latch_[i] = false;
        }

        if (state.leds.dim[i]) {
            if (dim_led_latch_[i]) {
                state.leds.dim[i] = false;
            } else {
                dim_led_latch_[i] = true;
            }
        } else {
            dim_led_latch_[i] = false;
        }
    }
}

} // namespace bluestar
