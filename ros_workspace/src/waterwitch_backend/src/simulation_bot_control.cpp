#include <memory>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/float64.hpp"
#include "eer_interfaces/msg/waterwitch_control.hpp"
#include "waterwitch_constants.h"


class SimulationBotControl : public rclcpp::Node
{
public:
  SimulationBotControl() : Node("SimulationBotControl")
  {
    
    for (int thruster_index = 0; thruster_index < 6; thruster_index++) {
      thruster_publishers[thruster_index] = this->create_publisher<std_msgs::msg::Int32>("/waterwitch/" + THRUSTER_NAMES[thruster_index], 10);
    }

    front_camera_forward_servo_publisher = this->create_publisher<std_msgs::msg::Float64>("/front_camera_forward_servo", 10);
    front_camera_downtilt_servo_publisher = this->create_publisher<std_msgs::msg::Float64>("/front_camera_downtilt_servo", 10);
    back_camera_forward_servo_publisher = this->create_publisher<std_msgs::msg::Float64>("/back_camera_forward_servo", 10);
    back_camera_downtilt_servo_publisher = this->create_publisher<std_msgs::msg::Float64>("/back_camera_downtilt_servo", 10);

    auto control_values_subscriber_callback =
      [this](eer_interfaces::msg::WaterwitchControl::UniquePtr control_values_msg) -> void {
        for (int thruster_index = 0; thruster_index < 6; thruster_index++) {

            std_msgs::msg::Int32 simulation_thrust_msg;

            // Map the thrust value from the range [-1, 1] to [0, 255]
            simulation_thrust_msg.data = static_cast<int32_t>((control_values_msg->thrust[thruster_index] + 1) * 127.5);

            // Gazebo doesn't accept 0 as a value, so set it to 1 in that case
            if (simulation_thrust_msg.data == 0) simulation_thrust_msg.data = 1;

            thruster_publishers[thruster_index]->publish(simulation_thrust_msg);
        }

        std_msgs::msg::Float64 front_servo_msg;
        std_msgs::msg::Float64 back_servo_msg;

        front_servo_msg.data = static_cast<double>(control_values_msg->camera_servos[0]);

        if (front_servo_msg.data == 0) front_servo_msg.data = 0.001;

        front_camera_forward_servo_publisher->publish(front_servo_msg);
        front_camera_downtilt_servo_publisher->publish(front_servo_msg);

        back_servo_msg.data = static_cast<double>(control_values_msg->camera_servos[1]);

        if (back_servo_msg.data == 0) back_servo_msg.data = 0.001;

        back_camera_forward_servo_publisher->publish(back_servo_msg);
        back_camera_downtilt_servo_publisher->publish(back_servo_msg);
      };

    control_values_subscriber =
      this->create_subscription<eer_interfaces::msg::WaterwitchControl>(
        "waterwitch_control_values", 10, control_values_subscriber_callback);
  }

private:
  rclcpp::Subscription<eer_interfaces::msg::WaterwitchControl>::SharedPtr control_values_subscriber;
  std::array<rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr, 6> thruster_publishers;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr front_camera_forward_servo_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr front_camera_downtilt_servo_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr back_camera_forward_servo_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr back_camera_downtilt_servo_publisher;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SimulationBotControl>());
  rclcpp::shutdown();
  return 0;
}