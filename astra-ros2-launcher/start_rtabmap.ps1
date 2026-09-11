# Start RTAB-Map RGB-D SLAM (camera must be running: start_camera.ps1)
# Launches: depth converter, camera_info repair, rgbd_odometry, rtabmap SLAM, rtabmapviz GUI
wsl -d Ubuntu-22.04 -u root -e bash /root/launch_rtabmap3.sh
