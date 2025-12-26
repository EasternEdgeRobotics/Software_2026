FROM ros:jazzy-ros-base-noble

SHELL ["/bin/bash", "-c"]

RUN apt-get update 
RUN apt-get install -y python3 python3-pip ros-jazzy-rosbridge-server ros-jazzy-web-video-server ffmpeg libsm6 libxext6 dos2unix libglfw3 libglfw3-dev 
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src /app/src

ARG CACHEBUST=1

# TODO: Figure out  the required dependencies for bluestar_frontend and ensure that they are installed
RUN . /opt/ros/$ROS_DISTRO/setup.sh && colcon build --packages-skip bluestar_frontend 
RUN echo 'source /app/install/setup.bash' >> /etc/bash.bashrc