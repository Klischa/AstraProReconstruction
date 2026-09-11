# Hard reset: kill camera, restart WSL, re-attach USB, launch fresh.
# Use when camera hangs in "wait for device connect..." (wedged device state).
$ErrorActionPreference = 'SilentlyContinue'
$u = "C:\Program Files\usbipd-win\usbipd.exe"

Write-Host "[1/4] Killing camera processes..."
wsl -d Ubuntu-22.04 -u root -e bash -lc "pkill -9 -f 'OBCameraNodeFactor[y]'; pkill -9 -f 'point_cloud_xyz_nod[e]'; echo done"

Write-Host "[2/4] Restarting WSL..."
wsl --shutdown
Start-Sleep -Seconds 10

Write-Host "[3/4] Starting Ubuntu-22.04 + attaching USB..."
Start-Process wsl.exe -ArgumentList '-d','Ubuntu-22.04','-u','root','-e','sleep','infinity' -WindowStyle Hidden
Start-Sleep -Seconds 8
$busids = (& $u list) | Select-String '2bc5' | ForEach-Object { ($_.ToString().Trim() -split '\s+')[0] }
foreach ($b in $busids) { & $u attach --wsl --busid $b | Out-Null; Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 5
wsl -d Ubuntu-22.04 -u root -e bash -lc "lsusb | grep -i 2bc5"

Write-Host "[4/4] Launching camera..."
wsl -d Ubuntu-22.04 -u root -e bash /mnt/c/Users/DIMON/AppData/Local/Temp/opencode/launch_astra.sh
Write-Host ""
Write-Host "DONE. Check streams: check_camera.ps1"
