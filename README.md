# Software_2026

[Eastern Edge Robotics](https://www.easternedgerobotics.com/) (EER) is a student-led engineering design team based at the Memorial University of Newfoundland and the Fisheries and Marine Institute. The team competes in the annual MATE ROV Competition. Every year at EER, we build a small remotely-operated vehicle (ROV). This repository is EER's 2026 software package.

This code repository contains code for maintaining EER's current ROV, BlueStar.

## Table of Contents

- [Repository Structure](#repository-structure) 
    - [ros_workspace](#ros_workspace) 
    - [Tools](#Tools) 
    - [Assets](#Assets) 

## Repository Structure

### ros_workspace
This project uses ROS2 Jazzy.

#### ROS2 Packages
| Package Name                                                            | Description                                                                                |
|-------------------------------------------------------------------------|--------------------------------------------------------------------------------------------|
| [**bluestar_backend**](./ros_workspace/src/bluestar_backend/)           | Backend code for interfacing with the ROV named BlueStar.                                  |
| [**bluestar_frotnend**](./ros_workspace/src/bluestar_frontend/)         | A C++ GUI for BlueStar made with ImGui, meant for driving the ROV named BlueStar.          |
| [**bluestar_officer_frotnend**](./ros_workspace/src/bluestar_frontend/) | A C++ GUI for BlueStar made with ImGui, meant for the science officer.                     |
| [**common_backend**](./ros_workspace/src/common_backend/)               | Common backend ROS nodes for all ROVs.                                                     |
| [**eer_interfaces**](./ros_workspace/src/eer_interfaces/)               | Custom ROS2 interfaces used by all packages.                                               |

More details [here](./ros_workspace/)

### Tools
Assorted tools useful for BlueStar, or developing for BlueStar.

### Assets
Assets used in BlueStar.

### Firmware
The firmware running on BlueStar's onboard RP2040 microcontroller, which communicates with the onboard Raspberry Pi 4B via I2c, along with the PCB schematics can be found in the [Electrical_2026 Repository](https://github.com/EasternEdgeRobotics/Electrical_2026).

### Agentic AI Tools
Agentic AI tools, such as the Github Copilot Extension, should automatically use the [copilot-instructions.md](./.github/copilot-instructions.md) file for context. This file contains useful technical information for onboarding in natural language.

