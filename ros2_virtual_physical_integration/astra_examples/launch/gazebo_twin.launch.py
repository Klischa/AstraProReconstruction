import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, TextSubstitution
from launch_ros.actions import Node


def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    pkg_astra_examples = get_package_share_directory('astra_examples')

    twin_camera_model = LaunchConfiguration('twin_camera_model')
    enable_twin_sync = LaunchConfiguration('enable_twin_sync')
    enable_rviz = LaunchConfiguration('enable_rviz')
    use_real_camera = LaunchConfiguration('use_real_camera')
    gui = LaunchConfiguration('gui')
    use_uvc_camera = LaunchConfiguration('use_uvc_camera')
    uvc_camera_format = LaunchConfiguration('uvc_camera_format')
    depth_registration = LaunchConfiguration('depth_registration')
    enable_point_cloud = LaunchConfiguration('enable_point_cloud')
    enable_colored_point_cloud = LaunchConfiguration('enable_colored_point_cloud')

    declare_twin_camera_model_arg = DeclareLaunchArgument(
        'twin_camera_model',
        default_value='twin_camera',
        description='Twin camera model name in Gazebo'
    )

    declare_enable_twin_sync_arg = DeclareLaunchArgument(
        'enable_twin_sync',
        default_value='true',
        description='Enable twin pose synchronization'
    )

    declare_enable_rviz_arg = DeclareLaunchArgument(
        'enable_rviz',
        default_value='false',
        description='Enable RViz visualization (requires GPU resources)'
    )

    declare_use_real_camera_arg = DeclareLaunchArgument(
        'use_real_camera',
        default_value='false',
        description='Use real Astra camera or test publisher'
    )

    declare_gui_arg = DeclareLaunchArgument(
        'gui',
        default_value='true',
        description='Show Gazebo GUI window'
    )

    declare_use_uvc_camera_arg = DeclareLaunchArgument(
        'use_uvc_camera',
        default_value='false',
        description='Grab color frames over UVC (libuvc) instead of OpenNI2'
    )

    declare_uvc_camera_format_arg = DeclareLaunchArgument(
        'uvc_camera_format',
        default_value='mjpeg',
        description='UVC color format: mjpeg | yuyv | uncompressed | ...'
    )

    declare_depth_registration_arg = DeclareLaunchArgument(
        'depth_registration',
        default_value='true',
        description='Align depth to color frame'
    )

    declare_enable_point_cloud_arg = DeclareLaunchArgument(
        'enable_point_cloud',
        default_value='true',
        description='Publish point cloud'
    )

    declare_enable_colored_point_cloud_arg = DeclareLaunchArgument(
        'enable_colored_point_cloud',
        default_value='true',
        description='Publish colored point cloud'
    )

    gazebo_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
        ),
        launch_arguments={
            'gz_args': [TextSubstitution(text='-r '), os.path.join(pkg_astra_examples, 'launch', 'empty.world')],
            'gui': gui
        }.items()
    )

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
        ],
        condition=IfCondition(use_real_camera)
    )

    test_camera_publisher_node = Node(
        package='astra_examples',
        executable='test_camera_publisher',
        name='test_camera_publisher',
        output='screen',
        condition=UnlessCondition(use_real_camera)
    )

    gazebo_twin_node = Node(
        package='astra_examples',
        executable='gazebo_twin',
        name='gazebo_twin',
        output='screen',
        parameters=[{
            'twin_camera_model': twin_camera_model,
            'twin_camera_x': 0.0,
            'twin_camera_y': 0.0,
            'twin_camera_z': 1.0,
            'twin_camera_qx': 0.0,
            'twin_camera_qy': 0.0,
            'twin_camera_qz': 0.0,
            'twin_camera_qw': 1.0,
            'enable_twin_sync': enable_twin_sync
        }]
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', os.path.join(pkg_astra_examples, 'launch', 'twin_view.rviz')],
        condition=IfCondition(enable_rviz)
    )

    return LaunchDescription([
        declare_twin_camera_model_arg,
        declare_enable_twin_sync_arg,
        declare_enable_rviz_arg,
        declare_use_real_camera_arg,
        declare_gui_arg,
        declare_use_uvc_camera_arg,
        declare_uvc_camera_format_arg,
        declare_depth_registration_arg,
        declare_enable_point_cloud_arg,
        declare_enable_colored_point_cloud_arg,
        gazebo_launch,
        astra_camera_node,
        test_camera_publisher_node,
        gazebo_twin_node,
        rviz_node
    ])