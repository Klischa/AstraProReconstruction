# Astra Pro 数字孪生虚实联动项目

基于 ROS2 Jazzy 的 Astra Pro 相机深度图/点云处理与 Gazebo 数字孪生虚实联动系统。

## 项目概述

本项目实现了 **真实相机（Astra Pro）** 与 **Gazebo 虚拟环境** 的双向数字孪生联动：

1. **真实相机数据采集**：通过 Astra Pro 相机驱动获取深度图（18–25 Hz）、彩色图像（11–14 Hz，UVC MJPEG）、红外图像（27 Hz）
2. **虚拟环境同步**：将真实相机数据实时映射到 Gazebo 虚拟环境中的虚拟相机模型
3. **虚实双向联动**：模型在 Gazebo 中实时跟踪真实相机的深度距离；通过 `ros2 param set` 可远程调整虚拟相机位置

## 实测验证结果

| 话题 | 格式 | 频率 | 状态 |
|------|------|------|------|
| `/camera/depth/image_raw` | 16UC1, 640×480 | 18–25 Hz | ✅ |
| `/camera/color/image_raw` | rgb8, 640×480 | 11–14 Hz (UVC MJPEG) | ✅ |
| `/camera/ir/image_raw` | mono8, 640×480 | ~27 Hz | ✅ |
| `/twin/depth/image_raw` | 16UC1, 640×480 | ~27 Hz | ✅ |
| `/twin/color/image_raw` | rgb8, 640×480 | ~16 Hz | ✅ |
| `ros2 param set /gazebo_twin twin_camera_x 2.0` | 模型移动至 x=2 | - | ✅ |

