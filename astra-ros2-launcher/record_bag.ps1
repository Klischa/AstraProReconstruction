# Record a rosbag with depth, color and pointcloud (Ctrl+C on this window stops recording)
$name = "astra_$(Get-Date -Format yyyyMMdd_HHmmss)"
Write-Host "Recording to /root/bags/$name (Ctrl+C to stop)"
wsl -d Ubuntu-22.04 -u root -e bash -lc "mkdir -p /root/bags; source /opt/ros/humble/setup.bash; source /root/astra_ws/install/setup.bash; ros2 bag record -o /root/bags/$name /camera/depth/image_raw /camera/color/image_raw /camera/depth/points /camera/depth/camera_info /camera/color/camera_info"
