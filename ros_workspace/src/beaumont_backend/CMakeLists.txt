cmake_minimum_required(VERSION 3.8)
project(beaumont_backend)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rclpy REQUIRED)

# Install Python modules
ament_python_install_package(${PROJECT_NAME})

# Install Python executables 
install(PROGRAMS
  beaumont_backend/i2c_master.py
  beaumont_backend/simulation_bot_control.py
  beaumont_backend/task_manager.py
  DESTINATION lib/${PROJECT_NAME}
)

# Add the launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()