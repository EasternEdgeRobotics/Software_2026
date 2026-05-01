#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <stdexcept>
#include <sys/wait.h> 

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/float64.hpp"
#include "eer_interfaces/msg/blue_star_control.hpp"
#include "bluestar_constants.h"

#define THRUST_SCALE 127.5
#define LED1_REGISTER 7
#define LED2_REGISTER 6 // Flipped to match current config on BlueStar -PC
#define I2C_BUS_FILE "/dev/i2c-1"


class I2CMaster : public rclcpp::Node
{
public:
  I2CMaster() : Node("I2CMaster")
  {

    auto control_values_subscriber_callback =
      [this](eer_interfaces::msg::BlueStarControl::UniquePtr control_values_msg) -> void {
        for (int thruster_index = 0; thruster_index < 6; thruster_index++) {

            // Map the thrust value from the range [-1, 1] to [0, 255]
            uint8_t thrust = static_cast<uint8_t>((control_values_msg->thrust[thruster_index] + 1) * THRUST_SCALE);

            write_to_i2c(RP2040_ADDRESS, 2, control_values_msg->thruster_map[thruster_index], thrust);
        }

        write_to_i2c(RP2040_ADDRESS, 2, LED1_REGISTER, control_values_msg->led_brightness_1);
        write_to_i2c(RP2040_ADDRESS, 2, LED2_REGISTER, control_values_msg->led_brightness_2);

        // for (size_t i = 0; i < servo_ssh_targets.size(); i++)
        // {
        //   if (servo_ssh_targets[i] != control_values_msg->servo_ssh_targets[i])
        //   {
        //     if (ssh_session_is_active[i])
        //     {
        //       // We are switching to a different ssh target but already have an active ssh session for this servo
        //       // Therefore, close the current ssh session before creating a new one
        //       execute_bash_command("ssh -S /tmp/ssh_control_socket_" + std::to_string(i) + " -O exit " + servo_ssh_targets[i]);
        //     }

        //     // Create a new ssh session
        //     ssh_session_is_active[i] = execute_bash_command("ssh -M -S /tmp/ssh_control_socket_" + std::to_string(i) + " -fnNT " + control_values_msg->servo_ssh_targets[i]);
        //   }
        // }

        // servo_ssh_targets = control_values_msg->servo_ssh_targets;

        // for (size_t i = 0; i < servo_ssh_targets.size(); i++)
        // {
        //   if (ssh_session_is_active[i]) 
        //   {
        //     if (control_values_msg->camera_servos[i])
        //     {
        //       servo_angles[i] += SERVO_ANGLE_INCREMENT * control_values_msg->camera_servos[i];
        //       servo_angles[i] = std::clamp(servo_angles[i], MIN_SERVO_ANGLE, MAX_SERVO_ANGLE);
        //       execute_bash_command("ssh -S /tmp/ssh_control_socket_" + std::to_string(i) + " " + servo_ssh_targets[i] + " python3 servo.py " + std::to_string(servo_angles[i]));
        //     }
        //   }
        // }
      };

    // Open the i2c file
    if ((i2c_file = open(I2C_BUS_FILE, O_RDWR)) < 0) {
      RCLCPP_ERROR(this->get_logger(), "Failed to open the i2c bus");
    }

    control_values_subscriber =
      this->create_subscription<eer_interfaces::msg::BlueStarControl>(
        "bluestar_control_values", 10, control_values_subscriber_callback);  
  }

  // Deconstructor for I2CMaster
  ~I2CMaster()
  { 
    // Close the i2c file
    if(i2c_file >= 0){
      close(i2c_file);
    }

    // CLose active ssh sessions
    // for (size_t i = 0; i < servo_ssh_targets.size(); i++)
    // {
    //   if (ssh_session_is_active[i])
    //   {
    //     execute_bash_command("ssh -S /tmp/ssh_control_socket_" + std::to_string(i) + " -O exit " + servo_ssh_targets[i]);
    //   }
    // }
  }

private:
  rclcpp::Subscription<eer_interfaces::msg::BlueStarControl>::SharedPtr control_values_subscriber;
  // std::array<int, 2> servo_angles = {0, 0};
  // std::array<std::string, 2> servo_ssh_targets = {"", ""};
  // std::array<bool, 2> ssh_session_is_active = {false, false};
  int i2c_file;
  bool previous_i2c_write_failed = false;

  void write_to_i2c(int device_address, uint8_t num_bytes, uint8_t byte_1, uint8_t byte_2 = 0)
  {
    
    // Raise an error if the i2c file is not open
    if (i2c_file < 0) {
      if (!previous_i2c_write_failed)
      {
        RCLCPP_ERROR(this->get_logger(), "I2C bus is not open");
        previous_i2c_write_failed = true;
      }
      return;
    }

    if (ioctl(i2c_file, I2C_SLAVE, device_address) < 0) {
      if (!previous_i2c_write_failed)
      {
        RCLCPP_ERROR(this->get_logger(), "I2C bus is not open");
        previous_i2c_write_failed = true;
      }
      return;
    }

    if (num_bytes == 1) {
      uint8_t buffer[1];
      buffer[0] = byte_1;
      if (write(i2c_file, buffer, 1) != 1) 
      {
        if (!previous_i2c_write_failed)
        {
          RCLCPP_ERROR(this->get_logger(), "Failed to write to the i2c bus");
          previous_i2c_write_failed = true;
        }
        return;
      }
    }
    else if (num_bytes == 2)
    {
      uint8_t buffer[2];
      buffer[0] = byte_1;
      buffer[1] = byte_2;
      if (write(i2c_file, buffer, 2) != 2) 
      {
        if (!previous_i2c_write_failed)
        {
          RCLCPP_ERROR(this->get_logger(), "Failed to write to the i2c bus");
          previous_i2c_write_failed = true;
        }
        return;
      }
    }
    previous_i2c_write_failed = false;
  }

  // bool execute_bash_command(std::string command)
  // {
  //   int ret = system(command.c_str());
  //   if (ret == -1) {
  //     RCLCPP_ERROR(this->get_logger(), "Failed to execute SSH command %s", command.c_str());
  //     return false;
  //   } else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
  //     RCLCPP_ERROR(this->get_logger(), "SSH command %s failed with exit status %d", command.c_str(), WEXITSTATUS(ret));
  //     return false;
  //   }

  //   RCLCPP_INFO(this->get_logger(), "Executed command: %s", command.c_str());

  //   return true;
  // }
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<I2CMaster>());
  rclcpp::shutdown();
  return 0;
}