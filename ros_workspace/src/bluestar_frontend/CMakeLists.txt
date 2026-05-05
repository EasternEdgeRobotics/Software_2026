cmake_minimum_required(VERSION 3.25)
project(bluestar_frontend)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(eer_interfaces REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED IMPORTED_TARGET
  gstreamer-1.0
  gstreamer-app-1.0
  gstreamer-webrtc-1.0
  gstreamer-sdp-1.0
  gstreamer-video-1.0
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_subdirectory(include/glfw)
add_subdirectory(include/json)
add_subdirectory(include/glad)

include_directories(
  ${eer_interfaces_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIRS}
  include/glfw/include
  include/imgui
  include/imgui/backends
  include/bluestar_frontend
  include/stb
  include/glad/include
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

add_executable(
  bluestar_frontend
  src/main.cpp
  src/Camera.cpp
  src/webrtc_stream.cpp
  ${IMGUI_SOURCES}
)

target_link_libraries(
  bluestar_frontend
  ${OPENGL_LIBRARIES}
  glfw
  glad
  nlohmann_json::nlohmann_json
  rclcpp::rclcpp
  PkgConfig::GST
)

target_compile_options(
  bluestar_frontend
  PRIVATE ${GST_CFLAGS_OTHER}
)

set_target_properties(
  bluestar_frontend
  PROPERTIES
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

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

target_compile_options(
  bluestar_frontend
  PRIVATE -O0 -g -fsanitize=address,undefined
)

target_link_options(
  bluestar_frontend
  PRIVATE -fsanitize=address,undefined
)

ament_package()

target_compile_options(glad PRIVATE -Wno-pedantic)