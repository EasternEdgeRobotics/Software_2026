FROM ros:jazzy-ros-base-noble

# Specify ROV name
ARG ROV_NAME=bluestar

# Specify whether this is being built for sim (1) or real ROV (0)
ARG DEV_MODE=false

ENV ROV_NAME=${ROV_NAME}
ENV DEV_MODE=${DEV_MODE}

RUN if [ "$DEV_MODE" = "false" ]; then \
    # For real ROV (compiles on Raspberry Pi)
    apt-get update && apt-get install -y python3 python3-pip ros-jazzy-rosbridge-server ros-jazzy-web-video-server ffmpeg libsm6 libxext6 dos2unix libglfw3 libglfw3-dev i2c-tools python3-sqlalchemy python3-opencv python3-smbus2 python3-rpi.gpio gstreamer1.0-plugins-bad gstreamer1.0-nice; \
else \
    # For development (to interface with Simulator)
    apt-get update && apt-get install -y python3 python3-pip ros-jazzy-rosbridge-server ros-jazzy-web-video-server ffmpeg libsm6 libxext6 dos2unix libglfw3 libglfw3-dev python3-sqlalchemy python3-opencv gstreamer1.0-plugins-bad gstreamer1.0-nice; \
fi

WORKDIR /app
COPY src /app/src
COPY docker_entry_script.sh /app

ARG CACHEBUST=1
RUN . /opt/ros/$ROS_DISTRO/setup.sh && colcon build --packages-skip bluestar_frontend bluestar_officer_frontend
RUN echo 'source /app/install/setup.bash $ROV_NAME $DEV_MODE' >> /etc/bash.bashrc
RUN dos2unix /app/docker_entry_script.sh

ENTRYPOINT bash /app/docker_entry_script.sh "$ROV_NAME" "$DEV_MODE"
