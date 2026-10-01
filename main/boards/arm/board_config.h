#ifndef _PUPPY_BOARD_CONFIG_H_
#define _PUPPY_BOARD_CONFIG_H_

// ===================================================================
// Arm (RIG-Arm, 5-servo robot dog) 板型专属配置
// ===================================================================

// Motor UART Config
#define MOTOR_UART_TX_PIN GPIO_NUM_46
#define MOTOR_UART_RX_PIN GPIO_NUM_38

// Laser Control Pin
#define LASER_GPIO GPIO_NUM_3

#define TOUCH_BUTTON_GPIO GPIO_NUM_3

// IMU I2C Config
#define IMU_I2C_SDA GPIO_NUM_14
#define IMU_I2C_SCL GPIO_NUM_48

// Robot / Motor Task Config
#define ROBOT_TASK_INTERVAL_MS     2    // robot_task 循环间隔（毫秒），控制舵机指令发送频率
#define MOTOR_RX_TASK_INTERVAL_MS  20   // motor_rx_task 循环间隔（毫秒），控制舵机反馈和 IMU 读取频率

// Display 方向配置
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true

#endif // _PUPPY_BOARD_CONFIG_H_
