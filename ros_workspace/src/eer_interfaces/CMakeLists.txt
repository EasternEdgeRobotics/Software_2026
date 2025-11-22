cmake_minimum_required(VERSION 3.8)
project(eer_interfaces)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
	"srv/ListConfig.srv" 
	"srv/GetConfig.srv" 
	"srv/DeleteConfig.srv" 
	"srv/Cameras.srv" 
	"msg/PilotInput.msg" 
	"msg/SaveConfig.msg" 
	"msg/WaterwitchControl.msg" 
	"action/BeaumontAutoMode.action" 
	"srv/HSVColours.srv" 
)
ament_export_dependencies(rosidl_default_runtime)

ament_package()
