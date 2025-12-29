#ifndef BLUESTAR_FRONTEND_CORE_ROS_HPP
#define BLUESTAR_FRONTEND_CORE_ROS_HPP

#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/msg/pilot_input.hpp"
#include "eer_interfaces/msg/save_config.hpp"
#include "eer_interfaces/srv/list_config.hpp"

#include "bluestar_frontend/core/RovState.hpp"

namespace bluestar {

/**
 * ROS2 communication layer for the frontend.
 * 
 * Currently handles:
 * - Publishing PilotInput messages (ROV control)
 * - Publishing SaveConfig messages (config persistence)
 * - Fetching configs via ListConfig service
 */
class ROS {
public:
    /**
     * Construct ROS communication layer.
     * Creates internal ROS2 nodes for publishing and service calls.
     */
    ROS();

    /**
     * Publish current ROV state as PilotInput message.
     * Call this once per frame after processing input.
     * 
     * @param state Current ROV state to publish
     */
    void publishPilotInput(const RovState& state);

    /**
     * Save a configuration to persistent storage.
     * 
     * @param name Config name (e.g., "bluestar_config" or user config name)
     * @param json_data Serialized JSON string
     */
    void saveConfig(const std::string& name, const std::string& json_data);

    /**
     * Fetch all available configurations from config_manager.
     * Blocks until service responds or times out.
     * 
     * @return Vector of pairs: (config_name, config_json_data)
     */
    std::vector<std::pair<std::string, std::string>> fetchConfigs();

    /**
     * Spin ROS nodes once (non-blocking).
     * Call this in the main loop to process callbacks.
     */
    void spinOnce();

private:
    // Internal ROS2 node for pilot input publishing
    class PilotInputNode : public rclcpp::Node {
    public:
        PilotInputNode();
        void publish(const RovState& state);
    private:
        rclcpp::Publisher<eer_interfaces::msg::PilotInput>::SharedPtr publisher_;
    };

    // Internal ROS2 node for config saving
    class SaveConfigNode : public rclcpp::Node {
    public:
        SaveConfigNode();
        void publish(const std::string& name, const std::string& data);
    private:
        rclcpp::Publisher<eer_interfaces::msg::SaveConfig>::SharedPtr publisher_;
    };

    std::shared_ptr<PilotInputNode> pilot_input_node_;
    std::shared_ptr<SaveConfigNode> save_config_node_;
    std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
};

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_CORE_ROS_HPP
