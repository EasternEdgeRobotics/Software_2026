cmake_minimum_required(VERSION 3.5)
project(common_backend)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(eer_interfaces REQUIRED)

add_executable(config_manager src/ConfigManager.cpp)
ament_target_dependencies(config_manager rclcpp eer_interfaces)

install(TARGETS
  config_manager 
  DESTINATION lib/${PROJECT_NAME})

ament_package()