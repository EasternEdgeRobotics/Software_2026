FROM ros:jazzy-ros-base-noble

SHELL ["/bin/bash", "-c"]

# Base dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    ros-jazzy-rosbridge-server \
    ros-jazzy-web-video-server \
    ffmpeg \
    libsm6 \
    libxext6 \
    dos2unix \
    # OpenGL/GLFW dependencies for bluestar_frontend
    libglfw3 \
    libglfw3-dev \
    libglew-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    mesa-utils \
    libxkbcommon-dev \
    # OpenCV
    libopencv-dev \
    # X11 and VNC for GUI in container
    xvfb \
    x11vnc \
    xterm \
    openbox \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    && rm -rf /var/lib/apt/lists/*

# VNC configuration
ENV DISPLAY=:99
ENV VNC_PORT=5900
ENV VNC_RESOLUTION=1280x720x24

WORKDIR /app
COPY src /app/src

COPY docker_backend_entry_script.sh /app/docker_backend_entry_script.sh
RUN chmod +x /app/docker_backend_entry_script.sh

COPY docker_frontend_entry_script.sh /app/docker_frontend_entry_script.sh
RUN chmod +x /app/docker_frontend_entry_script.sh

ARG CACHEBUST=1

# Build all ROS packages
RUN . /opt/ros/jazzy/setup.sh && colcon build

RUN echo 'source /opt/ros/jazzy/setup.bash' >> /etc/bash.bashrc 
RUN echo 'source /app/install/setup.bash' >> /etc/bash.bashrc
