#include "imu.h"
#include "config.h"
#include "board_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cmath>

#define TAG "QMI8658C"

// 与 QMI8658C.cpp / 手册一致的寄存器与器件地址（QMI8658C.h 等价定义）
#define QMI8658C_I2C_ADDRESS_ALT 0x6A
#define QMI8658C_I2C_ADDRESS     0x6B
#define QMI8658C_REG_WHO_AM_I    0x00
#define QMI8658C_REG_CTRL1       0x02
#define QMI8658C_REG_CTRL2       0x03
#define QMI8658C_REG_CTRL3       0x04
#define QMI8658C_REG_CTRL5       0x06
#define QMI8658C_REG_CTRL7       0x08
#define QMI8658C_REG_ACC_X_L     0x35
#define QMI8658C_WHOAMI_EXPECTED 0x05

#define QMI8658C_CALIBRATION_COUNT 400

#define I2C_FREQ_HZ 400000

// ---------------------- 状态 ----------------------
static i2c_master_bus_handle_t bus_handle = nullptr;
static i2c_master_dev_handle_t dev_handle = nullptr;
static bool imu_initialized = false;
static uint8_t imu_i2c_addr = QMI8658C_I2C_ADDRESS;

// 导出的姿态数据（变量名保持与原先 imu.cc 一致）
float roll = 0.0f;
float pitch = 0.0f;
float pit_gyro = 0.0f;
float _pit_gyro = 0.0f;
float yaw = 0.0f;

float accel_x = 0.0f;
float accel_y = 0.0f;
float accel_z = 0.0f;
float gyro_x = 0.0f;
float gyro_y = 0.0f;
float gyro_z = 0.0f;
float dq = 0.0f;
bool imu_is_initialized() {
    return imu_initialized;
}

// 校准状态（对应 QMI8658C.cpp 中 qmi_countInit / isIMUInit / imu.*_bias）
static float acc_bias[3] = {0};
static float gyro_bias[3] = {0};
static uint16_t qmi_countInit = 0;


// 与 QMI8658C.cpp 文件内变量对齐（本文件内 static，避免与工程其它翻译单元冲突）
static double qmi_d_imu_time = 0;
static long qmi_last_imu_time = 0;
static float pit_g = 0.0f;
static float gyro_yaw = 0.0f;


// ---------------------- 内部函数 ----------------------
static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    if (!dev_handle) return ESP_ERR_INVALID_STATE;
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(dev_handle, data, 2, pdMS_TO_TICKS(50));
}

static esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len) {
    if (!dev_handle) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(dev_handle, &reg, 1, buf, len, pdMS_TO_TICKS(50));
}

static void calibration_bias_qmi8658c(void) {
    if (qmi_countInit == 0) {
        for (int i = 0; i < 3; i++) {
            acc_bias[i] = 0;
            gyro_bias[i] = 0;
        }
    }
    const uint64_t ms_since_boot = esp_timer_get_time() / 1000;
    if (ms_since_boot > 4000 && isIMUInit == 0) {
        if (qmi_countInit <= QMI8658C_CALIBRATION_COUNT) {
            acc_bias[0] += accel_x;
            acc_bias[1] += accel_y;
            acc_bias[2] += accel_z;
            gyro_bias[0] += gyro_x;
            gyro_bias[1] += gyro_y;
            gyro_bias[2] += gyro_z;
            qmi_countInit++;
        } else {
            for (int i = 0; i < 3; i++) {
                acc_bias[i] /= (float)QMI8658C_CALIBRATION_COUNT;
                gyro_bias[i] /= (float)QMI8658C_CALIBRATION_COUNT;
            }
            isIMUInit = 1;
        }
    }
}

