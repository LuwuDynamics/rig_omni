#ifndef _HOVER_BOARD_CONFIG_H_
#define _HOVER_BOARD_CONFIG_H_

// ===================================================================
// Hover (RIG-Hover, 1-servo + 2-wheel hovercraft) 板型专属配置
// ===================================================================

// Motor UART Config
#define MOTOR_UART_TX_PIN GPIO_NUM_46
#define MOTOR_UART_RX_PIN GPIO_NUM_38

// Laser Control Pin
#define LASER_GPIO GPIO_NUM_46

// Touch Button Pin (GPIO 3 freed up from UART TX which moved to GPIO 46)
#define TOUCH_BUTTON_GPIO GPIO_NUM_3

// IMU I2C Config
#define IMU_I2C_SDA GPIO_NUM_14
#define IMU_I2C_SCL GPIO_NUM_48

// Robot / Motor Task Config
#define ROBOT_TASK_INTERVAL_MS     4    // robot_task 循环间隔（毫秒），控制舵机指令发送频率
#define MOTOR_RX_TASK_INTERVAL_MS  5    // motor_rx_task 循环间隔（毫秒），控制舵机反馈和 IMU 读取频率

// Display 方向配置
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true

#endif // _HOVER_BOARD_CONFIG_H_
