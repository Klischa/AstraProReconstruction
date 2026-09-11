# Объектное 3D-сканирование: камера зафиксирована, объект вращаешь вручную/на поворотном столе
# (или медленно обходишь объект камерой). По Enter — снимок в PLY в C:\AstraProReconstruction\scans\
# Требует запущенного стеке: start_camera.ps1 + start_rtabmap.ps1 (работающий сам — не перезапускает).

param(
    [int]$Timeout = 6,          # секунд ожидания «устаканивания» облака после сигнала
    [switch]$Relaunch           # принудительно перезапустить SLAM-стек перед сессией
)

$ErrorActionPreference = 'Continue'
$Launcher = Split-Path -Parent $MyInvocation.MyCommand.Path
$ScansDir = Join-Path (Split-Path -Parent $Launcher) 'scans'
New-Item -ItemType Directory -Path $ScansDir -Force | Out-Null

function Invoke-Wsl([string]$Cmd) {
    (wsl -d Ubuntu-22.04 -u root -e bash -lc $Cmd) 2>&1 | ForEach-Object { $_.ToString() }
}

Write-Host "== Скан объекта (Astra Pro + RTAB-Map) ==" -ForegroundColor Cyan

# 1. Камера жива?
$cam = Invoke-Wsl "pgrep -f 'astra_camera' >/dev/null && echo 1 || echo 0"
if ($cam -notmatch '1') {
    Write-Host "Камера не запущена -> запускаю start_camera..." -ForegroundColor Yellow
    & (Join-Path $Launcher 'start_camera.ps1')
}

# 2. SLAM-стек жив?
$slam = Invoke-Wsl "pgrep -f 'rtabmap_sla[m]' >/dev/null && echo 1 || echo 0"
if ($slam -notmatch '1' -or $Relaunch) {
    Write-Host "SLAM не запущен -> запускаю start_rtabmap (окно карты появится)..." -ForegroundColor Yellow
    & (Join-Path $Launcher 'start_rtabmap.ps1')
} else {
    # SLAM уже работает - проверяем и поднимаем визуал (rtabmap_viz), если его нет
    $viz = Invoke-Wsl "pgrep -f '/lib/rtabmap_viz/' >/dev/null && echo 1 || echo 0"
    if ($viz -notmatch '1') {
        Write-Host "Поднимаю окно карты (rtabmap_viz)..." -ForegroundColor Yellow
        Invoke-Wsl "source /opt/ros/humble/setup.bash && source /root/astra_ws/install/setup.bash && nohup ros2 run rtabmap_viz rtabmap_viz > /tmp/rtabmap_viz.log 2>&1 &" | Out-Null
        Start-Sleep -Seconds 12
    }
}

# 3. Копируем свежий save_cloud.py в WSL
$srcPy = Join-Path $Launcher 'save_cloud.py'
if (-not (Test-Path $srcPy)) { Write-Host "Нет save_cloud.py рядом со скриптом" -ForegroundColor Red; exit 1 }
Invoke-Wsl "mkdir -p /root/scans && cp /mnt/c/AstraProReconstruction/astra-ros2-launcher/save_cloud.py /root/save_cloud.py" | Out-Null

Write-Host ""
Write-Host "Как снимать:" -ForegroundColor Green
Write-Host "  - Объект ставь на поворотную платформу/на месте, камеру закрепи (штатив)."
Write-Host "  - Медленно поворачивай объект, пока он не проскользнёт 360 градусов перед камерой."
Write-Host "  - Делай паузы ~1 сек каждые 10-20 градусов (нужно перекрытие кадров)."
Write-Host "  - После полного оборота жми Enter - сохранится цветное облако в метрах (PLY)."
Write-Host ""

while ($true) {
    $null = Read-Host "Повернул весь круг? Жми Enter для снимка (Ctrl+C - выход)"
    $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
    $wslOut = "$ts.ply"
    Write-Host "Снимаю облако ($Timeout сек)... " -ForegroundColor Cyan -NoNewline
    $res = Invoke-Wsl "source /opt/ros/humble/setup.bash && python3 /root/save_cloud.py --out /root/scans/$wslOut --timeout $Timeout"
    $m = ($res -join "`n") -match 'saved (\d+) points'
    if ($m) {
        $n = $Matches[1]
        $dest = Join-Path $ScansDir $wslOut
        Invoke-Wsl "cp /root/scans/$wslOut /mnt/c/AstraProReconstruction/scans/$wslOut" | Out-Null
        $size = [math]::Round((Get-Item $dest).Length / 1MB, 2)
        Write-Host "OK: $n точек, $size МБ -> $dest" -ForegroundColor Green
    } else {
        Write-Host "Не удалось снять облако (см. вывод выше)." -ForegroundColor Red
        $res | Select-Object -Last 5
    }
    Write-Host ""
}