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
  ${CMAKE_CURRENT_SOURCE_DIR}/include/json/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/minihdlc
)

# This file contains some constants used throughout the bluestar backend
# It is a static library so that it can be linked to other executables prior to runtime
add_executable(pilot_listener ${CMAKE_CURRENT_SOURCE_DIR}/src/pilot_listener.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/minihdlc/minihdlc.c)

ament_target_dependencies(pilot_listener rclcpp std_msgs eer_interfaces)

install(TARGETS
  pilot_listener
  DESTINATION lib/${PROJECT_NAME}
)

# Add the launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
