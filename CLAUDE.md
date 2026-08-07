# CLAUDE.md — RIG-Omni Project Constitution

> **本文件是 AI 编码助手的行为准则，每次对话开始时自动加载。违反这些规则的改动将在 Code Review 中被拒绝。**

---

## 项目定位

RIG-Omni 是一个**单代码库、三产品**的 ESP32-S3 嵌入式固件项目。三个产品共用同一块开发板和同一套驱动框架，仅在上层机器人逻辑上不同。

| 产品 | 板型宏 | 形态 | 电机配置 |
|------|--------|------|----------|
| **RIG-Puppy** | `CONFIG_BOARD_TYPE_PUPPY` | 5 舵机机器狗 | 5 × 串行舵机 |
| **RIG-Arm** | `CONFIG_BOARD_TYPE_ARM` | 5 舵机机械臂 | 5 × 串行舵机 |
| **RIG-Hover** | `CONFIG_BOARD_TYPE_HOVER` | 1 舵机 + 双轮气垫船 | 1 × 舵机 + 2 × DC 电机 |

---

## 代码分层（必须理解）

```
┌─────────────────────────────────────────────┐
│ 应用层 (main/*.cc)                          │
│ application · mcp_server · ota · settings   │
├─────────────────────────────────────────────┤
│ 共享子系统                                   │
│ audio/ · display/ · led/ · protocols/       │
├─────────────────────────────────────────────┤
│ 板型抽象层                                   │
│ boards/common/    ← 三个产品共享            │
│ boards/arm/       ← Arm 专属                │
│ boards/puppy/     ← Puppy 专属              │
│ boards/hover/     ← Hover 专属              │
├─────────────────────────────────────────────┤
│ ESP-IDF 框架                                │
│ WiFi · BLE · SPI · I2C · I2S · UART · GPIO  │
└─────────────────────────────────────────────┘
```

### 关键 include 机制

共享代码通过编译时 include path 引用板级头文件：

```cmake
# CMakeLists.txt 根据板型设置 include path
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR})
```

这意味着 `boards/common/imu.h` 中的 `#include "xgo.h"` 会**根据编译配置解析到当前板型的 xgo.h**。修改共享代码时，务必确认三个板型都能正确解析这个 include。

---

## 🚫 硬性规则

### 规则 1：板级隔离

**严禁在一次改动中同时修改两个及以上板型的专属文件。**

```
❌ 错误：同时改了 boards/arm/xgo.cc 和 boards/puppy/xgo.cc
✅ 正确：只改 boards/puppy/xgo.cc
```

如果确实需要改多个板子（如新增同一个 MCP 工具），请分别在独立的 commit 中提交。

### 规则 2：共享代码修改必须考虑三向兼容

修改 `main/boards/common/`、`main/audio/`、`main/display/`、`main/led/`、`main/protocols/` 或 `main/` 根目录下的任何文件之前，**必须在脑海中（或在 commit message 中）确认**：

1. 这个改动对 Puppy 有什么影响？
2. 这个改动对 Arm 有什么影响？
3. 这个改动对 Hover 有什么影响？

如果某个改动只对一个板子有益但对其他板子有破坏风险，**优先用条件编译**而不是直接改共享代码：

```cpp
// ✅ 正确：条件编译隔离板级差异
#if defined(CONFIG_BOARD_TYPE_HOVER)
    // Hover 专用逻辑
#endif
```

### 规则 3：GPIO 配置只在 board_config.h 中修改

每个板子的引脚定义在各自的 `board_config.h` 中：

| 文件 | 管理范围 |
|------|----------|
| [main/boards/puppy/board_config.h](main/boards/puppy/board_config.h) | Puppy 的 UART/IMU/Laser 引脚 |
| [main/boards/arm/board_config.h](main/boards/arm/board_config.h) | Arm 的 UART/IMU/Laser 引脚 |
| [main/boards/hover/board_config.h](main/boards/hover/board_config.h) | Hover 的 UART/IMU/Laser 引脚 |

**修改 GPIO 时，只改当前板子的 board_config.h。不要改其他板子的。**

### 规则 4：新增板级功能放在正确的位置

| 改动类型 | 应放置位置 |
|----------|------------|
| 新增舵机动作 | `boards/<板子>/xgo_action.cc` |
| 新增 MCP 工具（板级专属） | `boards/<板子>/<板子>_board.cc` |
| 新增 MCP 工具（通用） | `mcp_server.cc` |
| 修改 IMU/按键/电池/摄像头驱动 | `boards/common/` |
| 修改音频/显示/LED/协议 | 对应的共享子系统目录 |
| 新增 EAF 表情动画 | `boards/<板子>/emoji/` |
| 修改唤醒词模型 | `boards/<板子>/wakenet/` |

### 规则 5：Kconfig 修改需同步三向

[main/Kconfig.projbuild](main/Kconfig.projbuild) 中的 `depends on` 子句必须覆盖 `BOARD_TYPE_PUPPY || BOARD_TYPE_HOVER || BOARD_TYPE_ARM`。新增功能如果不适用于某个板子，必须明确排除。

### 规则 6：CI 构建必须三向通过

修改推送后，确保以下三个目标都能编译成功：

```bash
idf.py set-target esp32s3
# 分别以 CONFIG_BOARD_TYPE_PUPPY / _HOVER / _ARM 构建
```

---

## 编译时选择板型

本项目通过 Kconfig 的 `BOARD_TYPE` 选项在编译时选择目标产品：

```bash
idf.py menuconfig
# 导航到: RIG-Omni → Board Type → 选择 RIG-Puppy / RIG-Hover / RIG-Arm
```

**注意**：每次切换 `BOARD_TYPE` 后需要 `idf.py fullclean`，否则可能链接到错误的板级文件。

---

## 板级差异速查

| 差异点 | Puppy | Arm | Hover |
|--------|-------|-----|-------|
| 舵机数量 | 5 | 5 | 1 |
| 电机 | 无 | 无 | 2 × DC 无刷 |
| 舵机指令间隔 | 4ms | 2ms | 4ms |
| 反馈读取间隔 | 20ms | 20ms | 5ms |
| UART TX | GPIO 3 | GPIO 46 | GPIO 46 |
| UART RX | GPIO 38 | GPIO 38 | GPIO 38 |
| Laser GPIO | GPIO 46 | GPIO 3 | GPIO 46 |
| 触摸按键 | 无 | GPIO 3 | GPIO 3 |
| 显示镜像X | false | false | true |
| 显示镜像Y | true | true | false |
| BLE 遥控 | ✅ | ✅ | ❌（使用其他方式）|
| 专属音效 | woof.ogg | 无 | engine_startup / engine_throttle |
| 调试工具 | 无 | 无 | hover_debug_server |
