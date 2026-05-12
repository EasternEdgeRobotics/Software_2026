#!/bin/bash

# Source ROS setup
source /opt/ros/jazzy/setup.sh

# Default ROV name to "bluestar" if not specified
ROV_NAME=$1
DEV_MODE=$2

if [ ! -e /app/install/setup.sh ]
then 
  cd /app
  colcon build
fi

source /app/install/setup.sh

if [ $ROV_NAME == "bluestar" ]; then # Mostly keeping this if statement so we have to change less stuff in 27 -PC
  if [ $DEV_MODE == "true" ]; then
    ros2 launch bluestar_backend simulation_bluestar_startup.xml
  else
    ros2 launch bluestar_backend bluestar_startup.xml
  fi
fi
