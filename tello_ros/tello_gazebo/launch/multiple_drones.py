"""Simulate multiple Tello drones in gz sim."""

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
    drones = ["drone1", "drone2", "drone3", "drone4"]
    starts = [(0, 0), (0, 10), (10, 10), (10, 0)]

    tello_gazebo_share = get_package_share_directory("tello_gazebo")
    world_path = os.path.join(tello_gazebo_share, "worlds", "fiducial.world")
    tello_description_path = get_package_share_directory("tello_description")

    gz_resource_path = os.pathsep.join(_resource_paths())

    actions = [
        SetEnvironmentVariable(name="GZ_SIM_RESOURCE_PATH", value=gz_resource_path),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(get_package_share_directory("ros_gz_sim"), "launch", "gz_sim.launch.py")
            ),
            launch_arguments={"gz_args": f"-r {world_path}"}.items(),
        ),
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            output="screen",
            arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
        ),
    ]

    for idx, namespace in enumerate(drones):
        suffix = f"_{idx + 1}"
        urdf_path = os.path.join(tello_description_path, "urdf", f"tello{suffix}.urdf")
        x, y = starts[idx]

        actions.append(
            TimerAction(
                period=2.0 + idx * 0.2,
                actions=[
                    Node(
                        package="ros_gz_sim",
                        executable="create",
                        output="screen",
                        arguments=[
                            "-name",
                            namespace,
                            "-file",
                            urdf_path,
                            "-x",
                            str(x),
                            "-y",
                            str(y),
                            "-z",
                            "1",
                            "-Y",
                            "0",
                        ],
                    )
                ],
            )
        )

    return LaunchDescription(actions)
