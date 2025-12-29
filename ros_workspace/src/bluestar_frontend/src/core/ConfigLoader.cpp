#include "bluestar_frontend/core/ConfigLoader.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace bluestar {

BluestarConfig loadBluestarConfig(const std::string& json_str) {
    BluestarConfig config;
    
    try {
        json j = json::parse(json_str);

        // Thruster configuration
        if (j.contains("thruster_stronger_side_attenuation_constant")) {
            config.thruster_stronger_side_attenuation_constant = j["thruster_stronger_side_attenuation_constant"].get<float>();
        }

        for (std::size_t i = 0; i < config.thruster_map.size(); ++i) {
            if (j.contains("thruster_map") && j["thruster_map"].size() > i) {
                config.thruster_map[i] = j["thruster_map"][i].get<int>();
            }
            if (j.contains("reverse_thrusters") && j["reverse_thrusters"].size() > i) {
                config.reverse_thrusters[i] = j["reverse_thrusters"][i].get<bool>();
            }
            if (j.contains("stronger_side_positive") && j["stronger_side_positive"].size() > i) {
                config.stronger_side_positive[i] = j["stronger_side_positive"][i].get<bool>();
            }
        }

        // Servo presets
        if (j.contains("preset_servo_angles")) {
            for (std::size_t i = 0; i < config.preset_servo_angles.size(); ++i) {
                for (std::size_t k = 0; k < config.preset_servo_angles[i].size(); ++k) {
                    if (j["preset_servo_angles"].size() > i && j["preset_servo_angles"][i].size() > k) {
                        config.preset_servo_angles[i][k] = j["preset_servo_angles"][i][k].get<uint8_t>();
                    }
                }
            }
        }

        // PCDCM presets
        if (j.contains("preset_precision_control_dc_motor_angles")) {
            for (std::size_t i = 0; i < config.preset_pcdcm_angles.size(); ++i) {
                for (std::size_t k = 0; k < config.preset_pcdcm_angles[i].size(); ++k) {
                    if (j["preset_precision_control_dc_motor_angles"].size() > i && 
                        j["preset_precision_control_dc_motor_angles"][i].size() > k) {
                        config.preset_pcdcm_angles[i][k] = j["preset_precision_control_dc_motor_angles"][i][k].get<uint8_t>();
                    }
                }
            }
        }

        // DC motor speeds
        if (j.contains("dc_motor_speeds")) {
            for (std::size_t i = 0; i < config.dc_motor_speeds.size(); ++i) {
                if (j["dc_motor_speeds"].size() > i) {
                    config.dc_motor_speeds[i] = j["dc_motor_speeds"][i].get<uint8_t>();
                }
            }
        }

        // Firmware parameters
        if (j.contains("thruster_acceleration")) {
            config.thruster_acceleration = j["thruster_acceleration"].get<uint8_t>();
        }
        if (j.contains("thruster_timeout")) {
            config.thruster_timeout_ms = j["thruster_timeout"].get<uint16_t>();
        }

        // PCDCM PID configuration
        for (std::size_t i = 0; i < 2; ++i) {
            if (j.contains("precision_control_dc_motor_numbers") && j["precision_control_dc_motor_numbers"].size() > i) {
                config.pcdcm_motor_numbers[i] = j["precision_control_dc_motor_numbers"][i].get<uint8_t>();
            }
            if (j.contains("precision_control_dc_motor_control_loop_period") && 
                j["precision_control_dc_motor_control_loop_period"].size() > i) {
                config.pcdcm_loop_period_ms[i] = j["precision_control_dc_motor_control_loop_period"][i].get<uint8_t>();
            }
            if (j.contains("precision_control_dc_motor_proportional_gain") && 
                j["precision_control_dc_motor_proportional_gain"].size() > i) {
                config.pcdcm_proportional_gain[i] = j["precision_control_dc_motor_proportional_gain"][i].get<float>();
            }
            if (j.contains("precision_control_dc_motor_integral_gain") && 
                j["precision_control_dc_motor_integral_gain"].size() > i) {
                config.pcdcm_integral_gain[i] = j["precision_control_dc_motor_integral_gain"][i].get<float>();
            }
            if (j.contains("precision_control_dc_motor_derivative_gain") && 
                j["precision_control_dc_motor_derivative_gain"].size() > i) {
                config.pcdcm_derivative_gain[i] = j["precision_control_dc_motor_derivative_gain"][i].get<float>();
            }
        }

    } catch (const json::exception& e) {
        std::cerr << "[ConfigLoader] Failed to parse bluestar_config: " << e.what() << std::endl;
        // Return default config on error
    }

    return config;
}

