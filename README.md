## Running a Tello simulation in Gazebo Sim (`gz`)

This repository now targets Gazebo Sim (`gz`, Garden+ style APIs), not Gazebo Classic.

## Installation (ROS 2 Humble)

```bash
sudo apt update
sudo apt install -y \
  ros-humble-desktop \
  ros-humble-ros-gz-sim \
  ros-humble-ros-gz-bridge \
  ros-humble-ros-gz-image \
  ros-humble-cv-bridge \
  ros-humble-camera-calibration-parsers \
  libasio-dev
pip3 install transformations
```

## Build

```bash
mkdir -p ~/tello_ros_ws/src
cd ~/tello_ros_ws/src
git clone https://github.com/TIERS/tello-ros2-gazebo.git
cd ..
source /opt/ros/humble/setup.bash
colcon build --symlink-install
```

## Run

```bash
cd ~/tello_ros_ws
source install/setup.bash
ros2 launch tello_fira_gazebo fira_arena_launch.py
```

Other launch files:
- `ros2 launch tello_fira_gazebo fira_autonomous_race_launch.py`
- `ros2 launch tello_gazebo simple_launch.py`
- `ros2 launch tello_gazebo multiple_drones.py`

## Control

```bash
ros2 service call /drone1/tello_action tello_msgs/srv/TelloAction "{cmd: 'takeoff'}"
ros2 service call /drone1/tello_action tello_msgs/srv/TelloAction "{cmd: 'land'}"
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r __ns:=/drone1
```

## Docker

Open `http://localhost:6080`, then:

```bash
cd /home/ubuntu/ros_ws
source install/setup.bash
ros2 launch tello_fira_gazebo fira_arena_launch.py
```
