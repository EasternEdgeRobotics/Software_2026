cmake_minimum_required(VERSION 3.8)
project(bluestar_backend)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(eer_interfaces REQUIRED) 
find_package(rosidl_default_generators REQUIRED)

add_subdirectory(include/json)

# Include directories
include_directories(
  ${eer_interfaces_INCLUDE_DIRS} 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# This file contains some constants used throughout the bluestar backend
# It is a static library so that it can be linked to other executables prior to runtime
add_library(bluestar_constants STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/bluestar_constants.cpp)

add_executable(pilot_listener ${CMAKE_CURRENT_SOURCE_DIR}/src/pilot_listener.cpp)
add_executable(simulation_bot_control ${CMAKE_CURRENT_SOURCE_DIR}/src/simulation_bot_control.cpp)
add_executable(i2c_master ${CMAKE_CURRENT_SOURCE_DIR}/src/i2c_master.cpp)

ament_target_dependencies(simulation_bot_control rclcpp std_msgs eer_interfaces)
ament_target_dependencies(pilot_listener rclcpp std_msgs eer_interfaces)
ament_target_dependencies(i2c_master rclcpp std_msgs eer_interfaces)

target_link_libraries(pilot_listener "${cpp_typesupport_target}" bluestar_constants nlohmann_json::nlohmann_json)
target_link_libraries(simulation_bot_control "${cpp_typesupport_target}" bluestar_constants nlohmann_json::nlohmann_json)
target_link_libraries(i2c_master "${cpp_typesupport_target}" bluestar_constants nlohmann_json::nlohmann_json)

install(TARGETS
  pilot_listener
  simulation_bot_control
  i2c_master
  bluestar_constants
  DESTINATION lib/${PROJECT_NAME}
)

# Add the launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
