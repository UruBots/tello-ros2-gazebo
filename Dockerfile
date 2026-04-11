FROM sbarcelona11/ros2-desktop-vnc:humble

WORKDIR /home/ubuntu/ros_ws
ENV QT_X11_NO_MITSHM=1

# Refresh ROS 2 apt key (original key expired) and ensure repository entry uses it
# See: https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debians.html
RUN mkdir -p /etc/apt/keyrings \
    && curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /etc/apt/keyrings/ros-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
       | tee /etc/apt/sources.list.d/ros2.list > /dev/null

# Gazebo / gz apt repository (required for libgz-*-dev headers used by custom plugins)
RUN mkdir -p /usr/share/keyrings \
    && curl -fsSL https://packages.osrfoundation.org/gazebo.gpg \
       | gpg --dearmor -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] https://packages.osrfoundation.org/gazebo/ubuntu-stable $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
       | tee /etc/apt/sources.list.d/gazebo-stable.list > /dev/null

# Install dependencies
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-camera-calibration-parsers \
    ros-humble-ros-gz-sim \
    ros-humble-ros-gz-bridge \
    ros-humble-ros-gz-image \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Gazebo Sim development libraries needed to compile custom gz plugins.
# Resolve package versions dynamically from apt to avoid hardcoding.
RUN apt-get update && \
    SIM_DEV_PKG="$(apt-cache search '^libgz-sim[0-9]+-dev$' | awk '{print $1}' | sort -V | tail -n1)" && \
    MATH_DEV_PKG="$(apt-cache search '^libgz-math[0-9]+-dev$' | awk '{print $1}' | sort -V | tail -n1)" && \
    PLUGIN_DEV_PKG="$(apt-cache search '^libgz-plugin[0-9]+-dev$' | awk '{print $1}' | sort -V | tail -n1)" && \
    test -n "${SIM_DEV_PKG}" && test -n "${MATH_DEV_PKG}" && test -n "${PLUGIN_DEV_PKG}" && \
    apt-get install -y "${SIM_DEV_PKG}" "${MATH_DEV_PKG}" "${PLUGIN_DEV_PKG}" && \
    rm -rf /var/lib/apt/lists/*

# Install PYTHON dependencies
RUN pip3 install transformations
# Install YoloV8
RUN pip3 install opencv-python torch torchvision torchaudio ultralytics
# Create workspace and Colcon build
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  mkdir -p /home/ubuntu/ros_ws/src && cd /home/ubuntu/ros_ws/ && \
                  colcon build --symlink-install"

# Set up the workspace
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc && \
                  echo 'source /home/ubuntu/ros_ws/install/setup.bash' >> ~/.bashrc"

# Install interbotix_ros_core
RUN /bin/bash -c "cd /home/ubuntu/ros_ws/src && \
                  git clone https://github.com/Los-UruBots-del-Norte/tello-ros2-gazebo.git && \
                  cd tello-ros2-gazebo"
# Build
RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash && \
                  cd /home/ubuntu/ros_ws/ && \
                  rosdep update --rosdistro=$ROS_DISTRO && \
                  sudo apt-get update && \
                  rosdep install --from-paths src --ignore-src -r -y && \
                  colcon build --parallel-workers 1 --symlink-install"

RUN /bin/bash -c "cd /home/ubuntu/ros_ws && \
                    source install/setup.bash && \
                    export GZ_SIM_RESOURCE_PATH=${PWD}/install/tello_gazebo/share/tello_gazebo/models:${PWD}/install/tello_fira_gazebo/share/tello_fira_gazebo/models && \
                    source ~/.bashrc"
                    # && \ros2 launch tello_fira_gazebo fira_arena_launch.py"

# Ensure VS Code Remote can write to the workspace home directory
RUN set -eux; \
    owner="root:root"; \
    if id -u ubuntu >/dev/null 2>&1; then owner="ubuntu:ubuntu"; fi; \
    mkdir -p /home/ubuntu/.vscode-server /home/ubuntu/.vscode-remote; \
    chown -R "${owner}" /home/ubuntu
