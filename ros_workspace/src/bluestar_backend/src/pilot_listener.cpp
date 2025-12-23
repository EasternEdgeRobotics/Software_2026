#include <memory>
#include <thread>
#include <vector>
#include <cmath>
#include <chrono>
#include <array>
#include <string>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/msg/pilot_input.hpp"
#include "eer_interfaces/srv/get_config.hpp"
#include <nlohmann/json.hpp>
#include <termios.h>

extern "C" {
  #include "minihdlc/minihdlc.h"
}

#define UART_DEVICE "/dev/ttyUSB0" // Assuming we are using the first plugged-in USB-to-Serial adapter to communicate to ROV
#define BAUDRATE B115200

#define SERVO_ANGLE_INCREMENT 10
#define PRECISION_CONTROL_DC_MOTOR_ANGLE_INCREMENT 10
#define LED_BRIGHTNESS_INCREMENT 10

// Command IDs (see the main.c in the firmware for reference on command structure to the ROV)
#define SET_THRUST_COMMAND_ID 0x00
#define SET_LED_BRIGHTNESS_COMMAND_ID 0x06
#define SET_SERVO_POSITION_COMMAND_ID 0x08
#define SET_MOTOR_SPEED_COMMAND_ID 0x0C
#define SET_MOTOR_SPEED_REVERSE_COMMAND_ID 0x10
#define SET_PRECISION_CONTROL_DC_MOTOR_PARAMETERS_COMMAND_ID 0x14
#define SET_PRECISION_CONTROL_DC_MOTOR_ANGLE_COMMAND_ID 0x18
#define SET_THRUSTER_ACCELERATION_COMMAND_ID 0x1A
#define SET_THRUSTER_TIMEOUT_COMMAND_ID 0x1B

#define DC_MOTOR_FORWARD_DIRECTION false
#define DC MOTOR_REVERSE_DIRECTION true

const int8_t THRUSTER_CONFIG_MATRIX[6][6] = {
    {-1,-1, 0, 0, 0, -1}, // for star
    {-1, 1, 0, 0, 0, 1}, // for port
    {1, -1, 0, 0, 0, 1}, // aft star
    {1, 1, 0, 0, 0, -1}, // aft port
    {0, 0, 1, 0, 1, 0}, // star top
    {0, 0, 1, 0, -1, 0} // port top
};

enum CONTROL_AXES {
    SURGE = 0,
    SWAY = 1,
    HEAVE = 2,
    PITCH = 3,
    ROLL = 4,
    YAW = 5
};

