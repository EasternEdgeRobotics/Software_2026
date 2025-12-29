#ifndef BLUESTAR_FRONTEND_CORE_CONFIG_LOADER_HPP
#define BLUESTAR_FRONTEND_CORE_CONFIG_LOADER_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include "bluestar_frontend/PilotActions.hpp"

namespace bluestar {

/**
 * User configuration for controller/input mappings.
 * Per-user config stored as JSON.
 */
struct UserConfig {
    std::string name;
    float deadzone = 0.1f;
    std::vector<ButtonAction> button_actions;
    std::vector<AxisAction> axis_actions;
};

/**
 * ROV-wide configuration shared across all users.
 * Contains thruster mappings, presets, and firmware parameters.
 * 
 * JSON schema must remain compatible with pilot_listener.cpp in backend.
 */
struct BluestarConfig {
    // Thruster configuration (used by backend)
    std::array<int, 6> thruster_map = {0, 1, 2, 3, 4, 5};
    std::array<bool, 6> reverse_thrusters = {false, false, false, false, false, false};
    std::array<bool, 6> stronger_side_positive = {false, false, false, false, false, false};
    float thruster_stronger_side_attenuation_constant = 1.0f;

    // Preset angles (used by frontend)
    std::array<std::array<uint8_t, 3>, 4> preset_servo_angles = {{{0, 127, 255}, {0, 127, 255}, {0, 127, 255}, {0, 127, 255}}};
    std::array<std::array<uint8_t, 3>, 2> preset_pcdcm_angles = {{{0, 127, 255}, {0, 127, 255}}};
    std::array<uint8_t, 4> dc_motor_speeds = {127, 127, 127, 127};

    // Firmware parameters
    uint8_t thruster_acceleration = 1;
    uint16_t thruster_timeout_ms = 2500;

    // PCDCM PID configuration
    std::array<uint8_t, 2> pcdcm_motor_numbers = {0, 1};
    std::array<uint8_t, 2> pcdcm_loop_period_ms = {6, 6};
    std::array<float, 2> pcdcm_proportional_gain = {1.0f, 1.0f};
    std::array<float, 2> pcdcm_integral_gain = {0.0f, 0.0f};
    std::array<float, 2> pcdcm_derivative_gain = {0.0f, 0.0f};
};

/**
 * Load BluestarConfig from JSON string.
 * Returns default config if parsing fails.
 */
BluestarConfig loadBluestarConfig(const std::string& json_str);

/**
 * Serialize BluestarConfig to JSON string.
 */
std::string saveBluestarConfig(const BluestarConfig& config);

/**
 * Load UserConfig from JSON string.
 * Returns nullopt if parsing fails.
 */
std::optional<UserConfig> loadUserConfig(const std::string& json_str, const std::string& name);

/**
 * Serialize UserConfig to JSON string.
 * Compatible with existing schema (mappings["0"] structure for legacy compatibility).
 */
std::string saveUserConfig(const UserConfig& config);

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_CORE_CONFIG_LOADER_HPP
