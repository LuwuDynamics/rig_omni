#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并启动 BLE 遥控模式
 * 蓝牙名称与 BluFi 配网保持一致（RIG-PuppyXXXX）
 * 使用与 XGO APP 兼容的 GATT 服务
 * 
 * @return true 启动成功
 * @return false 启动失败（可能是蓝牙资源已被占用）
 */
bool ble_remote_init();

/**
 * @brief 停止 BLE 遥控模式
 */
void ble_remote_deinit();

/**
 * @brief 检查 BLE 遥控模式是否运行中
 */
bool ble_remote_is_running();

/**
 * @brief BLE 发送数据通知（设备 -> APP）
 */
void ble_remote_send(const uint8_t* data, size_t len);

/**
 * @brief 处理从 BLE 接收的数据（APP -> 设备）
 * 内部调用，解析 XGO 协议格式
 */
void ble_remote_on_rx(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
