"""Simulate one Tello drone in gz sim."""

import os

from ament_index_python.packages import get_package_share_directory
from ament_index_python import get_package_prefix
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def _resource_paths():
    tello_gazebo_share = get_package_share_directory("tello_gazebo")
    tello_description_share = os.path.join(get_package_prefix("tello_description"), "share", "tello_description")
    return [
        os.path.join(tello_gazebo_share, "models"),
        os.path.join(tello_description_share, "meshes"),
    ]


def generate_launch_description():
    ns = "drone1"
    tello_gazebo_share = get_package_share_directory("tello_gazebo")
    world_path = os.path.join(tello_gazebo_share, "worlds", "simple.world")
    urdf_path = os.path.join(get_package_share_directory("tello_description"), "urdf", "tello_1.urdf")

    gz_resource_path = os.pathsep.join(_resource_paths())

    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory("ros_gz_sim"), "launch", "gz_sim.launch.py")
        ),
        launch_arguments={"gz_args": f"-r {world_path}"}.items(),
    )

    spawn_tello = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        arguments=["-name", ns, "-file", urdf_path, "-x", "0", "-y", "0", "-z", "1", "-Y", "0"],
    )

    return LaunchDescription(
        [
            SetEnvironmentVariable(name="GZ_SIM_RESOURCE_PATH", value=gz_resource_path),
            gz_sim,
            Node(
                package="ros_gz_bridge",
                executable="parameter_bridge",
                output="screen",
                arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
            ),
            TimerAction(period=2.0, actions=[spawn_tello]),
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                output="screen",
                parameters=[{"use_sim_time": True}],
                arguments=[urdf_path],
            ),
            Node(package="joy", executable="joy_node", output="screen", namespace=ns),
            Node(package="tello_driver", executable="tello_joy_main", output="screen", namespace=ns),
        ]
    )
