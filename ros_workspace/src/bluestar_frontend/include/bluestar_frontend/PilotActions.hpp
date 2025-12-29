#ifndef BLUESTAR_FRONTEND_PILOT_ACTIONS_HPP
#define BLUESTAR_FRONTEND_PILOT_ACTIONS_HPP

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <string>

namespace bluestar {

/**
 * Actions triggered by button presses (digital inputs).
 * Used by both controller buttons and keyboard keys.
 */
enum class ButtonAction {
    NONE = 0,

    // Motion (digital override - full magnitude)
    SURGE_FORWARD,
    SURGE_BACKWARD,
    SWAY_LEFT,
    SWAY_RIGHT,
    YAW_LEFT,
    YAW_RIGHT,
    HEAVE_UP,
    HEAVE_DOWN,
    ROLL_CW,
    ROLL_CCW,

    // Mode toggles
    CONFIGURATION_MODE,
    FAST_MODE,
    INVERT_CONTROLS,

    // LEDs (2 LEDs)
    BRIGHTEN_LED0,
    DIM_LED0,
    BRIGHTEN_LED1,
    DIM_LED1,

    // Servos (4 servos)
    TURN_SERVO0_CW,
    TURN_SERVO0_CCW,
    TURN_SERVO1_CW,
    TURN_SERVO1_CCW,
    TURN_SERVO2_CW,
    TURN_SERVO2_CCW,
    TURN_SERVO3_CW,
    TURN_SERVO3_CCW,

    // Servo angle presets (3 presets per servo)
    USE_SERVO0_ANGLE_PRESET_0,
    USE_SERVO0_ANGLE_PRESET_1,
    USE_SERVO0_ANGLE_PRESET_2,
    USE_SERVO1_ANGLE_PRESET_0,
    USE_SERVO1_ANGLE_PRESET_1,
    USE_SERVO1_ANGLE_PRESET_2,
    USE_SERVO2_ANGLE_PRESET_0,
    USE_SERVO2_ANGLE_PRESET_1,
    USE_SERVO2_ANGLE_PRESET_2,
    USE_SERVO3_ANGLE_PRESET_0,
    USE_SERVO3_ANGLE_PRESET_1,
    USE_SERVO3_ANGLE_PRESET_2,

    // Precision Control DC Motors (2 PCDCMs)
    TURN_PCDCM0_CW,
    TURN_PCDCM0_CCW,
    TURN_PCDCM1_CW,
    TURN_PCDCM1_CCW,

    // PCDCM angle presets (3 presets per PCDCM)
    USE_PCDCM0_ANGLE_PRESET_0,
    USE_PCDCM0_ANGLE_PRESET_1,
    USE_PCDCM0_ANGLE_PRESET_2,
    USE_PCDCM1_ANGLE_PRESET_0,
    USE_PCDCM1_ANGLE_PRESET_1,
    USE_PCDCM1_ANGLE_PRESET_2,

    // DC Motors (4 DC motors)
    TURN_DCM0_FORWARD,
    TURN_DCM0_REVERSE,
    TURN_DCM1_FORWARD,
    TURN_DCM1_REVERSE,
    TURN_DCM2_FORWARD,
    TURN_DCM2_REVERSE,
    TURN_DCM3_FORWARD,
    TURN_DCM3_REVERSE,

    // Sentinel for iteration
    COUNT
};

/**
 * Actions triggered by analog axis inputs (continuous values).
 * Used by controller joysticks/triggers.
 */
enum class AxisAction {
    NONE = 0,
    SURGE,
    SWAY,
    YAW,
    ROLL,
    HEAVE,

