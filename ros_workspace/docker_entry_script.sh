#!/bin/bash

# Source ROS setup
source /opt/ros/jazzy/setup.sh

# Default ROV name to "waterwitch" if not specified
ROV_NAME=$1
DEV_MODE=$2

if [ ! -e /app/install/setup.sh ]
then 
  cd /app
  colcon build
fi

source /app/install/setup.sh

if [ $ROV_NAME == "beaumont" ]; then
  if [ $DEV_MODE == "true" ]; then
    ros2 launch beaumont_backend simulation_beaumont_startup.xml
  else
    ros2 launch beaumont_backend beaumont_startup.xml
  fi
else
  if [ $DEV_MODE == "true" ]; then
    ros2 launch waterwitch_backend simulation_waterwitch_startup.xml
  else
    ros2 launch waterwitch_backend waterwitch_startup.xml
  fi
fi
