# RIG-Omni

An Open-Source ESP32-S3 Firmware for Multipurpose Intelligent Robots

ESP32-S3 · Voice AI · Robot Motion · EAF Animation · TARS Console · MCP Tools

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
- [TARS Console and Controls](#tars-console-and-controls)
- [Development](#-development)
- [Contributing](#-contributing)
- [License](#-license)

---

## 📖 Overview

RIG-Omni is open-source ESP32-S3 firmware for four robot forms: **Puppy**, a five-servo robot dog; **Hover**, a two-wheel balancing robot; **ARM**, a robotic arm; and **TARS**, an *Interstellar*-inspired walking robot with a landscape console display.

The robots share voice interaction, networking, sensor drivers, and MCP tools, while each board provides its own motion control and interface. Puppy, Hover, and ARM use a 240×240 round display; TARS uses a 320×240 landscape display for logs and telemetry. Wake-word detection runs locally; conversational recognition and speech generation use network services.

> Mission: Give every robot builder an intuitive, modular, and delightful firmware experience.

---

## ✨ Features

| Category | Capability |
| --- | --- |
| 🧠 Voice AI | Offline wake word, cloud ASR/TTS, VAD, AGC |
| 🎭 Display | EAF expressions on round displays; a dedicated dual-column TARS console |
| 🤖 Motion Control | Servo gait, wheel balance, Hover gimbal mode, ARM calibration and teach mode |
| 📡 Connectivity | BluFi WiFi provisioning, OTA firmware update, HMAC device activation |
| 🔧 MCP Tools | Voice-callable movement, calibration, status, and TARS personality controls |
| 📷 Camera | Snapshot tools on camera-equipped hardware |
| 🎮 Remote Control | BLE control compatible with the robot remote app / mini program |
| 🖥️ Debug | Real-time hover debug web server for motor tuning |
| 📦 Build Tools | Per-board versions, regional asset packaging, and TARS voice generation |

---

## 🔧 Hardware

| # | Component | Interface | Details |
| --- | --- | --- | --- |
| 1 | GC9A01 round LCD / ST7789V2 landscape LCD | SPI | 240×240 for Puppy/Hover/ARM; 320×240 for TARS |
| 2 | IMU (QMI8658C) | I2C | 6-axis attitude + balance control |
| 3 | Serial servos | UART | EM3 for Puppy/Hover; SCS009 for ARM/TARS |
| 4 | KP4012 wheel motors (Hover: 2) | UART | Balance, differential drive, and gimbal control |
| 5 | I2S Audio (Direct) | I2S | Simplex/Duplex mic + speaker (no hardware codec chip) |
| 6 | Camera (GC0308/OV2640) | DVP | Snapshot via MCP tools |
| 7 | Boot button + external touch module | GPIO | Functions depend on the board; TARS uses an active-low digital touch input on GPIO3 |

> Pin assignments and display orientation are defined in `main/boards/common/config.h` and each board's `board_config.h`. Use the configuration for your hardware revision.

---

## 🏗️ Architecture

| Layer | Components | Technology |
| --- | --- | --- |
| **Application** | Voice AI · Emote/TARS display · MCP server · Camera tools | C++ (ESP-IDF) |
| **Shared Drivers** | `main/boards/common/` — IMU · Button · BLE · Battery · Camera · Motor protocols | C++ shared drivers |
| **Robot Logic** | `main/boards/{puppy,hover,arm,tars}/` — board setup, robot control, actions, and motors | Per-board C++ |
| **Platform** | WiFi · Bluetooth · SPI · I2C · I2S · UART · GPIO | ESP-IDF 5.5.x |

---

## 🚀 Quick Start

### Prerequisites

- Matching ESP32-S3 robot hardware with 16 MB flash, PSRAM, and the board-specific display
- ESP-IDF 5.5.x; TARS builds have been verified with 5.5.3
- Python 3.10+ for utility scripts; use the Python environment installed by ESP-IDF for firmware builds
- FFmpeg if generating or resampling voice assets

### Build

```bash
# Clone and enter
git clone https://github.com/LuwuDynamics/rig_omni.git RIG-Omni
cd RIG-Omni

# Source ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Select board type and firmware region (interactive menu)
idf.py set-target esp32s3
idf.py menuconfig
# → RIG-Omni → Board Type → RIG-Puppy / RIG-Hover / RIG-Arm / RIG-Tars
# → RIG-Omni → Firmware Region → Domestic (China) / Overseas
# → RIG-Omni → Select display style → Emote animation style
# → RIG-Omni → Flash Assets → Flash Emote Assets

# Build & Flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your serial port (for example, `/dev/cu.usbmodem…` on macOS). Select the firmware region before building so the provisioning QR code matches the network service you intend to use.

`idf.py build` builds the application and selected board assets. `idf.py flash` writes the configured images. For a fresh device or a board/region change, flash the matching application and assets together. Changing a board-specific `.ogg` updates the application binary; changing EAF animations or packaged wake-word models updates the assets image.

Versions are maintained in `main/boards/<board>/version.txt`. Check `build/project_description.json` for the version used by a particular build.

### Production Release

```bash
# Inspect the packaging selection without building
python3 tools/package_luwu_firmware.py --board tars --dry-run

# Build domestic, overseas, and hardware-test packages
python3 tools/package_luwu_firmware.py --board tars
# Multiple boards: --board puppy,hover,arm,tars
# Interactive selection: omit --board
```

The packaging script also builds the separate hardware-test project, which must be present as `Test_Firmware/` or `test_firmware/`. If you only need the robot firmware, use the `idf.py` workflow above.

Packages are written to `releases/Luwu-Tools/bin/`: `RIG/<board>/` for domestic firmware, `RIG/<board>/gb/` for overseas firmware, and `GARAGE/<board>/` for hardware tests. Regional builds use isolated directories under `build/packages/`; logs are saved in `releases/logs/`. Each main-firmware package includes full, application-only, and assets-only manifests.

TARS regional builds share the same dedicated English voice prompts. Region selection controls network endpoints, the default interface language, and the provisioning QR code.

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
│   │   │   └── motors/     # EM3, SCS009, KP4012, shared SCS bus
│   │   ├── puppy/           # Puppy robot (5-servo dog)
│   │   ├── hover/           # Two-wheel balance and gimbal modes
│   │   ├── arm/             # Five-servo robotic arm
│   │   └── tars/            # Three-servo robot, console, voice assets
│   ├── assets/              # Language packs, fonts
│   ├── application.cc/h     # Application lifecycle
│   ├── mcp_server.cc/h      # MCP remote control server
│   ├── ota.cc/h             # OTA firmware update
│   └── settings.cc/h        # Device settings (NVS)
├── partitions/              # Flash partition table
│   └── 16m.csv              # 16 MB flash: two OTA slots + assets
├── tools/                   # Build & utility scripts
│   ├── gen_lang.py          # Language config generation
│   ├── package_luwu_firmware.py  # Multi-board regional release packages
│   ├── generate_tars_voice_pack.py  # Volcengine voice generation
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
| **RIG-Puppy** | 5 EM3 servos | Dog gait, head tracking, actions | `main/boards/puppy/puppy_board.cc` |
| **RIG-Hover** | 1 EM3 servo + 2 KP4012 wheels | Balance, differential drive, gimbal mode | `main/boards/hover/hover_board.cc` |
| **RIG-ARM** | 5 SCS009 servos | Robotic arm, calibration, teach mode | `main/boards/arm/arm_board.cc` |
| **RIG-TARS** | 3 SCS009 servos | Walking, turning, calibration, live telemetry | `main/boards/tars/tars_board.cc` |

Each board has its own:

- Board setup (`<board>_board.cc`) and pin definitions (`board_config.h`)
- Motion logic (`robot.cc/h`, `robot_action.cc/h`) and motor adapter (`motor.cc/h`)
- Display assets (`emoji/`, `240_240/`; TARS uses `320_240/`)
- Wake word model (`wakenet/`)
- Debug tools (`hover_debug_server.cc/h` for Hover)

Shared drivers live in `main/boards/common/`. The `motors/` subdirectory contains EM3, SCS009, KP4012, and SCS bus implementations; each board's motor adapter handles its own motor IDs and configuration.

### TARS Console and Controls

TARS uses a **320×240 ST7789V2 landscape LCD** with an *Interstellar*-inspired terminal layout:

- **Left:** dense blue, 6 px scrolling log text.
- **Right:** green, 8 px status text for the current conversation state, servo angles, IMU attitude, and battery telemetry, refreshed once per second.
- **Footer:** bold 12 px `HONESTY` and `HUMOR` percentages, separated from the logs by a horizontal line. Defaults are **90% honesty** and **75% humor**; the `self.tars.set_personality` MCP tool changes them through conversation.
- **Wi-Fi setup:** a region-specific QR animation temporarily replaces the console to keep the code unobstructed.

The external touch module uses **GPIO3, active low**. During calibration, a short touch confirms the current servo zero positions and exits calibration. During normal operation, hold for at least **3 seconds**, then release to enter deep sleep; a short touch wakes the robot. A short touch outside calibration has no assigned action.

TARS voice prompts live in `main/boards/tars/tars_*.ogg`. To generate selected replacements with your own Volcengine voice:

```bash
export VOLC_TTS_SPEAKER='your-speaker-id'
# Set VOLC_SPEECH_API_KEY in your local environment.
python3 tools/generate_tars_voice_pack.py --only message_send,over --dry-run
python3 tools/generate_tars_voice_pack.py --only message_send,over --overwrite
```

Generated audio is saved under `tools/tars_voice_output/` for review. After listening, replace the corresponding board-specific files and rebuild; the script does not install the audio into the firmware automatically. FFmpeg normalizes non-16 kHz output to Ogg Opus with a 16 kHz input-rate header. `ffprobe` may report 48 kHz for an Opus stream because that is its decoding rate.

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
| Wake Word | Board models and active ESP-SR settings | Board models + Hey Kira |
| WiFi Config Animation | Domestic QR code | Overseas QR code |

The region selection automatically configures:

- **OTA URL** — different server endpoints for firmware updates and server address discovery
- **Default Language** — `zh_CN` for domestic, `en_US` for overseas (users can still switch via MCP)
- **Wake Word** — overseas enables ESP-SR's built-in "Hey Kira"; packaged models also depend on the board and active ESP-SR configuration
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

# 2. Add board setup, robot.cc, robot_action.cc, and motor.cc
# 3. Register in Kconfig (main/Kconfig.projbuild)
# 4. Add to CMakeLists.txt build config
```

Use `main/boards/common/motors/` for shared motor protocol code and keep board-specific pin assignments in `board_config.h`. After adding or removing audio files, run `idf.py reconfigure` before rebuilding so the embedded resource list is refreshed.

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
