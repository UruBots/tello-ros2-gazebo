"""Launch Tello in the FIRA arena world using gz sim."""

import os

from ament_index_python.packages import get_package_share_directory
from ament_index_python import get_package_prefix
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def _resource_paths():
    pkg_share = get_package_share_directory("tello_fira_gazebo")
    desc_share = os.path.join(get_package_prefix("tello_description"), "share", "tello_description")
    return [
        os.path.join(pkg_share, "models"),
        os.path.join(desc_share, "meshes"),
    ]


def generate_launch_description():
    ns = "drone1"
    pkg_share = get_package_share_directory("tello_fira_gazebo")
    world_path = os.path.join(pkg_share, "worlds", "fira_arena.world")
    urdf_path = os.path.join(get_package_share_directory("tello_description"), "urdf", "tello_1.urdf")

    return LaunchDescription(
        [
            SetEnvironmentVariable(name="GZ_SIM_RESOURCE_PATH", value=os.pathsep.join(_resource_paths())),
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
            TimerAction(
                period=2.0,
                actions=[
                    Node(
                        package="ros_gz_sim",
                        executable="create",
                        output="screen",
                        arguments=["-name", ns, "-file", urdf_path, "-x", "0", "-y", "0", "-z", "1", "-Y", "0"],
                    )
                ],
            ),
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
