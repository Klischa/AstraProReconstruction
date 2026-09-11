# One-click: start Astra Pro camera in ROS2 (Ubuntu-22.04 WSL)
# Launch camera node + pointcloud, then show topics and rates.
$ErrorActionPreference = 'SilentlyContinue'
$u = "C:\Program Files\usbipd-win\usbipd.exe"

Write-Host "[1/4] Starting Ubuntu-22.04 WSL (keeper)..."
$keeper = Get-CimInstance Win32_Process -Filter "Name='wsl.exe'" |
    Where-Object { $_.CommandLine -match 'Ubuntu-22\.04.*sleep' }
if (-not $keeper) {
    Start-Process wsl.exe -ArgumentList '-d','Ubuntu-22.04','-u','root','-e','sleep','infinity' -WindowStyle Hidden
    Start-Sleep -Seconds 6
} else {
    Write-Host "      already running"
}

Write-Host "[2/4] Attaching USB devices (depth 0403 + color 0501)..."
$busids = (& $u list) | Select-String '2bc5' | ForEach-Object { ($_.ToString().Trim() -split '\s+')[0] }
foreach ($b in $busids) { & $u attach --wsl --busid $b | Out-Null }
Start-Sleep -Seconds 4
wsl -d Ubuntu-22.04 -u root -e bash -lc "lsusb | grep -i 2bc5" 

Write-Host "[3/4] Launching camera + pointcloud (background)..."
wsl -d Ubuntu-22.04 -u root -e bash /mnt/c/Users/DIMON/AppData/Local/Temp/opencode/launch_astra.sh

Write-Host "[4/4] Verifying rates..."
wsl -d Ubuntu-22.04 -u root -e bash /mnt/c/Users/DIMON/AppData/Local/Temp/opencode/verify_topics.sh
Write-Host ""
Write-Host "DONE. Camera is streaming. Use check_camera.ps1 anytime."
