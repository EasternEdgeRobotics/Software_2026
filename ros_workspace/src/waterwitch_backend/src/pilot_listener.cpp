#include <memory>
#include <thread>
#include <vector>
#include <cmath>
#include <chrono>
#include <array>
#include <string>
#include <cstdlib>

#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/msg/pilot_input.hpp"
#include "eer_interfaces/msg/waterwitch_control.hpp"
#include "eer_interfaces/srv/get_config.hpp"
#include "waterwitch_constants.h"
#include <nlohmann/json.hpp>

class PilotListener : public rclcpp::Node
{
public:
  PilotListener() : Node("PilotListener")
  {

    // #####################################
    // ########### CONFIGURATION ###########
    // #####################################

    // Default Waterwitch Camera Servo Ips
    current_waterwitch_control_values.servo_ssh_targets = {"picam0.local", "picam1.local"};

    // Default Waterwitch Thruster Mapping to RP2040
    current_waterwitch_control_values.thruster_map = {0,1,2,3,4,5};

    // The mapping in the array above works as follows
    // Index 0: for_star (Forward Right)
    // Index 1: for_port (Forward Left)
    // Index 2: aft_star (Back Right)
    // Index 3: aft_port (Back Left)
    // Index 4: star_top (Top Right)
    // Index 5: port_top (Top Left)

    // Initalize the ros2 service client for obtaining configs
    waterwitch_config_client = this->create_client<eer_interfaces::srv::GetConfig>("get_config");
  
    // Wait until the config manager is initalized
    while (!waterwitch_config_client->wait_for_service(std::chrono::seconds(1))) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service. Exiting.");
            return;
            rclcpp::shutdown(); 
        }
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "config manager service not available, waiting again...");
    }

    // ###################################
    // ############ THRUSTERS ############
    // ###################################

    // Ensure that the current thrust and target_thrust_values are initialized to 0
    current_waterwitch_control_values.thrust.fill(0.0f);
    target_thrust_values.fill(0.0f);

    // Create a subscriber on the pilot input topic. The pilot_listener_callback method will be called whenever a new message is received.
    pilot_listener = this->create_subscription<eer_interfaces::msg::PilotInput>(
      "pilot_input", 10, std::bind(&PilotListener::pilot_listener_callback, this, std::placeholders::_1));


    // #####################################
    // ############ ROV CONTROL ############
    // #####################################

    // Create a publisher on the waterwitch_control_values topic
    waterwitch_control_publisher = this->create_publisher<eer_interfaces::msg::WaterwitchControl>("waterwitch_control_values", 10);

    // Create a timer that will call the software_to_board_communication_timer_callback method at a fixed rate
    software_to_board_communication_timer = this->create_wall_timer(std::chrono::milliseconds(SOFTWARE_TO_BOARD_COMMUNICATION_RATE), 
      std::bind(&PilotListener::software_to_board_communication_timer_callback, this));
  }