class PilotListener : public rclcpp::Node
{
public:
  PilotListener() : Node("PilotListener") {

    // #####################################
    // ########### CONFIGURATION ###########
    // #####################################

    // The mapping in the array above works as follows
    // Index 0: for_star (Forward Right)
    // Index 1: for_port (Forward Left)
    // Index 2: aft_star (Back Right)
    // Index 3: aft_port (Back Left)
    // Index 4: star_top (Top Right)
    // Index 5: port_top (Top Left)

    // Initalize the ros2 service client for obtaining configs
    bluestar_config_client = this->create_client<eer_interfaces::srv::GetConfig>("get_config");
  
    // Wait until the config manager is initalized
    while (!bluestar_config_client->wait_for_service(std::chrono::seconds(1))) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(rclcpp::get_logger("pilot_listener"), "Interrupted while waiting for the service. Exiting.");
            return;
            rclcpp::shutdown(); 
        }
        RCLCPP_INFO(rclcpp::get_logger("pilot_listener"), "config manager service not available, waiting again...");
    }

    initializeUART();
    minihdlc_init(uart_send_char, frame_received);

    // ###################################
    // ############ THRUSTERS ############
    // ###################################

    // Ensure that the current thrust and target_thrust_values are initialized to 0
    target_thrust_values.fill(0.0f);

    // Create a subscriber on the pilot input topic. The pilot_listener_callback method will be called whenever a new message is received.
    pilot_listener = this->create_subscription<eer_interfaces::msg::PilotInput>(
      "pilot_input", 10, std::bind(&PilotListener::pilot_listener_callback, this, std::placeholders::_1));
  }

  static int uart_fd;
  static bool previous_uart_write_failed;

  // Linux already has a driver for USB-to-Serial adapters which is configurable with the termios API (we configure it here to match UART)
  // Termios is a POSIX (Portable Operating System Interface) standard for configuring serial ports on UNIX systems
  // More details on all the flags can be found via `man termios`
  // This tutorial explains everything: https://www.xanthium.in/native-serial-port-communication-arduino-micro-linux-unix-bsd-system-c-lang-terminos-api 
  static void initializeUART() {

    // Open the UART device file descriptor in read/write mode + some other flags
    // NOTE: Ensure that the user running this program has read/write permissions to the device file (e.g., via `sudo usermod -a -G dialout $USER` and then logging out and back in)
    if ((uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
      RCLCPP_ERROR(rclcpp::get_logger("pilot_listener"), "Failed to open the uart device");
    } else {

      // Comments are based on my understanding from the man page
      struct termios options;

      // use the options struct to set UART parameters
      tcgetattr(uart_fd, &options);
      
      // Set input and output baud rates
      cfsetispeed(&options, BAUDRATE);
      cfsetospeed(&options, BAUDRATE);

      // Local mode flags
      options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Disable canonical mode (to not convert lowercase to uppercase etc), echo, erase characters, and signal chars (e.g., ctrl-c)

      // Input mode flags
      // Here we disable software flow control (e.g., ASCII for ctrl-s, ctrl-q) and
      // the feature that alllows any character to restart output (IXANY)
      options.c_iflag &= ~(IXON | IXOFF | IXANY);

      // Control mode flags
      options.c_cflag &= ~CRTSCTS; // Disable hardware flow control (RTS/CTS)
      options.c_cflag |= (CLOCAL | CREAD); // Ignore modem control lines (i.e., use 1-to-1 serial communication) and enable receiver
      // Set mode to 8N1 (8 data bits, no parity, 1 stop bit), suitable for UART  
      options.c_cflag &= ~PARENB; // Disable parity generation
      options.c_cflag &= ~CSTOPB; // Send only one stop bit (not two)
      options.c_cflag &= ~CSIZE; // Clear current data size setting
      options.c_cflag |= CS8; // Set data size to 8 bits

      // Output mode flags
      // Disable implementation-defined output processing (send every-byte as is)
      options.c_oflag &= ~OPOST;
      
      // Set read to return immediately with any available data OR with a timeout of 10 deciseconds (1 second)
      // This makes read() non-blocking
      options.c_cc[VMIN] = 0; 
      options.c_cc[VTIME] = 10;
      
      tcflush(uart_fd, TCIFLUSH); // Discard data that was received but not read yet after changing settings
      tcsetattr(uart_fd, TCSANOW, &options); // Apply the settings immediately
    }
  }

  // We never call this directly, minihdlc calls this when we call minihdlc_send_frame
  static void uart_send_char(uint8_t data) {
    
    // Error out if UART device is not open
    if (uart_fd < 0) {
      if (!previous_uart_write_failed) {
        RCLCPP_ERROR(rclcpp::get_logger("pilot_listener"), "UART device is not open");
        previous_uart_write_failed = true;
      }
      return;
    }

    // Write a single byte to the UART device and error if it fails (write returns the number of bytes written)
    if (write(uart_fd, &data, 1) != 1) {
      if (!previous_uart_write_failed)
      {
        RCLCPP_ERROR(rclcpp::get_logger("pilot_listener"), "Failed to write to UART device");
        previous_uart_write_failed = true;
      }
    } else {
      previous_uart_write_failed = false;
    }
  }

  // We never call this directly, minihdlc calls this when we call minihdlc_char_reciever enough times to form a frame in our receive logic
  static void frame_received(const uint8_t *frame_buffer, uint16_t frame_length)
  {
    return; // We don't handle recieving anything from rov at the moment (in fact, we don't have recieve logic where we call minihdlc_char_receiver)
  }

private:
  rclcpp::Client<eer_interfaces::srv::GetConfig>::SharedPtr bluestar_config_client;
  rclcpp::Subscription<eer_interfaces::msg::PilotInput>::SharedPtr pilot_listener;

  std::mutex uart_write_mutex;

  std::array<float, 6> target_thrust_values;
  std::array<float, 7> thruster_multipliers = {
    100,  // power
    100,  // surge
    100,  // sway
    100,  // heave
    100,  // pitch (not used)
    100,  // roll
    100   // yaw
  };
  std::array<uint8_t, 6> thruster_map = {0,1,2,3,4,5};

  bool inital_configuration_set = false;
  bool single_thruster_configuration_mode = false;

  // Indicates if the corresponding thruster's direction should be flipped 
  std::array<int8_t, 6> thruster_direction = {    
    1, // for_star
    1, // for_port
    1, // aft_star
    1, // aft_port
    1, // star_top
    1 //  port_top
  };

  // Indicates if the corresponding thruster's positive direction is stronger  
  std::array<bool, 6> stronger_side_positive = {    
    false, // for_star
    false, // for_port
    false, // aft_star
    false, // aft_port
    false, // star_top
    false //  port_top
  };

  float thruster_acceleration = 0.5f;
  float thruster_stronger_side_attenuation_constant = 1.0f;

  // Firmware should set default servo positions on startup as they are set here (so that servos or cameras don't jerk to a different position on first communication)
  std::array<uint8_t, 4> target_servo_angle = {127, 127, 127, 127}; // Default servo positions (middle position)
  std::array<uint8_t, 2> target_precision_control_dc_motor_angle = {127, 127}; // Default DC motor angles
  std::array<uint8_t, 2> led_brightness = {127, 127}; // Default LED brightness (half brightness)



  void pilot_listener_callback(eer_interfaces::msg::PilotInput::UniquePtr pilot_input)
  { 

    // This would be a part of the constructor, but since `get_bluestar_config` contains
    // a call to shared_from_this(), we need to call this in the `pilot_listener_callback` to avoid runtime error
    if (!inital_configuration_set)
    {
      // Get configs
      get_bluestar_config();
      inital_configuration_set = true;
    }

    send_thrust_commands(pilot_input.get());
    
    send_led_brightness_commands(pilot_input.get());

    send_servo_angle_commands(pilot_input.get());

    send_dc_motor_speed_commands(pilot_input.get());

    send_precision_control_dc_motor_parameters_commands(pilot_input.get());

    send_precision_control_dc_motor_angle_commands(pilot_input.get());

    if (pilot_input->set_thruster_acceleration) {
      uint8_t set_thruster_acceleration_command[2] = {
        SET_THRUSTER_ACCELERATION_COMMAND_ID, 
        pilot_input->thruster_acceleration
      }; 
      minihdlc_send_frame(set_thruster_acceleration_command, 2);
    }

    if (pilot_input->set_thruster_timeout) {
      // Use little-endian format to send the thrsuter timeout value in two bytes
      uint8_t set_thruster_timeout_command[3] = {
        SET_THRUSTER_TIMEOUT_COMMAND_ID, 
        static_cast<uint8_t>(pilot_input->thruster_timeout & 0xFF), 
        static_cast<uint8_t>((pilot_input->thruster_timeout >> 8) & 0xFF)
      }; 
      minihdlc_send_frame(set_thruster_timeout_command, 3);
    }

  }

  void send_thrust_commands(eer_interfaces::msg::PilotInput* pilot_input) {

    // Check if thrust is being controlled using a bool input
    if (pilot_input->heave_up | pilot_input->heave_down) 
      pilot_input->heave = (pilot_input->heave_up - pilot_input->heave_down) * 100;
    if (pilot_input->pitch_up | pilot_input->pitch_down) 
      pilot_input->pitch = (pilot_input->pitch_up - pilot_input->pitch_down) * 100;
    if (pilot_input->roll_cw | pilot_input->roll_ccw) 
      pilot_input->roll = (pilot_input->roll_cw - pilot_input->roll_ccw) * 100;

    // Apply the multipliers 
    float overall_multiplier = pilot_input->power_multiplier * 0.0001f;
    pilot_input->surge = pilot_input->surge * pilot_input->surge_multiplier * overall_multiplier;
    pilot_input->sway = pilot_input->sway * pilot_input->sway_multiplier * overall_multiplier;
    pilot_input->heave = pilot_input->heave * pilot_input->heave_multiplier * overall_multiplier;
    pilot_input->pitch = pilot_input->pitch * pilot_input->pitch_multiplier * overall_multiplier;
    pilot_input->roll = pilot_input->roll * pilot_input->roll_multiplier * overall_multiplier;
    pilot_input->yaw = pilot_input->yaw * pilot_input->yaw_multiplier * overall_multiplier;

    // If single_thruster_configuration_mode is going from true to false, get the bluestar config again
    if (!pilot_input->configuration_mode && single_thruster_configuration_mode)
    {
      get_bluestar_config();
    }

    single_thruster_configuration_mode = pilot_input->configuration_mode;

    // Lambda function to compute thrust value for each thruster
    auto compute_and_send_thrust_value = [this](int thruster_index, const eer_interfaces::msg::PilotInput* pilot_input, std::array<float, 6>& target_thrust_values) 
    {
      // If we are in single thruster configuration mode, ignore other thruster indices and only move the selected thruster based on the sign of surge input
      if (single_thruster_configuration_mode && pilot_input->configuration_mode_thruster_number != thruster_index) {
        return;
      } else if (single_thruster_configuration_mode) {
        target_thrust_values[thruster_index] = (pilot_input->surge ? std::copysign(1.0f, pilot_input->surge) : 0.0f) * thruster_direction[thruster_index];

        // Depending on whether the stornger side is positive or negative, attenuate the thrust value
        if (target_thrust_values[thruster_index] > 0.0f && stronger_side_positive[thruster_index]) target_thrust_values[thruster_index] = target_thrust_values[thruster_index] * thruster_stronger_side_attenuation_constant;
        else if (target_thrust_values[thruster_index] < 0.0f && !stronger_side_positive[thruster_index]) target_thrust_values[thruster_index] = target_thrust_values[thruster_index] * thruster_stronger_side_attenuation_constant;
        
      } else {
        // Calculate the thrust value based on pilot input and configuration matrix
        float thrust_value = ((pilot_input->surge * THRUSTER_CONFIG_MATRIX[thruster_index][SURGE] +
                              pilot_input->sway * THRUSTER_CONFIG_MATRIX[thruster_index][SWAY] +
                              pilot_input->heave * THRUSTER_CONFIG_MATRIX[thruster_index][HEAVE] +
                              pilot_input->pitch * THRUSTER_CONFIG_MATRIX[thruster_index][PITCH] +
                              pilot_input->roll * THRUSTER_CONFIG_MATRIX[thruster_index][ROLL] +
                              pilot_input->yaw * THRUSTER_CONFIG_MATRIX[thruster_index][YAW]) / 100.0f) * thruster_direction[thruster_index];

        if (thrust_value > 0.0f && stronger_side_positive[thruster_index]) thrust_value = thrust_value * thruster_stronger_side_attenuation_constant;
        else if (thrust_value < 0.0f && !stronger_side_positive[thruster_index]) thrust_value = thrust_value * thruster_stronger_side_attenuation_constant;

        // Clamp the thrust value between -1.0 and 1.0 
        target_thrust_values[thruster_index] = std::max(-1.0f, std::min(thrust_value, 1.0f));

        // Send thrust value to ROV 
        {
          std::lock_guard<std::mutex> lock(this->uart_write_mutex);
          uint8_t set_thrust_command[2] = {
            static_cast<uint8_t>(SET_THRUST_COMMAND_ID + thruster_map[thruster_index]), 
            static_cast<uint8_t>(std::round((target_thrust_values[thruster_index] + 1.0f) * 127.5f))}; 
          minihdlc_send_frame(set_thrust_command, 2);
        }
      }
    }; 

    std::vector<std::thread> threads;

    for (int thruster_index = 0; thruster_index < 6; thruster_index++) 
    {
      // Compute the thrust value of each thruster in a thread to improve performance
      threads.emplace_back(compute_and_send_thrust_value, thruster_index, pilot_input, std::ref(target_thrust_values));
    }

    for (auto& thread : threads) 
    {
      // Ensure all threads have finished executing before continuing
      thread.join();
    }
  }

  void send_led_brightness_commands(const eer_interfaces::msg::PilotInput* pilot_input) {
    for (int led_index = 0; led_index < 2; led_index++) {

      uint8_t set_led_brightness_command[2];
      set_led_brightness_command[0] = SET_LED_BRIGHTNESS_COMMAND_ID + led_index; 

      if (pilot_input->brighten_led[led_index]) {
        set_led_brightness_command[1] = std::min<int>(led_brightness[led_index] + LED_BRIGHTNESS_INCREMENT, 255); // Prevent wraparound
        minihdlc_send_frame(set_led_brightness_command, 2);
        led_brightness[led_index] = set_led_brightness_command[1];
      } else if (pilot_input->dim_led[led_index]) {
        set_led_brightness_command[1] = std::max<int>(led_brightness[led_index] - LED_BRIGHTNESS_INCREMENT, 0);
        minihdlc_send_frame(set_led_brightness_command, 2);
        led_brightness[led_index] = set_led_brightness_command[1];
      } 
    } 
  }

  void send_servo_angle_commands(const eer_interfaces::msg::PilotInput* pilot_input) {
    for (int servo_index = 0; servo_index < 4; servo_index++) {

      uint8_t set_servo_angle_command[2]; 
      set_servo_angle_command[0] = SET_SERVO_POSITION_COMMAND_ID + servo_index; 

      if (pilot_input->set_servo_angle[servo_index]) {
        set_servo_angle_command[1] = pilot_input->servo_angle[servo_index];
        target_servo_angle[servo_index] = set_servo_angle_command[1];
        minihdlc_send_frame(set_servo_angle_command, 2);
      } else if (pilot_input->turn_servo_ccw[servo_index]) {
        set_servo_angle_command[1] = std::min<int>(target_servo_angle[servo_index] + SERVO_ANGLE_INCREMENT, 255); 
        target_servo_angle[servo_index] = set_servo_angle_command[1];
        minihdlc_send_frame(set_servo_angle_command, 2);
      } else if (pilot_input->turn_servo_cw[servo_index]) {
        set_servo_angle_command[1] = std::max<int>(target_servo_angle[servo_index] - SERVO_ANGLE_INCREMENT, 0); 
        target_servo_angle[servo_index] = set_servo_angle_command[1];
        minihdlc_send_frame(set_servo_angle_command, 2);
      }
    } 
  }

  void send_dc_motor_speed_commands(const eer_interfaces::msg::PilotInput* pilot_input) {
    for (int dc_motor_index = 0; dc_motor_index < 4; dc_motor_index++) {
      if (pilot_input->motor_direction[dc_motor_index] == DC_MOTOR_FORWARD_DIRECTION) {
        uint8_t set_dc_motor_speed_and_direction_command[2] = {
          static_cast<uint8_t>(SET_MOTOR_SPEED_COMMAND_ID + dc_motor_index), 
          pilot_input->motor_speed[dc_motor_index] 
        }; 
        minihdlc_send_frame(set_dc_motor_speed_and_direction_command, 2);
      } else {        
        uint8_t set_dc_motor_speed_and_direction_command[2] = {
          static_cast<uint8_t>(SET_MOTOR_SPEED_REVERSE_COMMAND_ID + dc_motor_index), 
          pilot_input->motor_speed[dc_motor_index] 
        }; 
        minihdlc_send_frame(set_dc_motor_speed_and_direction_command, 2);
      }
    }  
  }

  void send_precision_control_dc_motor_parameters_commands(const eer_interfaces::msg::PilotInput* pilot_input) {
    // Only two DC motors can be used for precision control at a time
    for (int pcdcm_index = 0; pcdcm_index < 2; pcdcm_index++) {
      if (pilot_input->set_precision_control_dc_motor_parameters[pcdcm_index]) {
        if (pilot_input->associated_dc_motor_number[pcdcm_index] > 3) {
          RCLCPP_ERROR(this->get_logger(), "Invalid DC motor number for precision control configuration");
          continue;
        }
        uint8_t configure_precision_control_dc_motor_command[14];
        configure_precision_control_dc_motor_command[0] = SET_PRECISION_CONTROL_DC_MOTOR_PARAMETERS_COMMAND_ID + pilot_input->associated_dc_motor_number[pcdcm_index];
        configure_precision_control_dc_motor_command[1] = pilot_input->control_loop_period[pcdcm_index];
        std::memcpy(&configure_precision_control_dc_motor_command[2], pilot_input->proportional_gain.data(), 4);
        std::memcpy(&configure_precision_control_dc_motor_command[6], pilot_input->integral_gain.data(), 4);
        std::memcpy(&configure_precision_control_dc_motor_command[10], pilot_input->derivative_gain.data(), 4);
        minihdlc_send_frame(configure_precision_control_dc_motor_command, 14);
      }
    }
  }

  void send_precision_control_dc_motor_angle_commands(const eer_interfaces::msg::PilotInput* pilot_input) {
    for (int pcdcm_index = 0; pcdcm_index < 2; pcdcm_index++) {

      uint8_t set_pcdm_angle_command[2]; 
      set_pcdm_angle_command[0] = SET_PRECISION_CONTROL_DC_MOTOR_ANGLE_COMMAND_ID + pcdcm_index; 

      if (pilot_input->set_precision_control_dc_motor_parameters[pcdcm_index]) {
        set_pcdm_angle_command[1] = pilot_input->precision_control_dc_motor_angle[pcdcm_index];
        target_precision_control_dc_motor_angle[pcdcm_index] = set_pcdm_angle_command[1];
        minihdlc_send_frame(set_pcdm_angle_command, 2);
      } else if (pilot_input->turn_servo_ccw[pcdcm_index]) {
        set_pcdm_angle_command[1] = std::min<int>(target_precision_control_dc_motor_angle[pcdcm_index] + PRECISION_CONTROL_DC_MOTOR_ANGLE_INCREMENT, 255); 
        target_precision_control_dc_motor_angle[pcdcm_index] = set_pcdm_angle_command[1];
        minihdlc_send_frame(set_pcdm_angle_command, 2);
      } else if (pilot_input->turn_servo_cw[pcdcm_index]) {
        set_pcdm_angle_command[1] = std::max<int>(target_precision_control_dc_motor_angle[pcdcm_index] - PRECISION_CONTROL_DC_MOTOR_ANGLE_INCREMENT, 0); 
        target_precision_control_dc_motor_angle[pcdcm_index] = set_pcdm_angle_command[1];
        minihdlc_send_frame(set_pcdm_angle_command, 2);
      }
    } 
  }


  void get_bluestar_config()
  {
    // Make a request for the bluestar_config
    auto request = std::make_shared<eer_interfaces::srv::GetConfig::Request>();
    request->name = "bluestar_config";

    // Send the request to this config client
    auto result = bluestar_config_client->async_send_request(request, 
      [this](rclcpp::Client<eer_interfaces::srv::GetConfig>::SharedFuture future) {
          auto response = future.get();

          // Check if the response is empty
          if (response->config.empty()) {
            RCLCPP_INFO(this->get_logger(), "BlueStar backend recieved empty config response");
            return;
          } 

          try {
              nlohmann::json configuration_data = nlohmann::json::parse(response->config);

              if (configuration_data.contains("thruster_stronger_side_attenuation_constant") && !configuration_data["thruster_stronger_side_attenuation_constant"].is_null() && configuration_data["thruster_stronger_side_attenuation_constant"].is_number_float())
              {
                thruster_stronger_side_attenuation_constant = configuration_data["thruster_stronger_side_attenuation_constant"];
              }

              // Ensure that the config related to thruster configuration is valid
              bool valid_thruster_configuration = true;
              if (!(configuration_data.contains("thruster_map") && configuration_data.contains("reverse_thrusters") && configuration_data.contains("stronger_side_positive"))) {
                  valid_thruster_configuration = false;
              } else {
                  for (size_t i = 0; i < thruster_map.size(); i++) {
                      if (i >= configuration_data["thruster_map"].size() || configuration_data["thruster_map"][i].is_null() || !configuration_data["thruster_map"][i].is_number_integer()
                          || i >= configuration_data["reverse_thrusters"].size() || configuration_data["reverse_thrusters"][i].is_null() || !configuration_data["reverse_thrusters"][i].is_boolean()
                          || i >= configuration_data["stronger_side_positive"].size() || configuration_data["stronger_side_positive"][i].is_null() || !configuration_data["stronger_side_positive"][i].is_boolean()
                        ) {
                          valid_thruster_configuration = false;
                          
                          break; // Exit the loop early if an invalid entry is found
                      }
                  }
              }
              if (valid_thruster_configuration) {
                  for (size_t i = 0; i < thruster_map.size(); i++) {
                      thruster_map[i] = static_cast<int8_t>(std::abs(configuration_data["thruster_map"][i].get<int>()));
                      thruster_direction[i] = configuration_data["reverse_thrusters"][i].get<bool>() ? -1: 1;
                      stronger_side_positive[i] = configuration_data["stronger_side_positive"][i].get<bool>();
                  }
              }
              
          } catch (const nlohmann::json::parse_error& e) {
              RCLCPP_ERROR(this->get_logger(), "JSON parse error: %s", e.what());
          }
      });

  if (!result.valid()) {
      RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Failed to call service get_configs");
  }

  }
};

int PilotListener::uart_fd = -1;
bool PilotListener::previous_uart_write_failed = false;

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto pilot_listener_node = std::make_shared<PilotListener>();
  // Use a multi-threaded executor to improve performance since pilot_listener is thread-safe
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(pilot_listener_node);
  executor.spin();

  if (PilotListener::uart_fd >= 0) {
    close(PilotListener::uart_fd);
  }

  rclcpp::shutdown();
  return 0;
}