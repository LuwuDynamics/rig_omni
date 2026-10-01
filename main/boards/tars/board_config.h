#ifndef _TARS_BOARD_CONFIG_H_
#define _TARS_BOARD_CONFIG_H_

// ===================================================================
// RIG-Tars 板型专属配置
// ===================================================================

// Motor UART Config
#define MOTOR_UART_TX_PIN GPIO_NUM_46
#define MOTOR_UART_RX_PIN GPIO_NUM_38

// The external touch module is a regular digital-output sensor (not the ESP
// capacitive-touch peripheral).  This enclosure uses the same active-low
// digital sensor wiring as the hall-power design.
#define TOUCH_BUTTON_GPIO GPIO_NUM_3
#define TOUCH_BUTTON_ACTIVE_HIGH false

// IMU I2C Config
#define IMU_I2C_SDA GPIO_NUM_14
#define IMU_I2C_SCL GPIO_NUM_48

// Robot / Motor Task Config
#define ROBOT_TASK_INTERVAL_MS     2   // robot_task 循环间隔（毫秒）
#define MOTOR_RX_TASK_INTERVAL_MS  20  // motor_rx_task 循环间隔（毫秒）

// Display 方向配置
//
// The TARS enclosure uses a landscape 320x240 ST7789V2 panel.  These values
// deliberately live in the TARS board config rather than boards/common: the
// other robots still use the GC9A01 240x240 round panel.
#define TARS_DISPLAY_WIDTH 320
#define TARS_DISPLAY_HEIGHT 240
// The TARS console renderer flushes 16 scan lines at a time.  Keeping the
// SPI DMA transfer limit to this real flush size prevents ESP-IDF from
// reserving a full-screen (153.6 KB) internal DMA bounce buffer.
#define TARS_DISPLAY_DRAW_BUFFER_LINES 16
#define TARS_DISPLAY_SPI_TRANS_QUEUE_DEPTH 2
#define TARS_DISPLAY_PCLK_HZ (40 * 1000 * 1000)
// Measured from the physical 320x240 panel: it needs SwapXY to map the
// logical 320-pixel axis to the long edge.
#define TARS_DISPLAY_MIRROR_X false
// Both axes are mirrored so that the mounted landscape screen is rotated 180°.
#define TARS_DISPLAY_MIRROR_Y true
#define TARS_DISPLAY_SWAP_XY true
// Unlike the legacy GC9A01, this ST7789V2 panel expects RGB byte ordering.
#define TARS_DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
// Kept from the original TARS panel configuration.  This particular module
// needs the ST7789 display-inversion command for the expected dark console.
#define TARS_DISPLAY_INVERT_COLOR true

// Camera 方向配置
#define CAMERA_ROTATION 0

#endif // _TARS_BOARD_CONFIG_H_
