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

        void sendInput(const Power& power,
        const int& surge,
        const int& sway,
        const int& heave,
        const int& yaw,
        const int& roll,
        const int& DCmotor_1,
        const int& DCmotor_2,
        const int& LED1Brightness,
        const int& LED2Brightness,
        const int& Servo1Angle,
        const int& Servo2Angle,
        const int& Servo3Angle,
        const int& Servo4Angle,
        const bool& configurationMode,
        const int& configurationModeThrusterNumber)
        {
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

            msg.led_1_brightness = LED1Brightness;
            msg.led_2_brightness = LED2Brightness;

            msg.dc_motor_1 = DCmotor_1;
            msg.dc_motor_2 = DCmotor_2;

            msg.servo_1_angle = Servo1Angle;
            msg.servo_2_angle = Servo2Angle;
            msg.servo_3_angle = Servo3Angle;
            msg.servo_4_angle = Servo4Angle;

            msg.configuration_mode = configurationMode;
            msg.configuration_mode_thruster_number = configurationModeThrusterNumber;
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