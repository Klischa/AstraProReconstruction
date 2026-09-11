# Open RViz2 with the repo's pointcloud config (needs WSLg - window appears on Windows)
wsl -d Ubuntu-22.04 -u root -e bash -lc "source /opt/ros/humble/setup.bash; source /root/astra_ws/install/setup.bash; rviz2 -d /root/astra_ws/astrapro_pointcloud.rviz"
