#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 初始化并启动 BLE 广播（UUID 与 XGO 一致，广播名 LULU-XXXX）
void lulu_ble_init();

// 停止 BLE 广播
void lulu_ble_deinit();

// 检查 BLE 广播是否运行中
bool lulu_ble_is_running();

// BLE 发送通知（设备 -> APP）
void lulu_ble_send(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
