# ROS Workspace

This README.md details how to setup the ROS Worksapce.

## Using Docker Compose (QUICKSTART)

1. Install [Docker](https://www.docker.com/) and ensure you can use `docker compose`
 
2. Run `docker compose -f compose.dev.yaml up` the root of the repository (include `--build` after making changes)

### Viewing the Frontend

The frontend should now be running inside the `bluestar_frontend` Docker container using a virtual framebuffer. To view and interact with the frontend, use a VNC viewer like noVNC.

1. Clone the [noVNC](https://github.com/novnc/noVNC.git) repository
2. Run `./noVNC/utils/novnc_proxy --vnc localhost:5900`


## Compiling Directly (Production)

1. Ensure that you are running Ubuntu 24.04 (Noble Numbat) on your computer or in a virtualized environment

2. Install [ROS2 Jazzy](https://docs.ros.org/en/jazzy/Releases/Release-Jazzy-Jalisco.html)

3. Install necessary dependencies for our ROS2 workspace. You can look through the [Dockerfile](./Dockerfile) for the list of dependancies.

4. Run `colcon build` in `ros_workspace`

5. Source the workspace by running `source /opt/ros/jazzy/setup.bash` and `ros_workspace/install/setup.bash`

6. Run `ros2 launch bluestar_frontend bluestar_frontend.xml`

7. Run `ros2 launch bluestar_backend bluestar_backend.xml`

If you make changes and would like to recompile, restart from step 4.

## VSCode Dev Container (Development)

1. Install the [VSCode Dev Container Extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open `ros_workspace` directly in vscode and use the Dev Container extension to re-open the workspace in a devcontainer based on the configuration in `.devcontainer/devcontainer.json`

This devcontainer is useful for development and testing without installing any dependencies on your local machine.

#### Simulation Environment (TODO)

EER has a [simulation environment](https://github.com/EasternEdgeRobotics/rov-sim) that was integrated with the [2025 Software](https://github.com/EasternEdgeRobotics/Software_2025). 

Porting this simulation environment requires:
- Creating plugins and an associated ROV model in the [simulation environment](https://github.com/EasternEdgeRobotics/rov-sim) to mimic Bluestar.
- Writing a replacement for [pilot_listener.cpp](./src/bluestar_backend/src/pilot_listener.cpp) that does the same thing but publishes to Gazebo simulation topics instead of UART.

#### Autonomy (TODO)

The frontend has been seperated into core and gui components. Core components are meant to be reused for any autonomy ROS node. 