- **节点发现**：在 dimon 用户下 `ros2 node list` / `ros2 param set` 正常
- **gz transport**：`/world/default/set_pose` 直接调用，模型响应确认
- **实测演示**：模型 x/y 跟随参数设置，z 自动跟踪真实深度值

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    真实世界                              │
│  ┌────────────────────┐   /camera/depth/image_raw       │
│  │ Astra Pro (2bc5)   │──▶ /camera/color/image_raw      │
│  │  Depth (OpenNI2)   │──▶ /camera/ir/image_raw         │
│  │  Color (UVC/libuvc)│──▶ /camera/depth_registered/points │
│  └────────────────────┘──▶ /camera/depth/camera_info    │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                 gazebo_twin 节点                         │
│                                                          │
│  1. 订阅真实相机话题                                     │
│  2. 发布 /twin/* 孪生话题                               │
│  3. 广播 TF 变换（world → twin_camera）                  │
│  4. gz transport: /world/default/set_pose               │
│     (gz::transport::Node::Request, gz.msgs.Pose)        │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                 虚拟世界 (Gazebo Harmonic)                │
│  ┌─────────────┐   /twin/depth/image_raw                │
│  │  twin_camera│──▶ /twin/color/image_raw                │
│  │  (虚拟模型) │──▶ /twin/depth_registered/points        │
│  └─────────────┘                                        │
│                                                          │
│  位姿同步：x/y 跟随参数, z = 实时深度                    │
└─────────────────────────────────────────────────────────┘
```

## 文件结构

```
ros2_virtual_physical_integration/
├── README.md
├── astra_examples/
│   ├── CMakeLists.txt
│   ├── package.xml
│   ├── src/
│   │   ├── gazebo_twin.cpp              # 核心：数字孪生节点（gz transport）
│   │   └── test_camera_publisher.cpp    # 模拟数据发布器
│   └── launch/
│       ├── gazebo_twin.launch.py        # 主启动（真实/模拟相机 + Gazebo + RViz）
│       ├── test_twin.launch.py          # 测试启动（模拟数据）
│       ├── twin_view.rviz               # RViz2 配置
│       ├── twin_camera.sdf              # 虚拟相机模型
│       └── empty.world                  # Gazebo 空世界
└── astra_camera_ros/
    └── astra_camera/                    # ros2_astra_camera v1.1.0 (Joe Dong)
        └── src/                         # OpenNI2 depth + libuvc color
```

## 环境要求

- **OS**: Ubuntu 24.04 (WSL2) + Windows 11
- **ROS**: Jazzy (desktop/full install)
- **Gazebo**: Harmonic（随 Jazzy desktop 安装）
- **相机驱动**: ros2_astra_camera v1.1.0（已含在本仓库）

### Astra Pro USB 支持（WSL2 + usbipd-win）

```powershell
# Windows 11 端 (PowerShell 管理员)
usbipd list                                          # 找到 Astra 的 BUSID (8-1, 8-2)
usbipd bind --busid 8-1; usbipd bind --busid 8-2    # 持久绑定
# 启动 WSL 后 attach（WSL 必须处于运行状态）:
usbipd attach --wsl=Ubuntu-24.04 --busid 8-1
usbipd attach --wsl=Ubuntu-24.04 --busid 8-2
```

### udev 规则（OpenNI2 设备访问）

```bash
# 在 WSL 内执行（dimon 用户）
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="2bc5", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-orbbec-astra.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
# 重新 attach USB 设备后生效
```

## 编译

```bash
source /opt/ros/jazzy/setup.bash
cd ~/ros2_learning/ros2_depth_projects/ref

# 编译全部三个包（astra_camera、astra_camera_msgs、astra_examples）
colcon build --packages-select astra_camera astra_camera_msgs astra_examples \
  --cmake-args -DCMAKE_BUILD_TYPE=Release

source install/setup.bash
```

## 运行方式

### 方式一：模拟数据（无需真实相机）

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash

ros2 launch astra_examples test_twin.launch.py gui:=true
```

### 方式二：真实相机 + Gazebo

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_learning/ros2_depth_projects/ref/install/setup.bash

# 轻量模式（无 Gazebo GUI，适合测试）
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true \
  gui:=false \
  use_uvc_camera:=true \
  uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false \
  enable_colored_point_cloud:=false

# 完整 GUI 模式（Gazebo + RViz）
ros2 launch astra_examples gazebo_twin.launch.py \
  use_real_camera:=true \
  gui:=true \
  use_uvc_camera:=true \
  uvc_camera_format:=mjpeg \
  depth_registration:=false \
  enable_point_cloud:=false \
  enable_colored_point_cloud:=false \
  enable_rviz:=true
```

### 启动参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `gui` | `true` | 显示 Gazebo 图形窗口 |
| `use_real_camera` | `false` | 使用真实 Astra 相机（false 则使用模拟数据） |
| `use_uvc_camera` | `false` | 通过 UVC/libuvc 获取彩色图像（推荐 true） |
| `uvc_camera_format` | `mjpeg` | UVC 格式：mjpeg / yuyv / uncompressed |
| `depth_registration` | `true` | 深度对齐彩色（GPU 重时建议 false） |
| `enable_point_cloud` | `true` | 发布点云（重载时建议 false） |
| `enable_colored_point_cloud` | `true` | 发布彩色点云 |
| `enable_rviz` | `false` | 启动 RViz2 |
| `enable_twin_sync` | `true` | 启用虚实同步 |

> **性能提示**：`use_uvc_camera:=true` 会额外使用 libuvc（USB）。在 WSL2 usbip 场景下，
> 如果 CPU 资源紧张，关闭 `depth_registration` 和 `enable_point_cloud` 可保证稳定运行。

### 显示实拍画面

```bash
# RGB 彩色图像
ros2 run image_view image_view image:=/camera/color/image_raw

# 深度图（原始 16UC1，接近全黑）
ros2 run image_view image_view image:=/camera/depth/image_raw

# 深度归一化（0-2000mm → grayscale，推荐）
python3 normalize_depth.py &    # 发布 /camera/depth/view
ros2 run image_view image_view image:=/camera/depth/view
```

## 位姿同步演示

启动后，模型在 Gazebo 中实时跟踪真实深度：

```bash
# 将模型移到 x=2.5, 观察 Gazebo 中模型移动
ros2 param set /gazebo_twin twin_camera_x 2.5

# 改变 y（观察模型在 Gazebo 中移动到侧面）
ros2 param set /gazebo_twin twin_camera_y 1.5

# 归位
ros2 param set /gazebo_twin twin_camera_x 0.0
ros2 param set /gazebo_twin twin_camera_y 0.0
```

> **注意**：z 值由实时深度自动决定（来自真实 Astra 深度测量），
> 无需手动设置 `twin_camera_z`。

## 核心节点说明

### gazebo_twin 节点

核心数字孪生节点（`astra_examples/gazebo_twin.cpp`）：

- **订阅**：`/camera/depth/image_raw`, `/camera/color/image_raw`, `/camera/depth/camera_info`
- **发布**：`/twin/depth/image_raw`, `/twin/color/image_raw`, `/twin/depth_registered/points`
- **Gazebo 服务**：`/world/default/set_pose`（`gz::transport::Node::Request<gz::msgs::Pose>`）
- **参数**：`twin_camera_x/y/z/qx/qy/qz/qw`, `twin_camera_model`, `gz_setpose_service`

### test_camera_publisher 节点

模拟数据发布器：渐变深度图 + 彩色渐变图 + 规则点云，无需真实硬件。

## 关键实现细节

### gz transport 位姿同步（非 ros_gz_bridge）

ROS2 Jazzy + Gazebo Harmonic 中，`ros_gz_bridge` 不提供 `SetEntityPose` 转换工厂。
本项目改为直接通过 gz transport 调用 Gazebo 原生服务：

```cpp
// gazebo_twin.cpp
gz::transport::Node gz_node_;
gz_node_.Request<gz::msgs::Pose, gz::msgs::Boolean>(
    gz_setpose_service_,        // "/world/default/set_pose"
    pose_msg,                   // gz::msgs::Pose
    5000,                       // timeout ms
    result,                     // gz::msgs::Boolean
    result_ok);                 // bool
```

### OpenNI2 + UVC 双路径彩色获取

`ros2_astra_camera` 支持两种彩色获取路径：
- **OpenNI2 color**（`use_uvc_camera:=false`）：通过 OpenNI2 SDK，Astra Pro 的 RGB 走 OpenNI 接口
- **UVC/libuvc**（`use_uvc_camera:=true`）：通过 libuvc 直接读取 `/dev/bus/usb` 上的 UVC 设备

本项目使用 UVC 路径（11–14 Hz MJPEG），因为 Astra Pro 在 OpenNI2 下的彩色流需要额外配置。

## 故障排查

| 问题 | 解决方案 |
|------|---------|
| `ros2 node list` 为空 / `ros2 param set` 失败 | 必须以 dimon 用户运行（非 sudo），节点名需要统一 DDS 域 |
| `Could not open 2bc5/0403: Access denied` | 缺少 udev 规则：见 udev 规则章节 |
| 彩色图像不发布 | 使用 `use_uvc_camera:=true`（推荐），Astra Pro 的 UVC 比 OpenNI2 彩色更可靠 |
| `color is started` 后无帧 | 确认 udev 规则已生效、设备未被其他进程占用 |
| RViz Image 面板空白 | `image_transport` 与 astra_camera 发布 QoS 不完全匹配，改用 `image_view` |
| Gazebo 模型不移动 | 确认无残留 gz 进程：`pkill -f '[g]z sim'`；确认 `enable_twin_sync:=true` |
| WSL2 usbip attach 失败 | attach 时 WSL 必须在运行；保持一个终端打开 `bash -c 'sleep 900'` |
| `AMENT_TRACE_SETUP_FILES: unbound variable` | 脚本中用 `set -eo pipefail`（不含 `-u`），避免与 setup.bash 冲突 |

## 技术栈

- **ROS2 Jazzy** — 机器人操作系统
- **Gazebo Harmonic** — 物理仿真引擎
- **ros2_astra_camera v1.1.0** — Astra Pro 相机驱动（OpenNI2 depth + libuvc color）
- **OpenCV** — 图像处理
- **gz-transport13 / gz-msgs10** — Gazebo 位姿服务直接调用
- **libuvc** — UVC 彩色图像获取
- **usbipd-win** — WSL2 USB 设备转发
- **TF2** — 坐标变换
