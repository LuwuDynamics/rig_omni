# RIG-Omni

开源 ESP32-S3 多形态智能机器人固件

ESP32-S3 · 语音 AI · 机器人运动 · EAF 动画 · TARS 终端 · MCP 工具

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
- [TARS 面板与操作](#tars-面板与操作)
- [开发指南](#-开发指南)
- [贡献](#-贡献)
- [许可证](#-许可证)

---

## 📖 概述

RIG-Omni 是基于 ESP32-S3 的开源机器人固件，同一代码库支持四种形态：五舵机机器狗 **Puppy**、双轮平衡机器人 **Hover**、机械臂 **ARM**，以及致敬《星际穿越》、配备横屏终端的行走机器人 **TARS**。

各板型共享语音交互、网络连接、传感器驱动和 MCP 工具，并独立实现运动控制与显示界面。Puppy、Hover、ARM 使用 240×240 圆屏，TARS 使用 320×240 横屏显示日志和实时状态。唤醒词在本地检测，对话识别和语音合成依赖网络服务。

> 愿景：为每一位机器人爱好者提供直觉、模块化且愉悦的固件体验。

---

## ✨ 功能特性

| 类别 | 能力 |
| --- | --- |
| 🧠 语音 AI | 离线唤醒词、云端 ASR/TTS、VAD 检测、AGC 增益 |
| 🎭 显示界面 | 圆屏 EAF 表情动画、TARS 专属双栏终端 |
| 🤖 运动控制 | 舵机步态、轮式平衡、Hover 云台模式、ARM 标定与示教 |
| 📡 网络连接 | BluFi 蓝牙配网、OTA 固件升级、HMAC 设备激活 |
| 🔧 MCP 工具 | 通过语音调用运动、标定、状态查询和 TARS 人格参数设置 |
| 📷 摄像头 | 配备摄像头的硬件支持快照工具 |
| 🎮 遥控 | 兼容机器人遥控 App / 小程序的 BLE 控制 |
| 🖥️ 调试 | 实时调试 Web 服务器（Hover 电机调参） |
| 📦 构建工具 | 按板型管理版本、区域资源打包、TARS 语音生成 |

---

## 🔧 硬件

| # | 组件 | 接口 | 说明 |
| --- | --- | --- | --- |
| 1 | GC9A01 圆屏 / ST7789V2 横屏 | SPI | Puppy/Hover/ARM 为 240×240；TARS 为 320×240 |
| 2 | IMU（QMI8658C） | I2C | 6 轴姿态 + 平衡控制 |
| 3 | 串行舵机 | UART | Puppy/Hover 使用 EM3；ARM/TARS 使用 SCS009 |
| 4 | KP4012 轮毂电机（Hover: 2 轮） | UART | 平衡、差速驱动和云台控制 |
| 5 | I2S 音频（直连） | I2S | 单工/双工 麦克风 + 扬声器（无硬件编解码芯片） |
| 6 | 摄像头（GC0308/OV2640） | DVP | MCP 工具快照 |
| 7 | Boot 按键 + 外置触摸模块 | GPIO | 功能随板型而异；TARS 使用 GPIO3 低电平有效的数字触摸输入 |

> 引脚和屏幕方向定义在 `main/boards/common/config.h` 与各板型的 `board_config.h` 中，请按实际硬件版本核对。

---

## 🏗️ 架构

| 层级 | 组件 | 技术 |
| --- | --- | --- |
| **应用层** | 语音 AI · 表情/TARS 面板 · MCP 服务 · 摄像头工具 | C++（ESP-IDF） |
| **共享驱动** | `main/boards/common/` — IMU · 按键 · BLE · 电池 · 摄像头 · 电机协议 | C++ 共享驱动 |
| **机器人逻辑** | `main/boards/{puppy,hover,arm,tars}/` — 板级初始化、运动、动作与电机适配 | 各板型 C++ |
| **平台层** | WiFi · 蓝牙 · SPI · I2C · I2S · UART · GPIO | ESP-IDF 5.5.x |

---

## 🚀 快速开始

### 环境要求

- 对应板型的 ESP32-S3 硬件，配备 16 MB Flash、PSRAM 和匹配的显示屏
- ESP-IDF 5.5.x；TARS 已使用 5.5.3 验证构建
- 工具脚本使用 Python 3.10+；固件构建使用 ESP-IDF 安装的 Python 环境
- 如需生成或重采样语音资源，安装 FFmpeg

### 编译

```bash
# 克隆仓库
git clone https://github.com/LuwuDynamics/rig_omni.git RIG-Omni
cd RIG-Omni

# 激活 ESP-IDF 环境
source ~/esp/esp-idf/export.sh

# 选择板型和固件区域（交互菜单）
idf.py set-target esp32s3
idf.py menuconfig
# → RIG-Omni → Board Type → RIG-Puppy / RIG-Hover / RIG-Arm / RIG-Tars
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas
# → RIG-Omni → Select display style → Emote animation style
# → RIG-Omni → Flash Assets → Flash Emote Assets

# 编译 & 烧录
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

将 `/dev/ttyUSB0` 替换为实际串口，macOS 下通常为 `/dev/cu.usbmodem…`。编译前选择固件区域，让配网二维码与要使用的网络服务匹配。

`idf.py build` 构建应用和所选板型的资源，`idf.py flash` 烧录配置中的镜像。首次烧录或切换板型、区域时，应一起更新匹配的应用和资源。修改板级 `.ogg` 会更新应用固件；修改 EAF 动画或打包的唤醒模型会更新资源镜像。

板型版本保存在 `main/boards/<board>/version.txt`，可在 `build/project_description.json` 中核对一次构建实际使用的版本。

### 发布打包

```bash
# 查看打包选择，不执行构建
python3 tools/package_luwu_firmware.py --board tars --dry-run

# 构建国内版、海外版和硬件测试固件包
python3 tools/package_luwu_firmware.py --board tars
# 多个板型：--board puppy,hover,arm,tars
# 交互选择：省略 --board
```

打包脚本还会构建独立的硬件测试工程，本地需另行准备 `Test_Firmware/` 或 `test_firmware/`。只需机器人固件时，使用上面的 `idf.py` 构建流程即可。

输出目录为 `releases/Luwu-Tools/bin/`：`RIG/<board>/` 存放国内版，`RIG/<board>/gb/` 存放海外版，`GARAGE/<board>/` 存放硬件测试固件。区域构建分别使用 `build/packages/` 下的独立目录，日志保存在 `releases/logs/`。每个正式固件包包含完整烧录、仅应用、仅资源三类 manifest。

TARS 的不同区域构建共用同一套专属英文提示音；区域选择决定网络地址、默认界面语言和配网二维码。

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
│   │   │   └── motors/     # EM3、SCS009、KP4012、共享 SCS 总线
│   │   ├── puppy/           # Puppy 机器人（5 舵机机器狗）
│   │   ├── hover/           # 双轮平衡与云台模式
│   │   ├── arm/             # 五舵机机械臂
│   │   └── tars/            # 三舵机机器人、终端面板、专属语音
│   ├── assets/              # 语言包、字体
│   ├── application.cc/h     # 应用生命周期
│   ├── mcp_server.cc/h      # MCP 远程控制服务
│   ├── ota.cc/h             # OTA 固件升级
│   └── settings.cc/h        # 设备设置（NVS）
├── partitions/              # Flash 分区表
│   └── 16m.csv              # 16 MB Flash：双 OTA 槽位 + 资源
├── tools/                   # 构建 & 工具脚本
│   ├── gen_lang.py          # 语言配置生成
│   ├── package_luwu_firmware.py  # 多板型、区域版本发布包
│   ├── generate_tars_voice_pack.py  # 火山引擎语音生成
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
| **RIG-Puppy** | 5 个 EM3 舵机 | 狗步态、头部跟随、预设动作 | `main/boards/puppy/puppy_board.cc` |
| **RIG-Hover** | 1 个 EM3 舵机 + 2 个 KP4012 轮毂电机 | 平衡、差速驱动、云台模式 | `main/boards/hover/hover_board.cc` |
| **RIG-ARM** | 5 个 SCS009 舵机 | 机械臂运动、标定、示教模式 | `main/boards/arm/arm_board.cc` |
| **RIG-TARS** | 3 个 SCS009 舵机 | 行走、转向、标定、实时状态显示 | `main/boards/tars/tars_board.cc` |

每个板型拥有独立的：

- 板级初始化（`<board>_board.cc`）和引脚配置（`board_config.h`）
- 运动逻辑（`robot.cc/h`、`robot_action.cc/h`）与电机适配（`motor.cc/h`）
- 显示资源（`emoji/`、`240_240/`；TARS 使用 `320_240/`）
- 唤醒词模型（`wakenet/`）
- 调试工具（Hover 的 `hover_debug_server.cc/h`）

共享驱动位于 `main/boards/common/`，其中 `motors/` 提供 EM3、SCS009、KP4012 协议及 SCS 总线实现，各板型的电机适配层负责具体电机 ID 和配置。

### TARS 面板与操作

TARS 使用 **320×240 ST7789V2 横屏**，以终端界面致敬《星际穿越》：

- **左侧：**密集蓝色日志，使用 6 px 字体滚动显示。
- **右侧：**绿色状态信息，使用 8 px 字体，每秒刷新对话状态、舵机角度、IMU 姿态和电量等数据。
- **底部：**12 px 加粗的 `HONESTY`（诚实度）与 `HUMOR`（幽默度）百分比，上方横线隔开日志区。默认值分别为 **90%** 和 **75%**，可通过对话调用 `self.tars.set_personality` 修改。
- **配网时：**临时切换到对应区域的二维码动画，避免日志遮挡二维码。

外置触摸模块连接 **GPIO3，低电平有效**。标定中短按可确认当前舵机零位并退出标定；正常运行时长按至少 **3 秒后松手**进入深度睡眠，睡眠后短按唤醒。非标定状态下的短按未分配操作。

TARS 专属提示音位于 `main/boards/tars/tars_*.ogg`。使用自己的火山引擎音色生成指定提示音：

```bash
export VOLC_TTS_SPEAKER='your-speaker-id'
# 在本机环境中设置 VOLC_SPEECH_API_KEY。
python3 tools/generate_tars_voice_pack.py --only message_send,over --dry-run
python3 tools/generate_tars_voice_pack.py --only message_send,over --overwrite
```

生成结果保存在 `tools/tars_voice_output/`，试听后再替换对应板级文件并重新构建；脚本不会自动安装语音到固件。非 16 kHz 的生成结果会通过 FFmpeg 转换为输入采样率头为 16 kHz 的 Ogg Opus。`ffprobe` 可能显示 Opus 解码采样率为 48 kHz，这与输入采样率头不同。

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
| 唤醒词 | 板型模型及当前 ESP-SR 配置 | 板型模型 + Hey Kira |
| 配网表情 | 国内版二维码 | 海外版二维码 |

区域选择自动联动以下配置：

- **OTA 地址** — 不同的固件升级和服务器地址发现端点
- **默认语言** — 国内默认 zh_CN，海外默认 en_US（用户仍可通过 MCP 切换）
- **唤醒词** — 海外版启用 ESP-SR 内置的 "Hey Kira"；实际打包模型同时取决于板型及当前 ESP-SR 配置
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

# 2. 添加板级初始化、robot.cc、robot_action.cc 和 motor.cc
# 3. 在 Kconfig 注册（main/Kconfig.projbuild）
# 4. 在 CMakeLists.txt 添加编译配置
```

通用电机协议放在 `main/boards/common/motors/`，板型专属引脚放在 `board_config.h`。新增或删除音频文件后，先执行 `idf.py reconfigure`，再构建以刷新嵌入资源列表。

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
