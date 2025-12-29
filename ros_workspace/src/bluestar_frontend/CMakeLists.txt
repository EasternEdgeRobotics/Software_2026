cmake_minimum_required(VERSION 3.25)
project(bluestar_frontend)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(eer_interfaces REQUIRED) 
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV REQUIRED)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_subdirectory(include/glfw)
add_subdirectory(include/json)

include_directories(
  ${eer_interfaces_INCLUDE_DIRS} 
  ${OPENGL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  include/glfw/include
  include/imgui
  include/imgui/backends
  include/bluestar_frontend
  include
  include/stb
)

set(IMGUI_SOURCES
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_demo.cpp
    include/imgui/backends/imgui_impl_glfw.cpp
    include/imgui/backends/imgui_impl_opengl3.cpp
)

# Core library (reusable by autonomy node)
set(CORE_SOURCES
    src/core/ConfigLoader.cpp
    src/core/ROS.cpp
)

# GUI library (frontend-specific)
set(GUI_SOURCES
    src/gui/InputManager.cpp
    src/gui/UIRenderer.cpp
)

# New modular executable
add_executable(bluestar_frontend 
    src/frontend_node.cpp 
    ${CORE_SOURCES}
    ${GUI_SOURCES}
    ${IMGUI_SOURCES}
)

target_link_libraries(bluestar_frontend 
    ${OPENGL_LIBRARIES} 
    ${OpenCV_LIBRARIES}
    glfw
    nlohmann_json::nlohmann_json
    rclcpp::rclcpp
)

set_target_properties(bluestar_frontend PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

ament_target_dependencies(bluestar_frontend eer_interfaces rclcpp)

install(TARGETS bluestar_frontend
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

# Add the launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
