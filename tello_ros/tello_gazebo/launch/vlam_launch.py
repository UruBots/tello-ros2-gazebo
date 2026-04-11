"""Simulate Tello drones with fiducial_vlam in gz sim."""

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
    drones = ["drone1", "drone2"]

    tello_gazebo_path = get_package_share_directory("tello_gazebo")
    tello_description_path = get_package_share_directory("tello_description")

    world_path = os.path.join(tello_gazebo_path, "worlds", "fiducial.world")
    map_path = os.path.join(tello_gazebo_path, "worlds", "fiducial_map.yaml")

    actions = [
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
        Node(
            package="fiducial_vlam",
            executable="vmap_main",
            output="screen",
            name="vmap_main",
            parameters=[
                {
                    "publish_tfs": 1,
                    "marker_length": 0.1778,
                    "marker_map_load_full_filename": map_path,
                    "make_not_use_map": 0,
                }
            ],
        ),
        Node(package="joy", executable="joy_node", output="screen", namespace=drones[0]),
        Node(package="tello_driver", executable="tello_joy_main", output="screen", namespace=drones[0]),
    ]

    for idx, namespace in enumerate(drones):
        suffix = f"_{idx + 1}"
        urdf_path = os.path.join(tello_description_path, "urdf", f"tello{suffix}.urdf")
        actions.append(
            TimerAction(
                period=2.0 + idx * 0.2,
                actions=[
                    Node(
                        package="ros_gz_sim",
                        executable="create",
                        output="screen",
                        arguments=["-name", namespace, "-file", urdf_path, "-x", "0", "-y", str(idx), "-z", "1", "-Y", "0"],
                    )
                ],
            )
        )
        actions.append(
            Node(
                package="fiducial_vlam",
                executable="vloc_main",
                output="screen",
                name="vloc_main",
                namespace=namespace,
                parameters=[
                    {
                        "publish_tfs": 1,
                        "base_frame_id": f"base_link{suffix}",
                        "t_camera_base_z": -0.035,
                        "camera_frame_id": f"camera_link{suffix}",
                    }
                ],
            )
        )

    return LaunchDescription(actions)
