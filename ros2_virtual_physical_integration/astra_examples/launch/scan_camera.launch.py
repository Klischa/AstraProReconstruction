import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_astra_examples = get_package_share_directory('astra_examples')

    use_uvc_camera = LaunchConfiguration('use_uvc_camera')
    uvc_camera_format = LaunchConfiguration('uvc_camera_format')
    depth_registration = LaunchConfiguration('depth_registration')
    enable_point_cloud = LaunchConfiguration('enable_point_cloud')
    enable_colored_point_cloud = LaunchConfiguration('enable_colored_point_cloud')

    declare_use_uvc_camera_arg = DeclareLaunchArgument(
        'use_uvc_camera', default_value='true',
        description='Grab color frames over UVC (libuvc) instead of OpenNI2')
    declare_uvc_camera_format_arg = DeclareLaunchArgument(
        'uvc_camera_format', default_value='mjpeg',
        description='UVC color format: mjpeg | yuyv | uncompressed | ...')
    declare_depth_registration_arg = DeclareLaunchArgument(
        'depth_registration', default_value='true',
        description='Align depth to color frame')
    declare_enable_point_cloud_arg = DeclareLaunchArgument(
        'enable_point_cloud', default_value='true',
        description='Publish point cloud')
    declare_enable_colored_point_cloud_arg = DeclareLaunchArgument(
        'enable_colored_point_cloud', default_value='true',
        description='Publish colored point cloud')

    astra_camera_node = Node(
        package='astra_camera',
        executable='astra_camera_node',
        name='camera',
        namespace='camera',
        output='screen',
        parameters=[{
            'camera_name': 'camera',
            'depth_registration': depth_registration,
            'enable_point_cloud': enable_point_cloud,
            'enable_colored_point_cloud': enable_colored_point_cloud,
            'enable_color': True,
            'enable_depth': True,
            'enable_ir': True,
            'use_uvc_camera': use_uvc_camera,
            'uvc_camera_format': uvc_camera_format,
            'publish_tf': True,
            'tf_publish_rate': 10.0
        }],
        remappings=[
            ('/camera/depth/color/points', '/camera/depth_registered/points'),
        ]
    )

    return LaunchDescription([
        declare_use_uvc_camera_arg,
        declare_uvc_camera_format_arg,
        declare_depth_registration_arg,
        declare_enable_point_cloud_arg,
        declare_enable_colored_point_cloud_arg,
        astra_camera_node,
    ])