// ---------------------- 初始化 ----------------------
void imu_init() {
    if (imu_initialized) {
        ESP_LOGW(TAG, "IMU already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing QMI8658C on SDA=%d, SCL=%d", IMU_I2C_SDA, IMU_I2C_SCL);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = IMU_I2C_SDA,
        .scl_io_num = IMU_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true},
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    imu_i2c_addr = QMI8658C_I2C_ADDRESS;
    if (i2c_master_probe(bus_handle, imu_i2c_addr, pdMS_TO_TICKS(200)) != ESP_OK) {
        imu_i2c_addr = QMI8658C_I2C_ADDRESS_ALT;
        if (i2c_master_probe(bus_handle, imu_i2c_addr, pdMS_TO_TICKS(200)) != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658C not found at 0x%02X / 0x%02X", QMI8658C_I2C_ADDRESS,
                     QMI8658C_I2C_ADDRESS_ALT);
            i2c_del_master_bus(bus_handle);
            bus_handle = nullptr;
            return;
        }
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = imu_i2c_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(bus_handle);
        bus_handle = nullptr;
        return;
    }

    /* 基本配置，参考 QMI8658C.cpp */
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t id = 0;
    ret = read_regs(QMI8658C_REG_WHO_AM_I, &id, 1);
    if (ret != ESP_OK || id != QMI8658C_WHOAMI_EXPECTED) {
        ESP_LOGE(TAG, "WHO_AM_I=0x%02X (expected 0x%02X)", id, QMI8658C_WHOAMI_EXPECTED);
        i2c_master_bus_rm_device(dev_handle);
        i2c_del_master_bus(bus_handle);
        dev_handle = nullptr;
        bus_handle = nullptr;
        return;
    }

    write_reg(QMI8658C_REG_CTRL1, 0x40);
    write_reg(QMI8658C_REG_CTRL7, 0x03);
    write_reg(QMI8658C_REG_CTRL2, 0x04);
    write_reg(QMI8658C_REG_CTRL3, 0x64);
    write_reg(QMI8658C_REG_CTRL5, 0x00);

    vTaskDelay(pdMS_TO_TICKS(50));

    qmi_last_imu_time = esp_timer_get_time();
    imu_initialized = true;
    ESP_LOGI(TAG, "QMI8658C initialized at 0x%02X", imu_i2c_addr);
}

// ---------------------- 读取数据 ----------------------
float d_time = 0.0f;
long last_time = 0;

void imu_read_once() {
    if (!imu_initialized || !dev_handle) {
        return;
    }

    uint8_t data[12] = {0};
    if (read_regs(QMI8658C_REG_ACC_X_L, data, 12) != ESP_OK) {
        return;
    }

    int16_t accx = (int16_t)(data[1] << 8 | data[0]);
    int16_t accy = (int16_t)(data[3] << 8 | data[2]);
    int16_t accz = (int16_t)(data[5] << 8 | data[4]);
    int16_t gyrox = (int16_t)(data[7] << 8 | data[6]);
    int16_t gyroy = (int16_t)(data[9] << 8 | data[8]);
    int16_t gyroz = (int16_t)(data[11] << 8 | data[10]);

    /* 与 QMI8658C.cpp 相同的量程与轴映射，写入原 imu.cc 变量名 */
    accel_x = -(float)accx / 16384.0f * 9.8f;
    accel_y = -(float)accy / 16384.0f * 9.8f;
    accel_z = (float)accz / 16384.0f * 9.8f;

    gyro_y = (float)gyrox / 32768.0f * 1024.0f;
    gyro_x = (float)gyroy / 32768.0f * 1024.0f;
    gyro_z = (float)gyroz / 32768.0f * 1024.0f;

    calibration_bias_qmi8658c();

    if (!isIMUInit) {
        return;
    }

    // gyro_x -= gyro_bias[0];
    // gyro_y -= gyro_bias[1];
    // gyro_z -= gyro_bias[2];

    long current_time = esp_timer_get_time();
    qmi_d_imu_time = (current_time - qmi_last_imu_time) * 0.000001;
    d_time = (float)qmi_d_imu_time;
    last_time = current_time;

    float pit_acc = 0.0f;
    pit_gyro = -(cos((q_head + 90.0) / 180.0 * PI) * gyro_x +
                 sin((q_head + 90.0) / 180.0 * PI) * gyro_z);
    pit_acc = atan((sin((q_head + 90.0) / 180.0 * PI) * accel_y +
                    cos((q_head + 90.0) / 180.0 * PI) * accel_z) /
                   accel_x) *
              57.2958f;
    roll = atan((sin((q_head + 90.0) / 180.0 * PI) * accel_z +
                 cos((q_head + 90.0) / 180.0 * PI) * accel_y) /
                accel_x) *
           57.2958f;

    dq = 0.6f * dq + 0.4f * pit_gyro;
    gyro_yaw = 0.5f * gyro_yaw + 0.5f * gyro_y;

    float pit_freq = 1.0f / (fabsf(pit_gyro) + 200.0f);
    if (fabsf(pit_acc) < 100.0f && fabsf(pit_gyro) < 1000.0f) {
        pitch = pit_freq * pit_acc + (1.0f - pit_freq) * (pitch + pit_gyro * (float)qmi_d_imu_time);
    }
    pit_g = pit_g + pit_gyro * (float)qmi_d_imu_time;
    qmi_last_imu_time = current_time;
    yaw = yaw - gyro_yaw * (float)qmi_d_imu_time;
}

// ---------------------- 清理 ----------------------

void imu_deinit() {
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = nullptr;
    }
    if (bus_handle) {
        i2c_del_master_bus(bus_handle);
        bus_handle = nullptr;
    }
    imu_initialized = false;
    isIMUInit = 0;
    qmi_countInit = 0;
    ESP_LOGI(TAG, "IMU deinitialized");
}
