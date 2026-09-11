# Stop camera node and pointcloud node (WSL processes)
# SIGKILL (-9) because the node blocks in USB calls and ignores SIGTERM.
# Bracket trick prevents pkill from matching its own shell.
wsl -d Ubuntu-22.04 -u root -e bash -lc "pkill -9 -f 'OBCameraNodeFactor[y]'; pkill -9 -f 'point_cloud_xyz_nod[e]'; sleep 2; pgrep -fa 'OBCameraNodeFactor[y]|point_cloud_xyz_nod[e]' || echo 'camera stopped'"
