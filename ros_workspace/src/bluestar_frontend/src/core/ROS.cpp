#include "bluestar_frontend/core/ROS.hpp"

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace bluestar {

// --- PilotInputNode Implementation ---

ROS::PilotInputNode::PilotInputNode() 
    : rclcpp::Node("pilot_input_publisher") 
{
    publisher_ = this->create_publisher<eer_interfaces::msg::PilotInput>("/pilot_input", 10);
}

void ROS::PilotInputNode::publish(const RovState& state) {
    auto msg = eer_interfaces::msg::PilotInput();

    // Motion values
    msg.surge = state.motion.surge;
    msg.sway = state.motion.sway;
    msg.heave = state.motion.heave;
    msg.roll = state.motion.roll;
    msg.yaw = state.motion.yaw;

    // Power multipliers (use effective power based on fast mode)
    const auto& power = state.effectivePower();
    msg.power_multiplier = power.power;
    msg.surge_multiplier = power.surge;
    msg.sway_multiplier = power.sway;
    msg.heave_multiplier = power.heave;
    msg.roll_multiplier = power.roll;
    msg.yaw_multiplier = power.yaw;

    // Configuration mode
    msg.configuration_mode = state.thruster_config.enabled;
    msg.configuration_mode_thruster_number = state.thruster_config.selected_thruster;

    // Thruster firmware parameters
    msg.set_thruster_acceleration = state.thruster_config.set_acceleration;
    msg.thruster_acceleration = state.thruster_config.acceleration;
    msg.set_thruster_timeout = state.thruster_config.set_timeout;
    msg.thruster_timeout = state.thruster_config.timeout_ms;

    // DC Motors
    msg.motor_speed = state.dc_motors.speed;
    msg.motor_direction = state.dc_motors.reverse;

    // LEDs
    msg.brighten_led = state.leds.brighten;
    msg.dim_led = state.leds.dim;

    // Servos
    msg.set_servo_angle = state.servos.set_angle;
    msg.servo_angle = state.servos.angle;
    msg.turn_servo_ccw = state.servos.turn_ccw;
    msg.turn_servo_cw = state.servos.turn_cw;

    // PCDCMs - configuration
    msg.set_precision_control_dc_motor_parameters = state.pcdcms.set_parameters;
    msg.associated_dc_motor_number = state.pcdcms.associated_motor;
    msg.control_loop_period = state.pcdcms.loop_period_ms;
    msg.proportional_gain = state.pcdcms.proportional_gain;
    msg.integral_gain = state.pcdcms.integral_gain;
    msg.derivative_gain = state.pcdcms.derivative_gain;

    // PCDCMs - angle control
    msg.set_precision_control_dc_motor_angle = state.pcdcms.set_angle;
    msg.precision_control_dc_motor_angle = state.pcdcms.angle;
    msg.turn_precision_control_dc_motor_ccw = state.pcdcms.turn_ccw;
    msg.turn_precision_control_dc_motor_cw = state.pcdcms.turn_cw;

    publisher_->publish(msg);
}

// --- SaveConfigNode Implementation ---

ROS::SaveConfigNode::SaveConfigNode() 
    : rclcpp::Node("save_config_publisher") 
{
    publisher_ = this->create_publisher<eer_interfaces::msg::SaveConfig>("/save_config", 10);
}

void ROS::SaveConfigNode::publish(const std::string& name, const std::string& data) {
    auto msg = eer_interfaces::msg::SaveConfig();
    msg.name = name;
    msg.data = data;
    publisher_->publish(msg);
}

// --- ROS Class Implementation ---

ROS::ROS() {
    pilot_input_node_ = std::make_shared<PilotInputNode>();
    save_config_node_ = std::make_shared<SaveConfigNode>();
    
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(pilot_input_node_);
    executor_->add_node(save_config_node_);
}

void ROS::publishPilotInput(const RovState& state) {
    pilot_input_node_->publish(state);
}

void ROS::saveConfig(const std::string& name, const std::string& json_data) {
    save_config_node_->publish(name, json_data);
}

std::vector<std::pair<std::string, std::string>> ROS::fetchConfigs() {
    // Create temporary node for service call
    auto node = rclcpp::Node::make_shared("list_configs_client");
    auto client = node->create_client<eer_interfaces::srv::ListConfig>("list_configs");
    auto request = std::make_shared<eer_interfaces::srv::ListConfig::Request>();

    // Wait for service
    while (!client->wait_for_service(1s)) {
        if (!rclcpp::ok()) {
            std::cerr << "[ROS] Interrupted while waiting for list_configs service" << std::endl;
            return {};
        }
        std::cout << "[ROS] Waiting for list_configs service..." << std::endl;
    }

    // Send request and wait for response
    auto result_future = client->async_send_request(request);
    if (rclcpp::spin_until_future_complete(node, result_future) != rclcpp::FutureReturnCode::SUCCESS) {
        std::cerr << "[ROS] Failed to call list_configs service" << std::endl;
        return {};
    }

    auto response = result_future.get();
    std::vector<std::pair<std::string, std::string>> configs;
    configs.reserve(response->names.size());

    for (std::size_t i = 0; i < response->names.size(); ++i) {
        configs.emplace_back(response->names[i], response->configs[i]);
    }

    return configs;
}

void ROS::spinOnce() {
    executor_->spin_some();
}

} // namespace bluestar
