from launch import LaunchDescription
from launch_ros.actions import Node


# Launch an emulator for testing


def generate_launch_description():
    tello_driver_params = [{'drone_ip': '127.0.0.1'}]

    return LaunchDescription([
        Node(package='tello_driver', executable='tello_emulator', output='screen'),
        Node(package='tello_driver', executable='tello_driver_main', name='tello_driver',
             parameters=tello_driver_params, output='screen'),
    ])
