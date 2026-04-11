## Running a Tello simulation in Gazebo Sim (`gz`)

`tello_gazebo` contains:
- `TelloSystem` (Gazebo Sim system plugin) with takeoff / land / simple dynamics.
- Marker models and worlds.
- Launch files based on `ros_gz_sim` and `ros_gz_bridge`.

## Run a teleop simulation

```bash
cd ~/tello_ros_ws
source install/setup.bash
ros2 launch tello_gazebo simple_launch.py
```

## Run FIRA worlds

```bash
source ~/tello_ros_ws/install/setup.bash
ros2 launch tello_fira_gazebo fira_arena_launch.py
ros2 launch tello_fira_gazebo fira_autonomous_race_launch.py
```

## Integrate with `fiducial_vlam`

```bash
cd ~/tello_ros_ws/src
git clone https://github.com/ptrmu/fiducial_vlam.git
cd ..
colcon build --event-handlers console_direct+
source install/local_setup.bash
ros2 launch tello_gazebo vlam_launch.py
```
