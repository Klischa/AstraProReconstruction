import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, TextSubstitution
from launch_ros.actions import Node


def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    pkg_astra_examples = get_package_share_directory('astra_examples')

    twin_camera_model = LaunchConfiguration('twin_camera_model')
    enable_twin_sync = LaunchConfiguration('enable_twin_sync')
    enable_rviz = LaunchConfiguration('enable_rviz')
    gui = LaunchConfiguration('gui')

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

    declare_gui_arg = DeclareLaunchArgument(
        'gui',
        default_value='true',
        description='Show Gazebo GUI window'
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

    test_camera_publisher_node = Node(
        package='astra_examples',
        executable='test_camera_publisher',
        name='test_camera_publisher',
        output='screen'
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
        declare_gui_arg,
        gazebo_launch,
        test_camera_publisher_node,
        gazebo_twin_node,
        rviz_node
    ])