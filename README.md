# Software_2025

[Eastern Edge Robotics](https://www.easternedgerobotics.com/) (EER) is a student-led engineering design team based at the Memorial University of Newfoundland and the Fisheries and Marine Institute. The team competes in the annual MATE ROV Competition. Every year at EER, we build a small remotely-operated vehicle (ROV). This repository is EER's 2026 software package.

This code repository contains code for maintaining EER's two current ROVs, Beaumont and Waterwitch.

## Table of Contents

- [Repository Structure](#repository-structure) 
    - [ros_workspace](#ros_workspace) 
    - [web_frontend](#web_frontend) 
- [How To Contribute](#how_to_contribute) 

## Repository Structure

### ros_workspace
This project uses ROS2 Jazzy.

#### ROS2 Packages
| Package Name         | Description                                                                 |
|----------------------|-----------------------------------------------------------------------------|
| **beaumont_backend**  | Backend code for interfacing with the ROV named Beaumont.                   |
| **waterwitch_backend** | Backend code for interfacing with the ROV named Waterwitch.               |
| **common_backend**    | Common backend ROS nodes for all ROVs.                                      |
| **eer_interfaces**    | Custom ROS2 interfaces used by all packages.                                |
| **waterwitch_frotnend**     | A frontend built using C++ with a GUI interface, meant for driving the ROV named Waterwitch.                          |

More details [here](./ros_workspace/)

### web_frontend
This folder can be thought of as another ROS2 package. It is written in ReactJS and communicates with our backends over ROS2 topics by using roslibjs.

More details [here](./web_frontend/)

## How to Contribute

1. Ensure that you're on the team Discord and Google Drive
2. Complete the [Onboarding Task](https://docs.google.com/document/d/13x00C8hjDYVJlFbLWkDBietP3UNgz9KQ2YJGyHPrInM/edit?usp=drive_link) and share your progress 
3. Pick a task from our [task tracker](https://docs.google.com/spreadsheets/d/1OF3RxeuQIAM3jEYy3F_bVlgd5O9Kzjha2wboch3H_Rw/edit?usp=drive_link) or suggest one based on the team's goal