std::string saveBluestarConfig(const BluestarConfig& config) {
    json j;
    
    j["name"] = "bluestar_config";
    j["thruster_stronger_side_attenuation_constant"] = config.thruster_stronger_side_attenuation_constant;

    for (std::size_t i = 0; i < config.thruster_map.size(); ++i) {
        j["thruster_map"][i] = config.thruster_map[i];
        j["reverse_thrusters"][i] = config.reverse_thrusters[i];
        j["stronger_side_positive"][i] = config.stronger_side_positive[i];
    }

    for (std::size_t i = 0; i < config.preset_servo_angles.size(); ++i) {
        for (std::size_t k = 0; k < config.preset_servo_angles[i].size(); ++k) {
            j["preset_servo_angles"][i][k] = config.preset_servo_angles[i][k];
        }
    }

    for (std::size_t i = 0; i < config.preset_pcdcm_angles.size(); ++i) {
        for (std::size_t k = 0; k < config.preset_pcdcm_angles[i].size(); ++k) {
            j["preset_precision_control_dc_motor_angles"][i][k] = config.preset_pcdcm_angles[i][k];
        }
    }

    for (std::size_t i = 0; i < config.dc_motor_speeds.size(); ++i) {
        j["dc_motor_speeds"][i] = config.dc_motor_speeds[i];
    }

    j["thruster_acceleration"] = config.thruster_acceleration;
    j["thruster_timeout"] = config.thruster_timeout_ms;

    for (std::size_t i = 0; i < 2; ++i) {
        j["precision_control_dc_motor_numbers"][i] = config.pcdcm_motor_numbers[i];
        j["precision_control_dc_motor_control_loop_period"][i] = config.pcdcm_loop_period_ms[i];
        j["precision_control_dc_motor_proportional_gain"][i] = config.pcdcm_proportional_gain[i];
        j["precision_control_dc_motor_integral_gain"][i] = config.pcdcm_integral_gain[i];
        j["precision_control_dc_motor_derivative_gain"][i] = config.pcdcm_derivative_gain[i];
    }

    return j.dump();
}

std::optional<UserConfig> loadUserConfig(const std::string& json_str, const std::string& name) {
    try {
        json j = json::parse(json_str);
        UserConfig config;
        config.name = name;

        config.deadzone = j.value("deadzone", 0.1f);

        // Load button mappings (legacy format from Beaumont's dual controller mode: mappings["0"]["buttons"])
        if (j.contains("mappings") && j["mappings"].contains("0")) {
            auto& mapping = j["mappings"]["0"];
            
            if (mapping.contains("buttons")) {
                for (auto& [key, value] : mapping["buttons"].items()) {
                    config.button_actions.push_back(codeToButtonAction(value.get<std::string>()));
                }
            }

            if (mapping.contains("axes")) {
                for (auto& [key, value] : mapping["axes"].items()) {
                    config.axis_actions.push_back(codeToAxisAction(value.get<std::string>()));
                }
            }
        }

        return config;

    } catch (const json::exception& e) {
        std::cerr << "[ConfigLoader] Failed to parse user_config '" << name << "': " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string saveUserConfig(const UserConfig& config) {
    json j;
    
    j["deadzone"] = config.deadzone;

    // Save in Beaumont legacy format
    for (std::size_t i = 0; i < config.axis_actions.size(); ++i) {
        j["mappings"]["0"]["deadzones"][std::to_string(i)] = config.deadzone;
    }

    for (std::size_t i = 0; i < config.button_actions.size(); ++i) {
        j["mappings"]["0"]["buttons"][i] = buttonActionToCode(config.button_actions[i]);
    }

    for (std::size_t i = 0; i < config.axis_actions.size(); ++i) {
        j["mappings"]["0"]["axes"][i] = axisActionToCode(config.axis_actions[i]);
    }

    return j.dump();
}

} // namespace bluestar
