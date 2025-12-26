# Software_2025

[Eastern Edge Robotics](https://www.easternedgerobotics.com/) (EER) is a student-led engineering design team based at the Memorial University of Newfoundland and the Fisheries and Marine Institute. The team competes in the annual MATE ROV Competition. Every year at EER, we build a small remotely-operated vehicle (ROV). This repository is EER's 2026 software package.

This code repository contains code for EER's current ROV, Bluestar.

## Table of Contents

- [Repository Structure](#repository-structure) 
    - [ros_workspace](#ros_workspace) 
    - [firmware](#firmware) 
- [How To Contribute](#how_to_contribute) 

## Repository Structure

### ros_workspace
This project uses ROS2 Jazzy.

#### ROS2 Packages
| Package Name         | Description                                                                 |
|----------------------|-----------------------------------------------------------------------------|
| **bluestar_backend**  | Backend code for interfacing with Bluestar (both physical ROV and simulation)              |
| **config_manager**    | Stores user and ROV configuration/preferences in JSON format                                      |
| **eer_interfaces**    | Custom ROS2 interfaces used by EER.                                |
| **waterwitch_frotnend**     | A C++ GUI for Bluestar made with ImGui                       |

More details [here](./ros_workspace/)

### firmware
This folder contains the firmware that runs onboard Bluestar on an RP2040 microcontroller, which communicates with topsides via RS-485.

More details [here](./firmware/).

PCB schematics can be found in the [Electrical_2026 Repository](https://github.com/EasternEdgeRobotics/Electrical_2026).


