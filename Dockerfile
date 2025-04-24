FROM osrf/ros:humble-desktop-full

WORKDIR /home/ubuntu/ros_ws
# ENV QT_X11_NO_MITSHM=1

# Install dependencies
RUN apt-get update && apt-get install -y \
    python3-pip \
    ros-humble-cv-bridge \
    ros-humble-camera-calibration-parsers \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# Install PYTHON dependencies
# RUN pip3 install transformations torch torchvision torchaudio ultralytics
# Install YoloV8
RUN pip3 install opencv-python
# Create workspace and Colcon build
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  mkdir -p /home/ubuntu/ros_ws/src && cd /home/ubuntu/ros_ws/ && \
                  colcon build --symlink-install"

# Set up the workspace
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  echo 'source /usr/share/gazebo/setup.bash' >> ~/.bashrc && \
                  echo 'source /home/ubuntu/ros_ws/install/setup.bash' >> ~/.bashrc"

# Install interbotix_ros_core
RUN /bin/bash -c "cd /home/ubuntu/ros_ws/src && \
                  git clone https://github.com/Urubots/tello-ros2-gazebo.git && \
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
                    export GAZEBO_MODEL_PATH=${PWD}/install/tello_gazebo/share/tello_gazebo/models && \
                    source ~/.bashrc"
                    # && \ros2 launch tello_gazebo simple_launch.py"