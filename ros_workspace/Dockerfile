FROM ros:jazzy-ros-base-noble

# Specify ROV name
ARG ROV_NAME=waterwitch

# Specify whether this is being built for sim (1) or real ROV (0)
ARG DEV_MODE=false

ENV ROV_NAME=${ROV_NAME}
ENV DEV_MODE=${DEV_MODE}

RUN if [ "$DEV_MODE" = "false" ]; then \
    # For real ROV (compiles on Raspberry Pi)
    apt-get update && apt-get install -y python3 python3-pip ros-jazzy-rosbridge-server ros-jazzy-web-video-server ffmpeg libsm6 libxext6 dos2unix libglfw3 libglfw3-dev i2c-tools && \
    # TODO: It seems this method of installing Python packages doesn't work on Ubuntu Nobel
    pip3 install --break-system-packages sqlalchemy RPi.GPIO smbus2 adafruit-circuitpython-bno055 adafruit-circuitpython-pca9685 adafruit-circuitpython-servokit opencv-python; \
else \
    # For development (to interface with Simulator)
    apt-get update && apt-get install -y python3 python3-pip ros-jazzy-rosbridge-server ros-jazzy-web-video-server ffmpeg libsm6 libxext6 dos2unix libglfw3 libglfw3-dev && \
    apt install -y python3-sqlalchemy python3-opencv; \
fi

WORKDIR /app
COPY src /app/src
COPY docker_entry_script.sh /app

ARG CACHEBUST=1
RUN . /opt/ros/$ROS_DISTRO/setup.sh && colcon build --packages-skip waterwitch_frontend
RUN echo 'source /app/install/setup.bash $ROV_NAME $DEV_MODE' >> /etc/bash.bashrc
RUN dos2unix /app/docker_entry_script.sh

ENTRYPOINT bash /app/docker_entry_script.sh "$ROV_NAME" "$DEV_MODE"
