#!/bin/bash

# Frontend entry script with VNC support
# Connect to VNC at localhost:5900

set -e

# Source ROS setup
source /opt/ros/jazzy/setup.sh

# Make the XDG runtime directory
mkdir -p /tmp/xdg

if [ ! -e /app/install/setup.sh ]; then
    cd /app
    colcon build
fi

source /app/install/setup.bash

# Start Xvfb (virtual framebuffer)
echo "[Frontend] Starting Xvfb on display :99..."
Xvfb :99 -screen 0 ${VNC_RESOLUTION:-1280x720x24} &
XVFB_PID=$!
sleep 2

# Start a simple window manager
echo "[Frontend] Starting openbox window manager..."
DISPLAY=:99 openbox &
sleep 1

# Start x11vnc server (no password by default, use -passwd for security)
echo "[Frontend] Starting VNC server on port ${VNC_PORT:-5900}..."
x11vnc -display :99 -forever -shared -rfbport ${VNC_PORT:-5900} &
X11VNC_PID=$!
sleep 1

echo "[Frontend] VNC server ready. Connect to localhost:${VNC_PORT:-5900}"
echo "[Frontend] Starting bluestar_frontend..."

# Run the frontend
DISPLAY=:99 ros2 launch bluestar_frontend bluestar_frontend.xml

# Cleanup on exit
kill $X11VNC_PID 2>/dev/null || true
kill $XVFB_PID 2>/dev/null || true
