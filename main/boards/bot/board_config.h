#ifndef _BOT_BOARD_CONFIG_H_
#define _BOT_BOARD_CONFIG_H_

// ===================================================================
// RIG-Bot 板型专属配置
// ===================================================================

// XGO / Motor UART Config
#define XGO_UART_TX_PIN GPIO_NUM_46
#define XGO_UART_RX_PIN GPIO_NUM_38

// Laser Control Pin
#define LASER_GPIO GPIO_NUM_3

#define TOUCH_BUTTON_GPIO GPIO_NUM_3

// IMU I2C Config
#define IMU_I2C_SDA GPIO_NUM_14
#define IMU_I2C_SCL GPIO_NUM_48

// XGO Task Config
#define XGO_TASK_INTERVAL_MS     2   // xgo_task 循环间隔（毫秒）
#define XGO_RX_TASK_INTERVAL_MS  20  // xgo_rx_task 循环间隔（毫秒）

// Display 方向配置
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true

#endif // _BOT_BOARD_CONFIG_H_
