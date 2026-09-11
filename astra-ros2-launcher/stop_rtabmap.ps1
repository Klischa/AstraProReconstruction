# Stop RTAB-Map stack (odometry + SLAM + viz + converter/repair nodes)
wsl -d Ubuntu-22.04 -u root -e bash -lc "pkill -9 -f 'rgbd_odometr[y]'; pkill -9 -f 'rtabmap_sla[m]'; pkill -9 -f 'rtabmap_vi[z]'; pkill -9 -f 'depth_convert'; pkill -9 -f 'camera_info_repai[r]'; sleep 2; echo 'rtabmap stopped'"
