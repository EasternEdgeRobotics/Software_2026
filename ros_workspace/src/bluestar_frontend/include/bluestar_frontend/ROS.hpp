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
        const bool& brightenLED_1,
        const bool& dimLED_1,
        const bool& brightenLED_2,
        const bool& dimLED_2,
        const bool& turnFrontServoCw,
        const bool& turnFrontServoCcw,
        const bool& turnBackServoCw,
        const bool& turnBackServoCcw,
        const bool& configurationMode,
        const int& frontServoAngle,
        const int& backServoAngle,
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
            msg.brighten_led_1 = brightenLED_1;
            msg.dim_led_1 = dimLED_1;
            msg.brighten_led_2 = brightenLED_2;
            msg.dim_led_2 = dimLED_2;
            msg.turn_front_servo_cw = turnFrontServoCw;
            msg.turn_front_servo_ccw = turnFrontServoCcw;
            msg.turn_back_servo_cw = turnBackServoCw;
            msg.turn_back_servo_ccw = turnBackServoCcw;
            msg.configuration_mode = configurationMode;
            msg.front_servo_angle = frontServoAngle;
            msg.back_servo_angle = backServoAngle;
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