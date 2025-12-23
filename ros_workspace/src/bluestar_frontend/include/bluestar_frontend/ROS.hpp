#ifndef ROS_HPP
#define ROS_HPP

#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/srv/list_config.hpp"
#include "eer_interfaces/msg/save_config.hpp"
#include "eer_interfaces/msg/pilot_input.hpp"
#include <cstdint>
#include "Power.hpp"

class SaveConfigPublisher : public rclcpp::Node {
    public:
        SaveConfigPublisher() : Node("save_config_publisher") {
            publisher_ = this->create_publisher<eer_interfaces::msg::SaveConfig>("/save_config", 10);
        }

        void saveConfig(const std::string & name, const std::string & data) {
            auto msg = eer_interfaces::msg::SaveConfig();
            msg.name = name;
            msg.data = data;
            publisher_->publish(msg);
        }

    private:
        rclcpp::Publisher<eer_interfaces::msg::SaveConfig>::SharedPtr publisher_;
};

class PilotInputPublisher : public rclcpp::Node {
    public:
        PilotInputPublisher() : Node("pilot_input_publisher") {
            publisher_ = this->create_publisher<eer_interfaces::msg::PilotInput>("/pilot_input", 10);
        }

        void sendInput(
        const Power& power,
        const int& surge,
        const int& sway,
        const int& heave,
        const int& yaw,
        const int& roll,

        const bool& configurationMode,
        const uint8_t& configurationModeThrusterNumber,
        
        const bool& setThrusterAcceleration,
        const uint8_t& thrusterAcceleration,
        const bool& setThrusterTimeout,
        const uint16_t& thrusterTimeout,

        const std::array<uint8_t, 4>& dcMotorSpeeds,
        const std::array<bool, 4>& turnDcmReverse,

        const std::array<bool, 2>& brightenLED,
        const std::array<bool, 2>& dimLED,

        const std::array<bool, 4>& setServoAngle,
        const std::array<uint8_t, 4>& servoAngles,
        const std::array<bool, 4>& turnServoCcw,
        const std::array<bool, 4>& turnServoCw,

        const std::array<bool, 2>& setPrecisionControlDcMotorParameters,
        const std::array<uint8_t, 2>& precisionControlAssociatedDCMotorNumbers,
        const std::array<uint8_t, 2>& precisionControlLoopPeriod,
        const std::array<float, 2>& precisionControlProportionalGain,
        const std::array<float, 2>& precisionControlIntegralGain,
        const std::array<float, 2>& precisionControlDerivativeGain,

        const std::array<bool, 2>& setPcdcmAngle,
        const std::array<uint8_t, 2>& pcdcmAngles,
        const std::array<bool, 2>& turnPcdcmCcw,
        const std::array<bool, 2>& turnPcdcmCw) {
            auto msg = eer_interfaces::msg::PilotInput();
            msg.surge = surge;
            msg.sway = sway;
            msg.heave = heave;
            msg.yaw = yaw;
            msg.roll = roll;
            msg.power_multiplier = power.power;
            msg.surge_multiplier = power.surge;
            msg.sway_multiplier = power.sway;
            msg.heave_multiplier = power.heave;
            msg.roll_multiplier = power.roll;
            msg.yaw_multiplier = power.yaw;

            msg.configuration_mode = configurationMode;
            msg.configuration_mode_thruster_number = configurationModeThrusterNumber;

            msg.set_thruster_acceleration = setThrusterAcceleration;
            msg.thruster_acceleration = thrusterAcceleration;
            msg.set_thruster_timeout = setThrusterTimeout;
            msg.thruster_timeout = thrusterTimeout;

            msg.motor_speed = dcMotorSpeeds;
            msg.motor_direction = turnDcmReverse;

            msg.set_servo_angle = setServoAngle;
            msg.servo_angle = servoAngles;
            msg.turn_servo_ccw = turnServoCcw;
            msg.turn_servo_cw = turnServoCw;
            
            msg.brighten_led = brightenLED;
            msg.dim_led = dimLED;

            msg.set_precision_control_dc_motor_parameters = setPrecisionControlDcMotorParameters;
            msg.precision_control_associated_dc_motor_numbers = precisionControlAssociatedDCMotorNumbers;
            msg.precision_control_loop_period = precisionControlLoopPeriod;
            msg.precision_control_proportional_gain = precisionControlProportionalGain;
            msg.precision_control_integral_gain = precisionControlIntegralGain;
            msg.precision_control_derivative_gain = precisionControlDerivativeGain;

            msg.set_precision_control_dc_motor_angles = setPcdcmAngle;
            msg.precision_control_dc_motor_angles = pcdcmAngles;
            msg.turn_precision_control_dc_motor_ccw = turnPcdcmCcw;
            msg.turn_precision_control_dc_motor_cw = turnPcdcmCw;


            publisher_->publish(msg);
            // ########################
            // Add more inputs to this function
            // ########################
        }

    private:
        rclcpp::Publisher<eer_interfaces::msg::PilotInput>::SharedPtr publisher_;
};

std::array<std::vector<std::string>, 2> getConfigs() {
    auto node = rclcpp::Node::make_shared("list_configs_client");
    auto client = node->create_client<eer_interfaces::srv::ListConfig>("list_configs");
    auto request = std::make_shared<eer_interfaces::srv::ListConfig::Request>();
  
    while (!client->wait_for_service(1s)) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service. Exiting.");
            return std::array<std::vector<std::string>, 2>();
        }
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "service not available, waiting again...");
    }

    std::array<std::vector<std::string>, 2> output;
  
    auto result = client->async_send_request(request);
    if (rclcpp::spin_until_future_complete(node, result) == rclcpp::FutureReturnCode::SUCCESS) {
        auto response = result.get();
        output[0] = response->names;
        output[1] = response->configs;
        for (const auto & config : response->configs) {
            RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Config: %s", config.c_str());
        }
    } else {
        RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Failed to call service list_configs");
    }

    return output;
}

#endif