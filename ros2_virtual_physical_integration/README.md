# Astra Pro цифровой двойник: реальная камера ⇄ Gazebo

> Двуязычный README: **[Русский](#русская-версия)** · [English](#english-version)

Система реального времени на ROS2 Jazzy: реальная камера **Astra Pro** синхронизируется с виртуальной моделью в **Gazebo Sim** (Harmonic). Читает глубину через OpenNI2 (18–25 Hz), цвет через UVC/libuvc (11–14 Hz) и в реальном времени повторяет данные на виртуальном двойнике через gz transport.

![Стек](https://img.shields.io/badge/ROS2-Jazzy-blue) ![Gazebo](https://img.shields.io/badge/Gazebo-Harmonic-orange) ![Camera](https://img.shields.io/badge/Astra_Pro-Orbbec_2bc5-green)

---

# Русская версия

## Обзор

Проект реализует **двустороннюю цифровую синхронизацию** реальной камеры Astra Pro и виртуального мира Gazebo:

1. **Сбор реальных данных**: глубина (OpenNI2), цвет (UVC MJPEG), ИК-кадры, облако точек
2. **Синхронизация в Gazebo**: данные реальной камеры в реальном времени публикуются как `/twin/*` и отображаются на виртуальной модели
3. **Двусторонняя связь**: модель в Gazebo повторяет реальную глубину (ось z) и управляется через `ros2 param set` (оси x/y)

## Подтверждённые результаты

| Топик | Формат | Частота | Статус |
|-------|--------|---------|--------|
| `/camera/depth/image_raw` | 16UC1, 640×480 | 18–25 Hz | ✅ |
| `/camera/color/image_raw` | rgb8, 640×480 | 11–14 Hz (UVC MJPEG) | ✅ |
| `/camera/ir/image_raw` | mono8, 640×480 | ~27 Hz | ✅ |
| `/twin/depth/image_raw` | 16UC1, 640×480 | ~27 Hz | ✅ |
| `/twin/color/image_raw` | rgb8, 640×480 | ~16 Hz | ✅ |
| `twin_camera_x/y` через param set | модель движется по x/y | — | ✅ |
| Ось z модели | автоматически = реальная глубина | — | ✅ |

- **Node discovery**: `ros2 node list` / `ros2 param set` работают от пользователя `dimon`
- **gz transport**: вызов `/world/default/set_pose` напрямую, позиция подтверждена
- **Живое демо**: x/y следуют параметрам, z отслеживает реальное расстояние до объекта

## Архитектура

```
┌─────────────────────────────────────────────────────────┐
│                       Реальный мир                      │
│  ┌────────────────────┐   /camera/depth/image_raw       │
│  │ Astra Pro (v2bc5)  │──▶ /camera/color/image_raw      │
│  │ Depth (OpenNI2)    │──▶ /camera/ir/image_raw         │
│  │ Color (UVC/libuvc) │──▶ /camera/depth_registered/points │
│  └────────────────────┘──▶ /camera/depth/camera_info    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                      gazebo_twin node                  │
│  1. Подписка на топики реальной камеры                 │
│  2. Публикация /twin/*                                 │
│  3. TF: world → twin_camera                            │
│  4. gz transport: /world/default/set_pose              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  Виртуальный мир (Gazebo)               │
│  /twin/depth/image_raw, /twin/color/image_raw,          │
│  /twin/depth_registered/points                          │
│  Модель twin_camera: x/y из парам., z = реальная глубина│
└─────────────────────────────────────────────────────────┘
```

## Структура репозитория

```
├── ros2_virtual_physical_integration/
│   ├── README.md                        # Этот документ
│   ├── astra_examples/                  # Запуск + логика двойника
│   │   ├── src/
│   │   │   ├── gazebo_twin.cpp          # Основной узел (gz transport)
│   │   │   └── test_camera_publisher.cpp # Симуляция данных камеры
│   │   └── launch/
│   │       ├── gazebo_twin.launch.py    # Запуск (реальная/симуляция)
│   │       ├── scan_camera.launch.py    # Камера standalone (для сканирования)
│   │       ├── test_twin.launch.py      # Режим без реальной камеры
│   │       ├── twin_camera.sdf          # 3D модель виртуальной камеры
│   │       ├── twin_view.rviz           # Конфиг RViz2
│   │       └── empty.world              # Пустой мир Gazebo
│   ├── astra_scan/                      # Фильтр виртуального куба сканирования
│   │   ├── astra_scan/box_filter.py     # Узел box_crop_filter (Python)
│   │   └── launch/
│   │       ├── scan_box.launch.py       # Запуск фильтра
│   │       └── scan_box.rviz            # RViz-конфиг для куба
│   └── astra_camera_ros/astra_camera/   # Драйвер ros2_astra_camera
│       └── src/ob_camera_node_factory.cpp
├── astra-ros2-launcher/                 # PowerShell-скрипты (старт/стоп/бэг)
├── camera_stream/                       # Прошивка камеры (Arduino)
└── drivers/                             # Драйверы USB (CH341/CP210x)
```

## Требования

- **ОС**: Ubuntu 24.04 (WSL2) + Windows 11
- **ROS2**: Jazzy (desktop edition)
- **Gazebo**: Harmonic (в составе Jazzy desktop)
- **Драйвер**: ros2_astra_camera v1.1.0 (уже в репозитории)

### Astra Pro USB через WSL2 (usbipd-win)

```powershell
# Windows 11 (PowerShell от админа)
usbipd list                                          # найти BUSID (например 8-1, 8-2)
usbipd bind --busid 8-1; usbipd bind --busid 8-2     # закрепить за WSL

# WSL должен быть запущен (например `wsl -d Ubuntu-24.04 sleep 900`), затем:
usbipd attach --wsl=Ubuntu-24.04 --busid 8-1
usbipd attach --wsl=Ubuntu-24.04 --busid 8-2
```

### Правило udev (доступ OpenNI2 к устройству)

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="2bc5", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-orbbec-astra.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
# после этого пере-подключить USB (usbipd attach)
```

## Сборка

```bash
source /opt/ros/jazzy/setup.bash
cd ~/ros2_learning/ros2_depth_projects/ref
colcon build --packages-select astra_camera astra_camera_msgs astra_examples \
  --cmake-args -DCMAKE_BUILD_TYPE=Release
source install/setup.bash
```

## Запуск

### Режим симуляции (без реальной камеры)

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash
ros2 launch astra_examples test_twin.launch.py gui:=true
```

### Реальная камера + Gazebo

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash

# Без GUI (для теста)
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true gui:=false \
  use_uvc_camera:=true uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false enable_colored_point_cloud:=false

# Полный GUI (Gazebo + RViz)
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true gui:=true \
  use_uvc_camera:=true uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false enable_colored_point_cloud:=false \
  enable_rviz:=true
```

### Параметры запуска

| Параметр | По умолчанию | Описание |
|----------|--------------|----------|
| `gui` | `true` | Окно Gazebo |
| `use_real_camera` | `false` | Реальная камера (иначе — симуляция) |
| `use_uvc_camera` | `false` | Цвет через UVC/libuvc (**рекомендуется true**) |
| `uvc_camera_format` | `mjpeg` | mjpeg / yuyv / uncompressed |
| `depth_registration` | `true` | Выравнивание глубины к цвету |
| `enable_point_cloud` | `true` | Публикация облака точек |
| `enable_colored_point_cloud` | `true` | Цветное облако точек |
| `enable_rviz` | `false` | Запуск RViz2 |
| `enable_twin_sync` | `true` | Синхронизация двойника |

> 💡 На слабых машинах отключите `depth_registration` и `enable_point_cloud` — стабильность выше.

### Просмотр реальных кадров

```bash
# Цвет
ros2 run image_view image_view image:=/camera/color/image_raw

# Глубина (сырая 16UC1 — почти чёрная)
ros2 run image_view image_view image:=/camera/depth/image_raw

# Глубина, нормализованная в градации серого (0–2000 мм):
python3 normalize_depth.py &    # публикует /camera/depth/view
ros2 run image_view image_view image:=/camera/depth/view
```

## Сканирование объектов с выбором зоны (виртуальный куб)

Полная сессия: камера отдельно (без сцены Gazebo) + **RTAB-Map** SLAM + фильтр куба.

```bash
# 1. Камера standalone (стандарт, registration + цветное облако)
ros2 launch astra_examples scan_camera.launch.py

# 2. RTAB-Map RGB-D SLAM (построение карты)
ros2 launch rtabmap_launch rtabmap.launch.py \
  rgb_topic:=/camera/color/image_raw \
  depth_topic:=/camera/depth/image_raw \
  camera_info_topic:=/camera/depth/camera_info \
  frame_id:=camera_link approx_sync:=true approx_sync_max_interval:=0.2 \
  args:="--delete_db_on_start -d" database_path:=~/scans/rtabmap.db \
  rtabmap_viz:=true rviz:=false
```

> **Важно**: для сканирования нельзя одновременно держать `gazebo_twin` — он публикует
> свой TF `world → twin_camera` (два несвязанных дерева TF ломают RTAB-Map).

### Фильтр виртуального куба `scan_box_filter`

Узел подписывается на накопленную карту `/rtabmap/cloud_map`, оставляет только точки
внутри **настраиваемого 3D-куба** и публикует результат в `/scan/box_points`.
Размеры и положение куба меняются параметрами в реальном времени (без перезапуска).

```bash
# Запуск
ros2 launch astra_scan scan_box.launch.py
# или напрямую:
scan_box_filter --ros-args \
  -p input_topic:=/rtabmap/cloud_map -p output_topic:=/scan/box_points \
  -p center_z:=0.7 -p size_x:=0.8 -p size_y:=0.8 -p size_z:=0.8

# Центр куба (map frame, метры)
ros2 param set /scan_box_filter center_x 0.15
ros2 param set /scan_box_filter center_y -0.05
ros2 param set /scan_box_filter center_z 0.9

# Размеры куба (метры)
ros2 param set /scan_box_filter size_x 1.2
ros2 param set /scan_box_filter size_y 1.2
ros2 param set /scan_box_filter size_z 1.2

# Точки можно инвертировать: invert=true — всё ВНЕ куба
ros2 param set /scan_box_filter invert true
```

Параметры: `center_x/y/z`, `size_x/y/z`, `invert` (можно задать и явные границы
`min_x/min_y/min_z`, `max_x/max_y/max_z`). Куб виден в RViz как полупрозрачный
куб-маркер (`/scan_box/marker`). Для просмотра: `ros2 run rviz2 rviz2 -d astra_scan/launch/scan_box.rviz`.

### Сохранение обрезанного облака в PLY

```bash
python3 astra-ros2-launcher/save_cloud.py \
  --out ~/scans/object.ply --timeout 4 --topic /scan/box_points
```

## Демо синхронизации позиции

```bash
ros2 param set /gazebo_twin twin_camera_x 2.5   # модель едет вправо
ros2 param set /gazebo_twin twin_camera_y 1.5   # модель едет вглубь
ros2 param set /gazebo_twin twin_camera_x 0.0   # вернуть в центр
ros2 param set /gazebo_twin twin_camera_y 0.0
```

> **Важно**: ось `z` задаётся автоматически — это реальное измеренное расстояние
> (из глубины камеры). Задавать `twin_camera_z` вручную не нужно.

## Ключевые узлы

### gazebo_twin

- **Подписки**: `/camera/depth/image_raw`, `/camera/color/image_raw`, `/camera/depth/camera_info`
- **Публикации**: `/twin/depth/image_raw`, `/twin/color/image_raw`, `/twin/depth_registered/points`
- **Служба Gazebo**: `/world/default/set_pose`
- **Параметры**: `twin_camera_x/y/z`, `twin_camera_qx/qy/qz/qw`, `twin_camera_model`, `gz_setpose_service`

### test_camera_publisher

Симулирует данные камеры без реального устройства: градиентная глубина, цвет, регулярное облако точек.

## Ключевые технические решения

### Синхронизация позиции через gz transport

В ROS2 Jazzy + Gazebo Harmonic `ros_gz_bridge` не предоставляет конвертер для `SetEntityPose`.
Поэтому двойник вызывает нативный сервис Gazebo напрямую:

```cpp
gz::transport::Node gz_node_;
gz_node_.Request<gz::msgs::Pose, gz::msgs::Boolean>(
    gz_setpose_service_,   // "/world/default/set_pose"
    pose_msg, 5000, result, result_ok);
```

### Оба пути получения цвета (OpenNI2 + UVC)

`ros2_astra_camera` умеет цвет двумя способами:
- **OpenNI2 color** (`use_uvc_camera:=false`) — через API OpenNI2
- **UVC/libuvc** (`use_uvc_camera:=true`) — через libuvc и `/dev/bus/usb`

На Astra Pro рекомендуем UVC: стабильнее и падает в 11–14 Hz (MJPEG 640×480).

## Поиск неисправностей

| Проблема | Решение |
|----------|---------|
| `ros2 node list` пустой / `param set` не работает | запускать от пользователя (не sudo), DDS-домен должен совпадать |
| `Could not open 2bc5/0403: Access denied` | применить udev-правило (см. выше) |
| Нет цветного потока | `use_uvc_camera:=true` |
| RViz не показывает изображения | использовать `image_view` (QoS mismatch) |
| Модель в Gazebo не двигается | `pkill -f '[g]z sim'`; `enable_twin_sync:=true` |
| `usbipd attach` не работает | WSL должен быть запущен; держите окно `sleep 900` |
| `AMENT_TRACE_SETUP_FILES: unbound variable` | в скриптах `set -eo pipefail` (без `-u`) |

## Технологии

- **ROS2 Jazzy** · **Gazebo Harmonic** · **ros2_astra_camera 1.1.0**
- **OpenNI2** (глубина) · **libuvc** (цвет) · **OpenCV** · **TF2**
- **gz-transport13 / gz-msgs10** · **usbipd-win** (WSL2 USB)

---

# English Version

## Overview

Real-time digital twin on **ROS2 Jazzy**: the physical **Astra Pro** camera streams depth
(OpenNI2, 18–25 Hz), color (UVC/libuvc, 11–14 Hz) and IR into **Gazebo Sim** (Harmonic),
which mirrors the data on a virtual camera model via gz transport.

![Stack](https://img.shields.io/badge/ROS2-Jazzy-blue) ![Gazebo](https://img.shields.io/badge/Gazebo-Harmonic-orange) ![Camera](https://img.shields.io/badge/Astra_Pro-Orbbec_2bc5-green)

## Features

1. **Real data acquisition**: depth (OpenNI2), color (UVC MJPEG), IR frames, point cloud
2. **Gazebo mirroring**: real data republished as `/twin/*` and shown on the virtual model
3. **Two-way sync**: the Gazebo model tracks real depth on the `z` axis and moves on `x/y` via `ros2 param set`

## Verified results

| Topic | Format | Rate | Status |
|-------|--------|------|--------|
| `/camera/depth/image_raw` | 16UC1, 640×480 | 18–25 Hz | ✅ |
| `/camera/color/image_raw` | rgb8, 640×480 | 11–14 Hz (UVC MJPEG) | ✅ |
| `/camera/ir/image_raw` | mono8, 640×480 | ~27 Hz | ✅ |
| `/twin/depth/image_raw` | 16UC1, 640×480 | ~27 Hz | ✅ |
| `/twin/color/image_raw` | rgb8, 640×480 | ~16 Hz | ✅ |
| `twin_camera_x/y` via param set | model moves on x/y | — | ✅ |
| Model `z` axis | automatically = real depth | — | ✅ |

## Architecture

```
┌────────────────────────────────────────────────────────┐
│                     Real world                        │
│  ┌───────────────────┐   /camera/depth/image_raw      │
│  │ Astra Pro (2bc5)  │──▶ /camera/color/image_raw     │
│  │ Depth (OpenNI2)   │──▶ /camera/ir/image_raw        │
│  │ Color (UVC/libuvc)│──▶ /camera/depth_registered/points │
│  └───────────────────┘──▶ /camera/depth/camera_info   │
└────────────────────────────────────────────────────────┘
                        │
                        ▼
┌────────────────────────────────────────────────────────┐
│                   gazebo_twin node                    │
│  1. Subscribes to real camera topics                  │
│  2. Publishes /twin/*                                 │
│  3. TF: world → twin_camera                           │
│  4. gz transport: /world/default/set_pose             │
└────────────────────────────────────────────────────────┘
                        │
                        ▼
┌────────────────────────────────────────────────────────┐
│                 Virtual world (Gazebo)                │
│  /twin/depth/image_raw, /twin/color/image_raw,        │
│  /twin/depth_registered/points                        │
│  Model twin_camera: x/y from params, z = real depth   │
└────────────────────────────────────────────────────────┘
```

## Repository layout

```
├── ros2_virtual_physical_integration/
│   ├── README.md                       # This document
│   ├── astra_examples/                 # Launch + twin logic
│   │   ├── src/gazebo_twin.cpp         # Core node (gz transport)
│   │   ├── src/test_camera_publisher.cpp # Camera simulation
│   │   └── launch/                     # launch.py, SDF, RViz, world
│   ├── astra_scan/                     # Virtual scan-box filter
│   │   ├── astra_scan/box_filter.py    # scan_box_filter node (Python)
│   │   └── launch/                     # launch.py + rviz config
│   └── astra_camera_ros/astra_camera/  # ros2_astra_camera driver
├── astra-ros2-launcher/                # PowerShell helpers (start/stop/bag)
├── camera_stream/                      # Camera firmware (Arduino)
└── drivers/                            # USB drivers (CH341/CP210x)
```

## Requirements

- **OS**: Ubuntu 24.04 (WSL2) + Windows 11
- **ROS2**: Jazzy (desktop)
- **Gazebo**: Harmonic (ships with Jazzy desktop)
- **Driver**: ros2_astra_camera v1.1.0 (in this repo)

### Astra Pro USB over WSL2 (usbipd-win)

```powershell
# Windows 11 (admin PowerShell)
usbipd list                                          # find BUSID (e.g. 8-1, 8-2)
usbipd bind --busid 8-1; usbipd bind --busid 8-2     # persist binding

# WSL must be running (e.g. `wsl -d Ubuntu-24.04 sleep 900`), then:
usbipd attach --wsl=Ubuntu-24.04 --busid 8-1
usbipd attach --wsl=Ubuntu-24.04 --busid 8-2
```

### udev rule (OpenNI2 device access)

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="2bc5", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-orbbec-astra.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
# then re-attach the USB device (usbipd attach)
```

## Build

```bash
source /opt/ros/jazzy/setup.bash
cd ~/ros2_learning/ros2_depth_projects/ref
colcon build --packages-select astra_camera astra_camera_msgs astra_examples astra_scan \
  --cmake-args -DCMAKE_BUILD_TYPE=Release
source install/setup.bash
```

## Run

### Simulation mode (no real camera)

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash
ros2 launch astra_examples test_twin.launch.py gui:=true
```

### Real camera + Gazebo

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash

# Headless (testing)
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true gui:=false \
  use_uvc_camera:=true uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false enable_colored_point_cloud:=false

# Full GUI (Gazebo + RViz)
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true gui:=true \
  use_uvc_camera:=true uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false enable_colored_point_cloud:=false \
  enable_rviz:=true
```

### Launch parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `gui` | `true` | Show Gazebo window |
| `use_real_camera` | `false` | Use real camera (else simulation) |
| `use_uvc_camera` | `false` | Color via UVC/libuvc (**recommended true**) |
| `uvc_camera_format` | `mjpeg` | mjpeg / yuyv / uncompressed |
| `depth_registration` | `true` | Depth aligned to color |
| `enable_point_cloud` | `true` | Publish point cloud |
| `enable_colored_point_cloud` | `true` | Colored point cloud |
| `enable_rviz` | `false` | Launch RViz2 |
| `enable_twin_sync` | `true` | Enable twin sync |

> 💡 On weak machines disable `depth_registration` and `enable_point_cloud` for stability.

### Viewing live frames

```bash
# Color
ros2 run image_view image_view image:=/camera/color/image_raw

# Raw depth (16UC1 — mostly black)
ros2 run image_view image_view image:=/camera/depth/image_raw

# Depth normalized to grayscale (0–2000 mm):
python3 normalize_depth.py &    # publishes /camera/depth/view
ros2 run image_view image_view image:=/camera/depth/view
```

## Object scanning with a selectable region (virtual box)

Full session: standalone camera (no Gazebo scene) + **RTAB-Map** SLAM + box filter.

```bash
# 1. Standalone camera (registration + colored cloud)
ros2 launch astra_examples scan_camera.launch.py

# 2. RTAB-Map RGB-D SLAM
ros2 launch rtabmap_launch rtabmap.launch.py \
  rgb_topic:=/camera/color/image_raw \
  depth_topic:=/camera/depth/image_raw \
  camera_info_topic:=/camera/depth/camera_info \
  frame_id:=camera_link approx_sync:=true approx_sync_max_interval:=0.2 \
  args:="--delete_db_on_start -d" database_path:=~/scans/rtabmap.db \
  rtabmap_viz:=true rviz:=false
```

> **Note**: do not keep `gazebo_twin` running while scanning — it publishes its own
> TF `world → twin_camera` (two unconnected TF trees break RTAB-Map).

### Virtual box filter `scan_box_filter`

The node subscribes to the accumulated map `/rtabmap/cloud_map`, keeps only points
inside an **adjustable 3D box** and publishes the result to `/scan/box_points`.
Box position/size change live via parameters (no restart needed).

```bash
# Launch
ros2 launch astra_scan scan_box.launch.py
# or directly:
scan_box_filter --ros-args \
  -p input_topic:=/rtabmap/cloud_map -p output_topic:=/scan/box_points \
  -p center_z:=0.7 -p size_x:=0.8 -p size_y:=0.8 -p size_z:=0.8

# Box center (map frame, meters)
ros2 param set /scan_box_filter center_x 0.15
ros2 param set /scan_box_filter center_y -0.05
ros2 param set /scan_box_filter center_z 0.9

# Box size (meters)
ros2 param set /scan_box_filter size_x 1.2
ros2 param set /scan_box_filter size_y 1.2
ros2 param set /scan_box_filter size_z 1.2

# Invert selection: everything OUTSIDE the box
ros2 param set /scan_box_filter invert true
```

Parameters: `center_x/y/z`, `size_x/y/z`, `invert` (explicit bounds
`min_x/min_y/min_z`, `max_x/max_y/max_z` are supported too). The box is shown in
RViz as a translucent cube marker (`/scan_box/marker`). View it with:
`ros2 run rviz2 rviz2 -d astra_scan/launch/scan_box.rviz`.

### Saving the cropped cloud to PLY

```bash
python3 astra-ros2-launcher/save_cloud.py \
  --out ~/scans/object.ply --timeout 4 --topic /scan/box_points
```

## Pose-sync demo

```bash
ros2 param set /gazebo_twin twin_camera_x 2.5   # model moves right
ros2 param set /gazebo_twin twin_camera_y 1.5   # model moves deeper
ros2 param set /gazebo_twin twin_camera_x 0.0   # back to center
ros2 param set /gazebo_twin twin_camera_y 0.0
```

> **Note**: the `z` axis is set automatically from the measured depth — no manual `twin_camera_z`.

## Core nodes

### gazebo_twin

- **Subscribes**: `/camera/depth/image_raw`, `/camera/color/image_raw`, `/camera/depth/camera_info`
- **Publishes**: `/twin/depth/image_raw`, `/twin/color/image_raw`, `/twin/depth_registered/points`
- **Gazebo service**: `/world/default/set_pose`
- **Parameters**: `twin_camera_x/y/z`, `twin_camera_qx/qy/qz/qw`, `twin_camera_model`, `gz_setpose_service`

### test_camera_publisher

Simulates camera data without hardware: gradient depth, color, regular point cloud.

## Key technical decisions

### Pose sync via gz transport

ROS2 Jazzy + Gazebo Harmonic has no `SetEntityPose` conversion in `ros_gz_bridge`,
so the twin calls the native Gazebo service directly:

```cpp
gz::transport::Node gz_node_;
gz_node_.Request<gz::msgs::Pose, gz::msgs::Boolean>(
    gz_setpose_service_,   // "/world/default/set_pose"
    pose_msg, 5000, result, result_ok);
```

### Both color paths (OpenNI2 + UVC)

`ros2_astra_camera` supports two color paths:
- **OpenNI2 color** (`use_uvc_camera:=false`) — via the OpenNI2 API
- **UVC/libuvc** (`use_uvc_camera:=true`) — via libuvc and `/dev/bus/usb`

On Astra Pro we recommend UVC: more stable and delivers 11–14 Hz (MJPEG 640×480).

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Empty `ros2 node list` / `param set` fails | run as a normal user (not sudo), same DDS domain |
| `Could not open 2bc5/0403: Access denied` | apply the udev rule (above) |
| No color stream | `use_uvc_camera:=true` |
| RViz shows no images | use `image_view` (QoS mismatch) |
| Model does not move in Gazebo | `pkill -f '[g]z sim'`; `enable_twin_sync:=true` |
| `usbipd attach` fails | keep WSL running while attaching; a `sleep 900` session helps |
| `AMENT_TRACE_SETUP_FILES: unbound variable` | use `set -eo pipefail` (without `-u`) in scripts |

## Tech stack

- **ROS2 Jazzy** · **Gazebo Harmonic** · **ros2_astra_camera 1.1.0**
- **OpenNI2** (depth) · **libuvc** (color) · **OpenCV** · **TF2**
- **gz-transport13 / gz-msgs10** · **usbipd-win** (WSL2 USB)