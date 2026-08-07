# RIG-Omni

An Open-Source ESP32-S3 Firmware for Multipurpose Intelligent Robots

ESP32-S3 · Voice AI · EAF Animation · MCP Remote Control · Multi-Bot Architecture

📖 [中文文档](README_CN.md)

---

## Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Hardware](#-hardware)
- [Architecture](#-architecture)
- [Quick Start](#-quick-start)
- [Project Structure](#-project-structure)
- [Multi-Board Configuration](#-multi-board-configuration)
- [Development](#-development)
- [Contributing](#-contributing)
- [License](#-license)

---

## 📖 Overview

RIG-Omni is an open-source embedded firmware for ESP32-S3 powered intelligent robots. It drives multiple robot forms — from a 5-servo robot dog (Puppy) to a 2-wheel hovercraft (Hover) — all from a single unified codebase.

Powered by a modular "one common core, per-form specialization" architecture, RIG-Omni delivers voice AI interaction, EAF-based emotion animations, IMU sensor fusion, MCP remote control tools, and more — all running on a single ESP32-S3 chip with a 240×240 round LCD.

> Mission: Give every robot builder an intuitive, modular, and delightful firmware experience.

---

## ✨ Features

| Category | Capability |
| --- | --- |
| 🧠 Voice AI | Offline wake word, cloud ASR/TTS, VAD, AGC |
| 🎭 Emotion Display | EAF animation engine (LVGL), 20+ expressions, dynamic transitions |
| 🤖 Motion Control | Multi-servo & wheel motor control, IMU-based balance, preset actions |
| 📡 Connectivity | BluFi WiFi provisioning, OTA firmware update, HMAC device activation |
| 🔧 MCP Tools | Extensible tool framework for remote robot commands |
| 📷 Camera | ESP32-S3 camera integration with snapshot capabilities |
| 🎮 Remote Control | Bluetooth Low Energy (BLE) gamepad support |
| 🖥️ Debug | Real-time hover debug web server for motor tuning |

---

## 🔧 Hardware

| # | Component | Interface | Details |
| --- | --- | --- | --- |
| 1 | GC9A01 Round LCD (240×240) | SPI | EAF emotion animations via LVGL |
| 2 | IMU (QMI8658C) | I2C | 6-axis attitude + balance control |
| 3 | Servo Motors (Puppy: 5, Hover: 1) | UART (XGO Protocol) | Bidirectional position + speed control |
| 4 | Brushless Serial Motors (Hover: 2 wheels) | UART (XGO Protocol) | Differential drive |
| 5 | I2S Audio (Direct) | I2S | Simplex/Duplex mic + speaker (no hardware codec chip) |
| 6 | Camera (GC0308/OV2640) | DVP | Snapshot via MCP tools |
| 7 | Boot + Touch Button | GPIO | WiFi config, chat toggle, NVS reset (Touch on Hover & ARM) |

> All boards share the same ESP32-S3 core with GC9A01 display. Per-form motor configurations are isolated in their respective board directories.

---

## 🏗️ Architecture

| Layer | Components | Technology |
| --- | --- | --- |
| **Application** | Voice AI · Emote Display · MCP Server · Camera Tools | C++ (ESP-IDF) |
| **Board Abstraction** | `boards/common/` — IMU · Button · BLE · Battery · Camera | C++ shared drivers |
| **Robot Logic** | `boards/puppy/` (5-Servo Dog) · `boards/hover/` (1-Servo + 2-Wheel Hover) · `boards/arm/` (Multi-Servo Arm) | Per-board C++ |
| **Platform** | WiFi · Bluetooth · SPI · I2C · I2S · UART · GPIO | ESP-IDF v5.5+ |

---

## 🚀 Quick Start

### Prerequisites

- ESP32-S3 board with GC9A01 240×240 LCD
- ESP-IDF v5.5.2+
- Python 3.8+ (for build scripts)

### Build

```bash
# Clone and enter
git clone git@github.com:Xgorobot/RIG-Omni.git
cd RIG-Omni

# Source ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Select board type and firmware region (interactive menu)
idf.py set-target esp32s3
idf.py menuconfig
# → RIG-Omni → Board Type → Puppy / Hover / ARM
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas

# Build & Flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Production Release

```bash
# Build a release package (firmware + assets + manifest)
python tools/gen_bin_package.py
# Output: bin/rig-puppy.bin (or rig-hover.bin)
```

---

## 📂 Project Structure

```
RIG-Omni/
├── main/                    # Firmware source
│   ├── audio/               # Audio codec, wake word, processors
│   ├── display/             # LCD driver, EAF emote engine, LVGL
│   ├── led/                 # LED strip & GPIO LED control
│   ├── protocols/           # MQTT & WebSocket communication
│   ├── boards/              # Hardware abstraction layer
│   │   ├── common/          # Shared drivers (IMU, button, BLE, camera…)
│   │   ├── puppy/           # Puppy robot (5-servo dog)
│   │   ├── hover/           # Hover robot (2-wheel hovercraft)
│   │   └── arm/             # ARM robot (multi-servo robotic arm)
│   ├── assets/              # Language packs, fonts
│   ├── application.cc/h     # Application lifecycle
│   ├── mcp_server.cc/h      # MCP remote control server
│   ├── ota.cc/h             # OTA firmware update
│   └── settings.cc/h        # Device settings (NVS)
├── partitions/              # Flash partition table
│   └── 16m.csv              # 16MB single partition scheme
├── tools/                   # Build & utility scripts
│   ├── gen_lang.py          # Language config generation
│   ├── build_default_assets.py  # Default asset builder
│   └── spiffs_assets/       # SPIFFS asset packer
├── CMakeLists.txt           # Root CMake (ESP-IDF project)
├── sdkconfig.defaults       # Default Kconfig settings
└── README.md
```

---

## 🤖 Multi-Board Configuration

RIG-Omni uses Kconfig to select the target robot form at build time:

```bash
idf.py menuconfig
# RIG-Omni Configuration → Board Type
```

| Board | Motors | Motion | Key Files |
| --- | --- | --- | --- |
| **RIG-Puppy** | 5 servos | Dog gait, head tracking, actions | `boards/puppy/puppy_board.cc` |
| **RIG-Hover** | 1 servo + 2 FOC motors | Balance, wheel drive, differential | `boards/hover/hover_board.cc` |
| **RIG-ARM** | Multi-servo (AX-12A) | Robotic arm, calibration, teach mode | `boards/arm/arm_board.cc` |

Each board has its own:
- Motor control logic (`xgo.cc/h`, `xgo_action.cc/h`)
- EAF emotion assets (`emoji/`, `240_240/`)
- Wake word model (`wakenet/`)
- Debug tools (`hover_debug_server.cc/h` for Hover)

All hardware drivers (IMU, Bluetooth, buttons, camera, battery) live in `boards/common/` and are shared across all forms.

### Firmware Region (Domestic / Overseas)

RIG-Omni supports building both domestic (China) and overseas firmware from a single codebase. Select the region in menuconfig:

```bash
idf.py menuconfig
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas
```

| Configuration | Domestic (China) | Overseas |
| --- | --- | --- |
| OTA URL | `xl-api.xgorobot.com` | `xl-api.luwudynamics.ai` |
| Default Language | zh_CN | en_US |
| Wake Word | Custom (e.g. 小陆同学) | Custom + Hey Kira |
| WiFi Config Animation | Domestic QR code | Overseas QR code |

The region selection automatically configures:
- **OTA URL** — different server endpoints for firmware updates and server address discovery
- **Default Language** — `zh_CN` for domestic, `en_US` for overseas (users can still switch via MCP)
- **Wake Word** — overseas auto-enables ESP-SR built-in "Hey Kira" alongside the custom wake word
- **WiFi Config EAF** — different `wificonfig.eaf` animation (with region-specific QR code) is selected at build time

The `wificonfig.eaf` file is auto-generated during build. Each board keeps two source files:
```
main/boards/<board>/emoji/
    wificonfig_domestic.eaf    # Domestic QR code
    wificonfig_overseas.eaf    # Overseas QR code
    wificonfig.eaf             # Auto-generated (gitignored)
```

---

## 🛠️ Development

### Adding a New Board

```bash
# 1. Create board directory
mkdir -p main/boards/myrobot/240_240
mkdir -p main/boards/myrobot/emoji

# 2. Add board.cc + xgo.cc with your motor logic
# 3. Register in Kconfig (main/Kconfig.projbuild)
# 4. Add to CMakeLists.txt build config
```

### MCP Tools

Extend robot capabilities via the MCP framework. Example tool definition:

```cpp
mcp_server.AddTool("self.robot.move",
    "Move forward/backward in cm",
    PropertyList({Property("distance", kPropertyTypeInteger, -20, 20)}),
    [this](const PropertyList& props) -> ReturnValue {
        int distance = props["distance"].value<int>();
        // Execute movement...
        return true;
    });
```

### Debug Monitor

```bash
idf.py monitor
# Press Ctrl+] to exit
```

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

---

## 📜 License

This project is licensed under the Apache License, Version 2.0.

Copyright © 2024–2026 RIG-Omni Contributors

---

Built with ❤️ by the RIG-Omni Team
