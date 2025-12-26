# Copilot Instructions for EER Software_2026

## Project Overview
ROV (Remotely Operated Vehicle) control software for Eastern Edge Robotics' **Bluestar** ROV. Two-layer architecture: ROS2 packages (topsides) communicate with RP2040 firmware (onboard) via RS-485 UART.

## Architecture

### Data Flow
```
Frontend (ImGui GUI) → ROS2 Topics → Backend (pilot_listener) → UART/RS-485 → Firmware (RP2040)
```

### Key Components
| Layer | Location | Purpose |
|-------|----------|---------|
| **Frontend** | `ros_workspace/src/bluestar_frontend/` | ImGui-based C++ GUI for pilot control (requires display, run natively) |
| **Backend** | `ros_workspace/src/bluestar_backend/` | Translates ROS2 messages to UART commands |
| **Interfaces** | `ros_workspace/src/eer_interfaces/` | Custom ROS2 msg/srv definitions |
| **Config Manager** | `ros_workspace/src/config_manager/` | JSON config persistence via ROS2 services |
| **Firmware** | `firmware/LapisLazuli/` | RP2040 code controlling thrusters/servos/LEDs |

> **Note**: This ROV is built for the 2026 International MATE ROV Competition. Core controls are the current focus; autonomy and image recognition features will follow.

### Communication Protocol
- **minihdlc**: HDLC framing library used on both sides for reliable serial communication
- Command IDs defined in [pilot_listener.cpp](ros_workspace/src/bluestar_backend/src/pilot_listener.cpp#L29-L38) match [main.c](firmware/LapisLazuli/main.c#L62-L87) frame handler
- Commands: `0x00-0x05` (thrusters), `0x06-0x07` (LEDs), `0x08-0x0B` (servos), `0x0C-0x13` (DC motors)

## Build & Development

### ROS2 Workspace (Ubuntu 24.04 + ROS2 Jazzy)
```bash
cd ros_workspace
colcon build                                    # Build all packages
colcon build --packages-select bluestar_backend # Build specific package
source install/setup.bash                       # Source after build
ros2 launch bluestar_backend bluestar_startup.xml  # Launch backend
```

### Docker (production deployment)
```bash
docker compose up         # Run in container (for Raspberry Pi deployment)
docker compose up --build # Rebuild after changes
```
> **Frontend note**: The Docker container runs backend only. Frontend requires a graphical environment and runs natively on the pilot's laptop (dependencies not yet containerized—may require VNC).

### Firmware (Pico SDK)
```bash
cd firmware/LapisLazuli/build
cmake .. && make          # Generates LapisLazuli.uf2
# Flash: copy .uf2 to Pico in BOOTSEL mode
```

## Code Conventions

### ROS2 Patterns
- **Custom interfaces** in `eer_interfaces/msg/` and `eer_interfaces/srv/` - add new messages there
- **Service clients** wait for availability: `client->wait_for_service()` before calls
- **Nodes** use `rclcpp::Node` base class; multiple nodes run via `StaticSingleThreadedExecutor`

### Thruster Control
- 6 thrusters with configurable mapping via `THRUSTER_CONFIG_MATRIX[6][6]` in pilot_listener
- Thrust values: 0-255 maps to 1100-1900μs PWM pulse width
- Configuration stored in `configs/bluestar_config.json`

### Adding New Peripherals
1. Define command ID in firmware `main.c` frame handler switch case
2. Add matching command ID constant in `pilot_listener.cpp`
3. Add fields to `PilotInput.msg` if UI-controlled
4. Update frontend `main.cpp` to expose controls

### JSON Configuration
- Configs stored in `configs/` directory as `.json` files
- Use `get_config` service to retrieve, `save_config` topic to persist
- `nlohmann/json` library for parsing (already included)

## Key Files Reference
- [pilot_listener.cpp](ros_workspace/src/bluestar_backend/src/pilot_listener.cpp) - Backend UART communication
- [main.c](firmware/LapisLazuli/main.c) - Firmware command handler and PWM control
- [PilotInput.msg](ros_workspace/src/eer_interfaces/msg/PilotInput.msg) - Control message definition
- [ConfigManager.cpp](ros_workspace/src/config_manager/src/ConfigManager.cpp) - Config service implementation
