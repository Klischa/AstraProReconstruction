import os

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_astra_scan = get_package_share_directory('astra_scan')
    prefix = get_package_prefix('astra_scan')
    exe = os.path.join(prefix, 'bin', 'scan_box_filter')

    input_topic = LaunchConfiguration('input_topic')
    output_topic = LaunchConfiguration('output_topic')

    declare_input = DeclareLaunchArgument(
        'input_topic', default_value='/rtabmap/cloud_map')
    declare_output = DeclareLaunchArgument(
        'output_topic', default_value='/scan/box_points')

    filter_node = Node(
        executable=exe,
        name='scan_box_filter',
        output='screen',
        parameters=[{
            'input_topic': input_topic,
            'output_topic': output_topic,
            'marker_frame': 'map',
            'center_x': 0.0,
            'center_y': 0.0,
            'center_z': 0.7,
            'size_x': 0.8,
            'size_y': 0.8,
            'size_z': 0.8,
        }],
    )

    return LaunchDescription([
        declare_input,
        declare_output,
        filter_node,
    ])