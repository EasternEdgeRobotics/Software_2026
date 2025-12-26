# ROS Workspace

This README.md details how to setup the ROS Worksapce.

## Compiling directly

1. Ensure that you are running Ubuntu 24.04 (Noble Numbat) on your computer or in a virtualized environment

2. Install [ROS2 Jazzy](https://docs.ros.org/en/jazzy/Releases/Release-Jazzy-Jalisco.html)

3. Install necessary dependencies for our ROS2 workspace. You can look through the [Dockerfile](./Dockerfile) for the list of dependancies.

4. Run `colcon build` in `ros_workspace`

5. Source the workspace by running `source /opt/ros/jazzy/setup.bash` and `ros_workspace/install/setup.bash`


You can choose the backend launch file to run based on the ROV (ex. Waterwitch or Beaumont) and whether or not you are using the [simulation_environment](https://github.com/EasternEdgeRobotics/rov-sim). Once chosen, you can run:
```
ros2 launch <package name> <launch file>
```
or
```
ros2 run <package name> <ros2 node>
```

If you make changes and would like to recompile, restart from step 4.

## Setting up on Docker

1. Install [Docker](https://www.docker.com/) and ensure you can use `docker compose`
 
2. Run the following in the root of the repository
```
docker compose up 
```

If you make changes and would like to recompile:
```
docker compose up ---build 
```
## Simulation Environment

EER has a [simulation environment](https://github.com/EasternEdgeRobotics/rov-sim) that was integrated with the [2025 Software](https://github.com/EasternEdgeRobotics/Software_2025). 

Porting this simulation environment requires:
- Creating plugins and an associated ROV model in the [simulation environment](https://github.com/EasternEdgeRobotics/rov-sim) to mimic Bluestar.
- Writing a replacement for [pilot_listener.cpp](./src/bluestar_backend/src/pilot_listener.cpp) that does the same thing but publishes to Gazebo simulation topics instead of UART.
