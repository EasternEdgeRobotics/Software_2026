#!/bin/bash

# NOTE: This entry script is currently unused

# Source ROS setup
source /opt/ros/jazzy/setup.sh

if [ ! -e /app/install/setup.sh ]
then 
  cd /app
  colcon build
fi

source /app/install/setup.sh

ros2 launch bluestar_backend bluestar_startup.xml