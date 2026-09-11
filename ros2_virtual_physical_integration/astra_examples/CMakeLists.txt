cmake_minimum_required(VERSION 3.8)
project(astra_examples)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(pcl_ros REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2 REQUIRED)
find_package(image_geometry REQUIRED)

file(GLOB GZ_TWIN_INCLUDE_DIRS /opt/ros/jazzy/opt/*_vendor/include)
list(APPEND GZ_TWIN_INCLUDE_DIRS
  /opt/ros/jazzy/opt/gz_utils_vendor/include/gz/utils2
  /opt/ros/jazzy/opt/gz_msgs_vendor/include/gz/msgs10
  /opt/ros/jazzy/opt/gz_transport_vendor/include/gz/transport13
)
file(GLOB GZ_TWIN_LIBRARY_DIRS /opt/ros/jazzy/opt/*_vendor/lib)

include_directories(
  include
  ${OpenCV_INCLUDE_DIRS}
)

add_executable(gazebo_twin src/gazebo_twin.cpp)
target_include_directories(gazebo_twin PRIVATE
  include
  ${GZ_TWIN_INCLUDE_DIRS}
)
target_link_directories(gazebo_twin PRIVATE ${GZ_TWIN_LIBRARY_DIRS})
ament_target_dependencies(gazebo_twin
  rclcpp
  sensor_msgs
  geometry_msgs
  cv_bridge
  OpenCV
  pcl_ros
  pcl_conversions
  tf2_ros
  tf2
  image_geometry
)
target_link_libraries(gazebo_twin gz-transport13 gz-msgs10 gz-utils2 protobuf)

add_executable(test_camera_publisher src/test_camera_publisher.cpp)
ament_target_dependencies(test_camera_publisher
  rclcpp
  sensor_msgs
  pcl_ros
  pcl_conversions
)

install(TARGETS
  gazebo_twin
  test_camera_publisher
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()