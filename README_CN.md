# RIG-Omni

开源 ESP32-S3 多形态智能机器人固件

ESP32-S3 · 语音 AI · EAF 动画 · MCP 远程控制 · 多机器人架构

📖 [English](README.md)

---

## 目录

- [概述](#-概述)
- [功能特性](#-功能特性)
- [硬件](#-硬件)
- [架构](#-架构)
- [快速开始](#-快速开始)
- [项目结构](#-项目结构)
- [多板型配置](#-多板型配置)
- [开发指南](#-开发指南)
- [贡献](#-贡献)
- [许可证](#-许可证)

---

## 📖 概述

RIG-Omni 是一个基于 ESP32-S3 的开源嵌入式固件，能在同一代码库下驱动多种机器人形态——从 5 舵机机器狗（Puppy）到双轮气垫船（Hover）。

采用"一份共用核心，各形态独立扩展"的模块化架构，RIG-Omni 在单颗 ESP32-S3 芯片上集成了语音 AI 交互、EAF 表情动画、IMU 姿态融合、MCP 远程控制等功能，搭配 240×240 圆形 LCD 显示。

> 愿景：为每一位机器人爱好者提供直觉、模块化且愉悦的固件体验。

---

## ✨ 功能特性

| 类别 | 能力 |
| --- | --- |
| 🧠 语音 AI | 离线唤醒词、云端 ASR/TTS、VAD 检测、AGC 增益 |
| 🎭 表情动画 | EAF 动画引擎（LVGL），20+ 种表情，动态切换 |
| 🤖 运动控制 | 多舵机 / 轮式电机控制，IMU 姿态平衡，预设动作 |
| 📡 网络连接 | BluFi 蓝牙配网、OTA 固件升级、HMAC 设备激活 |
| 🔧 MCP 工具 | 可扩展远程指令框架 |
| 📷 摄像头 | ESP32-S3 摄像头集成，支持快照 |
| 🎮 遥控 | BLE 蓝牙手柄遥控 |
| 🖥️ 调试 | 实时调试 Web 服务器（Hover 电机调参） |

---

## 🔧 硬件

| # | 组件 | 接口 | 说明 |
| --- | --- | --- | --- |
| 1 | GC9A01 圆形 LCD（240×240） | SPI | 通过 LVGL 播放 EAF 表情动画 |
| 2 | IMU（QMI8658C） | I2C | 6 轴姿态 + 平衡控制 |
| 3 | 舵机（Puppy: 5 个，Hover: 1 个） | UART（XGO 协议） | 双向位置 + 速度控制 |
| 4 | 无刷串行电机（Hover: 2 轮） | UART（XGO 协议） | 差速驱动 |
| 5 | I2S 音频（直连） | I2S | 单工/双工 麦克风 + 扬声器（无硬件编解码芯片） |
| 6 | 摄像头（GC0308/OV2640） | DVP | MCP 工具快照 |
| 7 | Boot + 触摸按键 | GPIO | 配网、对话切换、NVS 重置（触摸 Hover 和 ARM 有） |

> 所有板型共用 ESP32-S3 核心 + GC9A01 显示。各形态的电机配置独立存放在对应板型目录。

---

## 🏗️ 架构

| 层级 | 组件 | 技术 |
| --- | --- | --- |
| **应用层** | 语音 AI · 表情显示 · MCP 服务 · 摄像头工具 | C++（ESP-IDF） |
| **板型抽象** | `boards/common/` — IMU · 按键 · BLE · 电池 · 摄像头 | C++ 共享驱动 |
| **机器人逻辑** | `boards/puppy/`（5舵机机器狗）· `boards/hover/`（1舵机+2轮气垫船）· `boards/arm/`（多舵机机械臂） | 各板型 C++ |
| **平台层** | WiFi · 蓝牙 · SPI · I2C · I2S · UART · GPIO | ESP-IDF v5.5+ |

---

## 🚀 快速开始

### 环境要求

- ESP32-S3 开发板（含 GC9A01 240×240 LCD）
- ESP-IDF v5.5.2+
- Python 3.8+（构建脚本）

### 编译

```bash
# 克隆仓库
git clone git@github.com:Xgorobot/RIG-Omni.git
cd RIG-Omni

# 激活 ESP-IDF 环境
source ~/esp/esp-idf/export.sh

# 选择板型和固件区域（交互菜单）
idf.py set-target esp32s3
idf.py menuconfig
# → RIG-Omni → Board Type → Puppy / Hover / ARM
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas

# 编译 & 烧录
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 发布打包

```bash
# 构建发布包（固件 + 资源 + manifest）
python tools/gen_bin_package.py
# 输出: bin/rig-puppy.bin (或 rig-hover.bin)
```

---

## 📂 项目结构

```
RIG-Omni/
├── main/                    # 固件源码
│   ├── audio/               # 音频编解码、唤醒词、处理器
│   ├── display/             # LCD 驱动、EAF 表情引擎、LVGL
│   ├── led/                 # LED 灯带 & GPIO 灯控制
│   ├── protocols/           # MQTT & WebSocket 通信
│   ├── boards/              # 硬件抽象层
│   │   ├── common/          # 共享驱动（IMU、按键、BLE、摄像头…）
│   │   ├── puppy/           # Puppy 机器人（5 舵机机器狗）
│   │   ├── hover/           # Hover 机器人（双轮气垫船）
│   │   └── arm/             # ARM 机器人（多舵机机械臂）
│   ├── assets/              # 语言包、字体
│   ├── application.cc/h     # 应用生命周期
│   ├── mcp_server.cc/h      # MCP 远程控制服务
│   ├── ota.cc/h             # OTA 固件升级
│   └── settings.cc/h        # 设备设置（NVS）
├── partitions/              # Flash 分区表
│   └── 16m.csv              # 16MB 分区方案
├── tools/                   # 构建 & 工具脚本
│   ├── gen_lang.py          # 语言配置生成
│   ├── build_default_assets.py  # 默认资源构建
│   └── spiffs_assets/       # SPIFFS 资源打包
├── CMakeLists.txt           # 根 CMake（ESP-IDF 项目）
├── sdkconfig.defaults       # 默认 Kconfig 设置
└── README.md
```

---

## 🤖 多板型配置

RIG-Omni 通过 Kconfig 在编译时选择目标机器人形态：

```bash
idf.py menuconfig
# RIG-Omni Configuration → Board Type
```

| 板型 | 电机 | 运动方式 | 核心文件 |
| --- | --- | --- | --- |
| **RIG-Puppy** | 5 舵机 | 狗步态、头部跟随、预设动作 | `boards/puppy/puppy_board.cc` |
| **RIG-Hover** | 1 舵机 + 2 DC 电机 | 平衡控制、差速驱动 | `boards/hover/hover_board.cc` |
| **RIG-ARM** | 多舵机（AX-12A） | 机械臂运动、标定、示教模式 | `boards/arm/arm_board.cc` |

每个板型拥有独立的：
- 电机控制逻辑（`xgo.cc/h`、`xgo_action.cc/h`）
- EAF 表情资源（`emoji/`、`240_240/`）
- 唤醒词模型（`wakenet/`）
- 调试工具（Hover 的 `hover_debug_server.cc/h`）

所有硬件驱动（IMU、蓝牙、按键、摄像头、电池）统一放在 `boards/common/`，全板型共享。

### 固件区域（国内 / 海外）

RIG-Omni 支持从同一代码库构建国内和海外两个版本的固件。在 menuconfig 中选择区域：

```bash
idf.py menuconfig
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas
```

| 配置项 | 国内 (Domestic) | 海外 (Overseas) |
| --- | --- | --- |
| OTA 地址 | `xl-api.xgorobot.com` | `xl-api.luwudynamics.ai` |
| 默认语言 | zh_CN | en_US |
| 唤醒词 | 自定义（如小陆同学） | 自定义 + Hey Kira |
| 配网表情 | 国内版二维码 | 海外版二维码 |

区域选择自动联动以下配置：
- **OTA 地址** — 不同的固件升级和服务器地址发现端点
- **默认语言** — 国内默认 zh_CN，海外默认 en_US（用户仍可通过 MCP 切换）
- **唤醒词** — 海外版自动启用 ESP-SR 内置的 "Hey Kira"，与自定义唤醒词并存
- **配网 EAF 动画** — 构建时自动选择对应区域的 `wificonfig.eaf`（含区域专属二维码）

`wificonfig.eaf` 在构建时自动生成。每个板型保留两个源文件：
```
main/boards/<board>/emoji/
    wificonfig_domestic.eaf    # 国内版二维码
    wificonfig_overseas.eaf    # 海外版二维码
    wificonfig.eaf             # 自动生成（已 gitignore）
```

---

## 🛠️ 开发指南

### 新增板型

```bash
# 1. 创建板型目录
mkdir -p main/boards/myrobot/240_240
mkdir -p main/boards/myrobot/emoji

# 2. 添加 board.cc + xgo.cc 电机逻辑
# 3. 在 Kconfig 注册（main/Kconfig.projbuild）
# 4. 在 CMakeLists.txt 添加编译配置
```

### MCP 工具

通过 MCP 框架扩展机器人能力。示例工具定义：

```cpp
mcp_server.AddTool("self.robot.move",
    "前进后退距离，单位厘米",
    PropertyList({Property("distance", kPropertyTypeInteger, -20, 20)}),
    [this](const PropertyList& props) -> ReturnValue {
        int distance = props["distance"].value<int>();
        // 执行移动...
        return true;
    });
```

### 调试监控

```bash
idf.py monitor
# 按 Ctrl+] 退出
```

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

## 📜 许可证

本项目采用 Apache License, Version 2.0 开源协议。

Copyright © 2024–2026 RIG-Omni Contributors

---

Built with ❤️ by the RIG-Omni Team