    // Sentinel for iteration
    COUNT
};

// Human-readable labels for UI display
constexpr const char* BUTTON_ACTION_LABELS[] = {
    "None",
    "Surge Forward", "Surge Backward", "Sway Left", "Sway Right",
    "Yaw Left", "Yaw Right", "Heave Up", "Heave Down", "Roll CW", "Roll CCW",
    "Configuration Mode", "Fast Mode", "Invert Controls",
    "Brighten LED 0", "Dim LED 0", "Brighten LED 1", "Dim LED 1",
    "Turn Servo 0 CW", "Turn Servo 0 CCW", "Turn Servo 1 CW", "Turn Servo 1 CCW",
    "Turn Servo 2 CW", "Turn Servo 2 CCW", "Turn Servo 3 CW", "Turn Servo 3 CCW",
    "Servo 0 Preset 0", "Servo 0 Preset 1", "Servo 0 Preset 2",
    "Servo 1 Preset 0", "Servo 1 Preset 1", "Servo 1 Preset 2",
    "Servo 2 Preset 0", "Servo 2 Preset 1", "Servo 2 Preset 2",
    "Servo 3 Preset 0", "Servo 3 Preset 1", "Servo 3 Preset 2",
    "Turn PCDCM 0 CW", "Turn PCDCM 0 CCW", "Turn PCDCM 1 CW", "Turn PCDCM 1 CCW",
    "PCDCM 0 Preset 0", "PCDCM 0 Preset 1", "PCDCM 0 Preset 2",
    "PCDCM 1 Preset 0", "PCDCM 1 Preset 1", "PCDCM 1 Preset 2",
    "DCM 0 Forward", "DCM 0 Reverse", "DCM 1 Forward", "DCM 1 Reverse",
    "DCM 2 Forward", "DCM 2 Reverse", "DCM 3 Forward", "DCM 3 Reverse"
};

constexpr const char* AXIS_ACTION_LABELS[] = {
    "None", "Surge", "Sway", "Yaw", "Roll", "Heave"
};

// Serialization codes for config JSON (must match existing schema)
constexpr const char* BUTTON_ACTION_CODES[] = {
    "none",
    "surge_forward", "surge_backward", "sway_left", "sway_right",
    "yaw_left", "yaw_right", "heave_up", "heave_down", "roll_cw", "roll_ccw",
    "configuration_mode", "fast_mode", "invert_controls",
    "brighten_led0", "dim_led0", "brighten_led1", "dim_led1",
    "turn_servo0_cw", "turn_servo0_ccw", "turn_servo1_cw", "turn_servo1_ccw",
    "turn_servo2_cw", "turn_servo2_ccw", "turn_servo3_cw", "turn_servo3_ccw",
    "use_servo0_angle_preset_0", "use_servo0_angle_preset_1", "use_servo0_angle_preset_2",
    "use_servo1_angle_preset_0", "use_servo1_angle_preset_1", "use_servo1_angle_preset_2",
    "use_servo2_angle_preset_0", "use_servo2_angle_preset_1", "use_servo2_angle_preset_2",
    "use_servo3_angle_preset_0", "use_servo3_angle_preset_1", "use_servo3_angle_preset_2",
    "turn_pcdcm0_cw", "turn_pcdcm0_ccw", "turn_pcdcm1_cw", "turn_pcdcm1_ccw",
    "use_pcdcm0_angle_preset_0", "use_pcdcm0_angle_preset_1", "use_pcdcm0_angle_preset_2",
    "use_pcdcm1_angle_preset_0", "use_pcdcm1_angle_preset_1", "use_pcdcm1_angle_preset_2",
    "turn_dcm0_forward", "turn_dcm0_reverse", "turn_dcm1_forward", "turn_dcm1_reverse",
    "turn_dcm2_forward", "turn_dcm2_reverse", "turn_dcm3_forward", "turn_dcm3_reverse"
};

constexpr const char* AXIS_ACTION_CODES[] = {
    "none", "surge", "sway", "yaw", "roll", "heave"
};

// Conversion utilities
inline const char* buttonActionToLabel(ButtonAction action) {
    auto idx = static_cast<std::size_t>(action);
    if (idx < static_cast<std::size_t>(ButtonAction::COUNT)) {
        return BUTTON_ACTION_LABELS[idx];
    }
    return BUTTON_ACTION_LABELS[0];
}

inline const char* buttonActionToCode(ButtonAction action) {
    auto idx = static_cast<std::size_t>(action);
    if (idx < static_cast<std::size_t>(ButtonAction::COUNT)) {
        return BUTTON_ACTION_CODES[idx];
    }
    return BUTTON_ACTION_CODES[0];
}

inline ButtonAction codeToButtonAction(const std::string& code) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(ButtonAction::COUNT); ++i) {
        if (code == BUTTON_ACTION_CODES[i]) {
            return static_cast<ButtonAction>(i);
        }
    }
    return ButtonAction::NONE;
}

inline const char* axisActionToLabel(AxisAction action) {
    auto idx = static_cast<std::size_t>(action);
    if (idx < static_cast<std::size_t>(AxisAction::COUNT)) {
        return AXIS_ACTION_LABELS[idx];
    }
    return AXIS_ACTION_LABELS[0];
}

inline const char* axisActionToCode(AxisAction action) {
    auto idx = static_cast<std::size_t>(action);
    if (idx < static_cast<std::size_t>(AxisAction::COUNT)) {
        return AXIS_ACTION_CODES[idx];
    }
    return AXIS_ACTION_CODES[0];
}

inline AxisAction codeToAxisAction(const std::string& code) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(AxisAction::COUNT); ++i) {
        if (code == AXIS_ACTION_CODES[i]) {
            return static_cast<AxisAction>(i);
        }
    }
    return AxisAction::NONE;
}

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_PILOT_ACTIONS_HPP
