#ifndef BLUESTAR_FRONTEND_CORE_ROV_STATE_HPP
#define BLUESTAR_FRONTEND_CORE_ROV_STATE_HPP

#include <array>
#include <cstdint>

namespace bluestar {

/**
 * Power multipliers for motion axes (0-100%).
 * Applied to raw motion inputs before sending to backend.
 */
struct PowerSettings {
    int power = 50;   // Global multiplier
    int surge = 50;
    int sway = 50;
    int heave = 50;
    int roll = 50;
    int yaw = 50;
};

/**
 * Motion setpoints for ROV movement.
 * Values range from -100 to +100.
 */
struct MotionState {
    int surge = 0;  // Forward/backward
    int sway = 0;   // Left/right strafe
    int heave = 0;  // Up/down
    int roll = 0;   // Roll rotation
    int yaw = 0;    // Yaw rotation
};

/**
 * LED state (2 LEDs).
 * Commands are edge-triggered (single pulse per press).
 */
struct LedState {
    static constexpr std::size_t COUNT = 2;

    std::array<bool, COUNT> brighten = {false, false};
    std::array<bool, COUNT> dim = {false, false};
};

/**
 * Servo state (4 servos).
 * Supports incremental turn commands and absolute angle setting.
 */
struct ServoState {
    static constexpr std::size_t COUNT = 4;
    static constexpr std::size_t PRESET_COUNT = 3;

    // Incremental turn commands (edge-triggered)
    std::array<bool, COUNT> turn_cw = {false, false, false, false};
    std::array<bool, COUNT> turn_ccw = {false, false, false, false};

    // Absolute angle setting
    std::array<bool, COUNT> set_angle = {false, false, false, false};
    std::array<uint8_t, COUNT> angle = {127, 127, 127, 127};
};

/**
 * Precision Control DC Motor state (2 PCDCMs).
 * Position-controlled DC motors typically used for camera pan/tilt.
 */
struct PcdcmState {
    static constexpr std::size_t COUNT = 2;
    static constexpr std::size_t PRESET_COUNT = 3;

    // Incremental turn commands (edge-triggered)
    std::array<bool, COUNT> turn_cw = {false, false};
    std::array<bool, COUNT> turn_ccw = {false, false};

    // Absolute angle setting
    std::array<bool, COUNT> set_angle = {false, false};
    std::array<uint8_t, COUNT> angle = {127, 127};

    // Configuration (sent to firmware)
    std::array<bool, COUNT> set_parameters = {false, false};
    std::array<uint8_t, COUNT> associated_motor = {0, 1};
    std::array<uint8_t, COUNT> loop_period_ms = {6, 6};
    std::array<float, COUNT> proportional_gain = {1.0f, 1.0f};
    std::array<float, COUNT> integral_gain = {0.0f, 0.0f};
    std::array<float, COUNT> derivative_gain = {0.0f, 0.0f};
};

/**
 * DC Motor state (4 DC motors).
 * Speed-controlled brushed DC motors.
 */
struct DcMotorState {
    static constexpr std::size_t COUNT = 4;

    std::array<uint8_t, COUNT> speed = {127, 127, 127, 127};
    std::array<bool, COUNT> reverse = {false, false, false, false};
};

/**
 * Thruster configuration state.
 * Used in configuration mode to test/remap individual thrusters.
 */
struct ThrusterConfigState {
    bool enabled = false;
    uint8_t selected_thruster = 0;

    // Firmware parameters
    bool set_acceleration = false;
    uint8_t acceleration = 1;
    bool set_timeout = false;
    uint16_t timeout_ms = 2500;
};

/**
 * Mode flags controlling ROV behavior.
 */
struct ModeState {
    bool fast_mode = false;
    bool invert_controls = false;
    bool keyboard_mode = false;
};

/**
 * Complete ROV state aggregating all subsystems.
 * This is the central state object passed throughout the application.
 */
struct RovState {
    MotionState motion;
    PowerSettings power;
    PowerSettings fast_mode_power;  // Power settings when fast mode is active

    LedState leds;
    ServoState servos;
    PcdcmState pcdcms;
    DcMotorState dc_motors;

    ThrusterConfigState thruster_config;
    ModeState modes;

    /**
     * Reset per-frame transient commands (edge-triggered signals).
     * Call at the start of each frame before processing input.
     */
    void resetTransientCommands() {
        motion = MotionState{};

        leds.brighten = {false, false};
        leds.dim = {false, false};

        servos.turn_cw = {false, false, false, false};
        servos.turn_ccw = {false, false, false, false};
        servos.set_angle = {false, false, false, false};

        pcdcms.turn_cw = {false, false};
        pcdcms.turn_ccw = {false, false};
        pcdcms.set_angle = {false, false};

        // One shot firmware commands
        thruster_config.set_acceleration = false;
        thruster_config.set_timeout = false;
        pcdcms.set_parameters = {false, false};
    }

    /**
     * Get effective power settings (fast mode or normal).
     */
    const PowerSettings& effectivePower() const {
        return modes.fast_mode ? fast_mode_power : power;
    }
};

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_CORE_ROV_STATE_HPP
