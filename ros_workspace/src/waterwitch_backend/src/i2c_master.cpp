#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <stdexcept>
#include <sys/wait.h> 

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/float64.hpp"
#include "eer_interfaces/msg/waterwitch_control.hpp"
#include "waterwitch_constants.h"

#define THRUST_SCALE 127.5
#define BILGE_PUMP_REGISTER 6
#define LED_REGISTER 7
#define UART_DEVICE "/dev/ttyUSB0"
#define BAUDRATE B115200


class UARTMaster : public rclcpp::Node
{
public:
  UARTMaster() : Node("UARTMaster")
  {

    auto control_values_subscriber_callback =
      [this](eer_interfaces::msg::WaterwitchControl::UniquePtr control_values_msg) -> void {
        for (int thruster_index = 0; thruster_index < 6; thruster_index++) {

            uint8_t thrust = static_cast<uint8_t>((control_values_msg->thrust[thruster_index] + 1) * THRUST_SCALE);

            write_to_uart(control_values_msg->thruster_map[thruster_index], thrust);
        }

        write_to_uart(LED_REGISTER, control_values_msg->led_brightness);
        write_to_uart(BILGE_PUMP_REGISTER, control_values_msg->bilge_pump_speed);

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

    if ((uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
      RCLCPP_ERROR(this->get_logger(), "Failed to open the uart device");
    } else {
      struct termios options;
      tcgetattr(uart_fd, &options);
      
      cfsetispeed(&options, BAUDRATE);
      cfsetospeed(&options, BAUDRATE);
      
      options.c_cflag &= ~PARENB;
      options.c_cflag &= ~CSTOPB;
      options.c_cflag &= ~CSIZE;
      options.c_cflag |= CS8;
      
      options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      options.c_oflag &= ~OPOST;
      options.c_iflag &= ~(IXON | IXOFF | IXANY);
      
      options.c_cflag |= (CLOCAL | CREAD);
      
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = 10;
      
      tcflush(uart_fd, TCIFLUSH);
      tcsetattr(uart_fd, TCSANOW, &options);
    }

    control_values_subscriber =
      this->create_subscription<eer_interfaces::msg::WaterwitchControl>(
        "waterwitch_control_values", 10, control_values_subscriber_callback);  
  }

  ~UARTMaster()
  { 
    if(uart_fd >= 0){
      close(uart_fd);
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
  rclcpp::Subscription<eer_interfaces::msg::WaterwitchControl>::SharedPtr control_values_subscriber;
  // std::array<int, 2> servo_angles = {0, 0};
  // std::array<std::string, 2> servo_ssh_targets = {"", ""};
  // std::array<bool, 2> ssh_session_is_active = {false, false};
  int uart_fd;
  bool previous_uart_write_failed = false;

  void write_to_uart(uint8_t byte_1, uint8_t byte_2 = 0)
  {
    
    if (uart_fd < 0) {
      if (!previous_uart_write_failed)
      {
        RCLCPP_ERROR(this->get_logger(), "UART device is not open");
        previous_uart_write_failed = true;
      }
      return;
    }

    uint8_t buffer[2];
    buffer[0] = byte_1;
    buffer[1] = byte_2;
    if (write(uart_fd, buffer, 2) != 2) 
    {
      if (!previous_uart_write_failed)
      {
        RCLCPP_ERROR(this->get_logger(), "Failed to write to the uart device");
        previous_uart_write_failed = true;
      }
      return;
    }

    previous_uart_write_failed = false;
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
  rclcpp::spin(std::make_shared<UARTMaster>());
  rclcpp::shutdown();
  return 0;
}