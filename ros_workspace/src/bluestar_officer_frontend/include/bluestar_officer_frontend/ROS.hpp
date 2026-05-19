#ifndef ROS_HPP
#define ROS_HPP

#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/srv/list_config.hpp"
#include "eer_interfaces/msg/save_config.hpp"
#include <cstdint>

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