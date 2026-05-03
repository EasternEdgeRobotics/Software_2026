#include <memory>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/float64.hpp"
#include "eer_interfaces/msg/blue_star_control.hpp"
#include "bluestar_constants.h"


class SimulationBotControl : public rclcpp::Node
{
public:
  SimulationBotControl() : Node("SimulationBotControl")
  {
    
    for (int thruster_index = 0; thruster_index < 6; thruster_index++) {
      thruster_publishers[thruster_index] = this->create_publisher<std_msgs::msg::Int32>("/bluestar/" + THRUSTER_NAMES[thruster_index], 10);
    }

    servo_1_forward_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_1_forward", 10);
    servo_1_downtilt_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_1_downtilt", 10);
    servo_2_forward_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_2_forward", 10);
    servo_2_downtilt_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_2_downtilt", 10);
    servo_3_forward_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_3_forward", 10);
    servo_3_downtilt_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_3_downtilt", 10);
    servo_4_forward_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_4_forward", 10);
    servo_4_downtilt_publisher = this->create_publisher<std_msgs::msg::Float64>("/servo_4_downtilt", 10);

    auto control_values_subscriber_callback =
      [this](eer_interfaces::msg::BlueStarControl::UniquePtr control_values_msg) -> void {
        for (int thruster_index = 0; thruster_index < 6; thruster_index++) {

            std_msgs::msg::Int32 simulation_thrust_msg;

            // Map the thrust value from the range [-1, 1] to [0, 255]
            simulation_thrust_msg.data = static_cast<int32_t>((control_values_msg->thrust[thruster_index] + 1) * 127.5);

            // Gazebo doesn't accept 0 as a value, so set it to 1 in that case
            if (simulation_thrust_msg.data == 0) simulation_thrust_msg.data = 1;

            thruster_publishers[thruster_index]->publish(simulation_thrust_msg);
        }

        std_msgs::msg::Float64 servo_1_msg;
        std_msgs::msg::Float64 servo_2_msg;
        std_msgs::msg::Float64 servo_3_msg;
        std_msgs::msg::Float64 servo_4_msg;

        servo_1_msg.data = static_cast<double>(control_values_msg->servos[0]);

        if (servo_1_msg.data == 0) servo_1_msg.data = 0.001;

        servo_1_forward_publisher->publish(servo_1_msg);
        servo_1_downtilt_publisher->publish(servo_1_msg);

        servo_2_msg.data = static_cast<double>(control_values_msg->servos[1]);

        if (servo_2_msg.data == 0) servo_2_msg.data = 0.001;

        servo_2_forward_publisher->publish(servo_2_msg);
        servo_2_downtilt_publisher->publish(servo_2_msg);

        servo_3_msg.data = static_cast<double>(control_values_msg->servos[2]);

        if (servo_3_msg.data == 0) servo_3_msg.data = 0.001;

        servo_3_forward_publisher->publish(servo_3_msg);
        servo_3_downtilt_publisher->publish(servo_3_msg);

        servo_4_msg.data = static_cast<double>(control_values_msg->servos[3]);

        if (servo_4_msg.data == 0) servo_4_msg.data = 0.001;

        servo_4_forward_publisher->publish(servo_4_msg);
        servo_4_downtilt_publisher->publish(servo_4_msg);
      };

    control_values_subscriber =
      this->create_subscription<eer_interfaces::msg::BlueStarControl>(
        "bluestar_control_values", 10, control_values_subscriber_callback);
  }

private:
  rclcpp::Subscription<eer_interfaces::msg::BlueStarControl>::SharedPtr control_values_subscriber;
  std::array<rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr, 6> thruster_publishers;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_1_forward_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_1_downtilt_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_2_forward_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_2_downtilt_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_3_forward_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_3_downtilt_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_4_forward_publisher;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr servo_4_downtilt_publisher;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SimulationBotControl>());
  rclcpp::shutdown();
  return 0;
}