private:
  rclcpp::Client<eer_interfaces::srv::GetConfig>::SharedPtr waterwitch_config_client;
  rclcpp::Subscription<eer_interfaces::msg::PilotInput>::SharedPtr pilot_listener;
  rclcpp::Publisher<eer_interfaces::msg::WaterwitchControl>::SharedPtr waterwitch_control_publisher;
  rclcpp::TimerBase::SharedPtr software_to_board_communication_timer;

  std::array<std::mutex, 6> target_thrust_mutexes;
  std::mutex current_waterwitch_control_values_mutex;

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
  eer_interfaces::msg::WaterwitchControl current_waterwitch_control_values;

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
  std::chrono::time_point<std::chrono::high_resolution_clock> last_input_recieved_time{};

  void pilot_listener_callback(eer_interfaces::msg::PilotInput::UniquePtr pilot_input)
  { 

    {
      std::lock_guard<std::mutex> lock(current_waterwitch_control_values_mutex);

      last_input_recieved_time = std::chrono::high_resolution_clock::now();

      // This would be a part of the constructor, but since `get_waterwitch_config` contains
      // a call to shared_from_this(), we need to call this in the `pilot_listener_callback` to avoid runtime error
      if (!inital_configuration_set)
      {
        // Get configs
        get_waterwitch_config();
        inital_configuration_set = true;
      }
    }

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

    // Lambda function to compute thrust value for each thruster
    auto compute_thrust_value = [this](int thruster_index, const eer_interfaces::msg::PilotInput* pilot_input, std::array<float, 6>& target_thrust_values) 
    {
      // Ensure that the software_to_board_communication_timer_callback method does not access the target_thrust_values while they are being updated
      std::lock_guard<std::mutex> lock(target_thrust_mutexes[thruster_index]);

      // If we are in single thruster configuration mode, ignore other thruster indices and only move the selected thruster in the "positive" direction
      if (single_thruster_configuration_mode && pilot_input->configuration_mode_thruster_number != thruster_index) {
        return;
      } else if (single_thruster_configuration_mode) {
        target_thrust_values[thruster_index] = (pilot_input->surge ? std::copysign(1.0f, pilot_input->surge) : 0.0f) * thruster_direction[thruster_index];
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
      }
    }; 

    std::vector<std::thread> threads;

    for (int thruster_index = 0; thruster_index < 6; thruster_index++) 
    {
      // Compute the thrust value of each thruster in a thread to improve performance
      threads.emplace_back(compute_thrust_value, thruster_index, pilot_input.get(), std::ref(target_thrust_values));
    }

    for (auto& thread : threads) 
    {
      // Ensure all threads have finished executing before continuing
      thread.join();
    }

    // Read any other pilot inputs
    {
      std::lock_guard<std::mutex> lock(current_waterwitch_control_values_mutex);

      // If single_thruster_configuration_mode is going from true to false, get the waterwitch config again
      if (!pilot_input->configuration_mode && single_thruster_configuration_mode)
      {
        get_waterwitch_config();
      }

      single_thruster_configuration_mode = pilot_input->configuration_mode;

      current_waterwitch_control_values.camera_servos[0] = pilot_input->turn_front_servo_cw - pilot_input->turn_front_servo_ccw;
      current_waterwitch_control_values.camera_servos[1] = pilot_input->turn_back_servo_cw - pilot_input->turn_back_servo_ccw;
      current_waterwitch_control_values.bilge_pump_speed = pilot_input->bilge_pump_speed;

      f (pilot_input->brighten_led ) {
        current_waterwitch_control_values.led_brightness = 255;
      }
      else if (pilot_input->dim_led) {
        current_waterwitch_control_values.led_brightness = 0;
      }
    }
  }

  void software_to_board_communication_timer_callback()
  {
    {      
      std::lock_guard<std::mutex> lock(current_waterwitch_control_values_mutex);

      if (std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - last_input_recieved_time).count() >= PILOT_COMMUNICATION_LOSS_THRUSTER_TIMEOUT_MS)
      {
        // Communication has been cut with topsides for too long, stop all thrusters and the pump
        for (int thruster_index = 0; thruster_index < 6; thruster_index++)  {
          target_thrust_values[thruster_index] = 0;
        }
        current_waterwitch_control_values.bilge_pump_speed = 0;
      }

    }

    for (int thruster_index = 0; thruster_index < 6; thruster_index++) {
      // Ensure varaibles are not updated as they are being accessed
      std::scoped_lock lock(target_thrust_mutexes[thruster_index], current_waterwitch_control_values_mutex);

      // Only update the thrust value of each thruster if the target value is different
      if (target_thrust_values[thruster_index] != current_waterwitch_control_values.thrust[thruster_index]) 
      {
        float difference = target_thrust_values[thruster_index] - current_waterwitch_control_values.thrust[thruster_index];

        if (abs(difference) > thruster_acceleration) 
        { 
          current_waterwitch_control_values.thrust[thruster_index] += std::copysign(difference * thruster_acceleration, difference);
        } 
        else
        {
          current_waterwitch_control_values.thrust[thruster_index] = target_thrust_values[thruster_index];
        }
      }
    }

    // Publish the waterwitch control values
    {
      std::lock_guard<std::mutex> lock(current_waterwitch_control_values_mutex);
      waterwitch_control_publisher->publish(current_waterwitch_control_values);
    }
  }

  void get_waterwitch_config()
  {
    // Make a request for the waterwitch_config
    auto request = std::make_shared<eer_interfaces::srv::GetConfig::Request>();
    request->name = "waterwitch_config";

    // Send the request to this config client
    auto result = waterwitch_config_client->async_send_request(request, 
      [this](rclcpp::Client<eer_interfaces::srv::GetConfig>::SharedFuture future) {
          auto response = future.get();

          // Check if the response is empty
          if (response->config.empty()) {
            RCLCPP_INFO(this->get_logger(), "Waterwitch backend recieved empty config response");
            return;
          } 

          try {
              nlohmann::json configuration_data = nlohmann::json::parse(response->config);

              if (configuration_data.contains("servos") && configuration_data["servos"].is_array()) {
                  for (size_t i = 0; i < current_waterwitch_control_values.servo_ssh_targets.size(); ++i) {
                      if (i < configuration_data["servos"].size() && !configuration_data["servos"][i].is_null()) {
                          current_waterwitch_control_values.servo_ssh_targets[i] = configuration_data["servos"][i].get<std::string>();
                      }
                  }
              }

              if (configuration_data.contains("thruster_acceleration") && !configuration_data["thruster_acceleration"].is_null() && configuration_data["thruster_acceleration"].is_number_float())
              {
                thruster_acceleration = configuration_data["thruster_acceleration"];
              }

              if (configuration_data.contains("thruster_stronger_side_attenuation_constant") && !configuration_data["thruster_stronger_side_attenuation_constant"].is_null() && configuration_data["thruster_stronger_side_attenuation_constant"].is_number_float())
              {
                thruster_stronger_side_attenuation_constant = configuration_data["thruster_stronger_side_attenuation_constant"];
              }

              // Ensure that the config related to thruster configuration is valid
              bool valid_thruster_configuration = true;
              if (!(configuration_data.contains("thruster_map") && configuration_data.contains("reverse_thrusters") && configuration_data.contains("stronger_side_positive"))) {
                  valid_thruster_configuration = false;
              } else {
                  for (size_t i = 0; i < current_waterwitch_control_values.thruster_map.size(); i++) {
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
                  for (size_t i = 0; i < current_waterwitch_control_values.thruster_map.size(); i++) {
                      current_waterwitch_control_values.thruster_map[i] = static_cast<int8_t>(std::abs(configuration_data["thruster_map"][i].get<int>()));
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

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto pilot_listener_node = std::make_shared<PilotListener>();
  
  // Use a multi-threaded executor to improve performance since pilot_listener is thread-safe
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(pilot_listener_node);
  executor.spin();

  rclcpp::shutdown();
  return 0;
}