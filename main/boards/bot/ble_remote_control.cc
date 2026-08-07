/**
 * BLE 遥控控制模块
 * 提供与小程序/APP的蓝牙遥控通信
 * 协议兼容 XGO APP
 */

#include "sdkconfig.h"

#ifdef CONFIG_BT_NIMBLE_ENABLED

#include "ble_remote_control.h"

#include <cstring>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_bt.h>

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "xgo.h"
#include "xgo_action.h"

static const char* TAG = "BleRemote";

// 蓝牙名称前缀（与 BluFi 保持一致）
#define BLE_DEVICE_NAME_PREFIX "RIG-Arm"

// GATT 服务 UUID（与 XGO 兼容）
// Service: 0xFFF0, RX(Notify): 0xFFF1, TX(Write): 0xFFF2
static const ble_uuid16_t REMOTE_SERVICE_UUID = BLE_UUID16_INIT(0xFFF0);
static const ble_uuid16_t REMOTE_CHAR_RX_UUID = BLE_UUID16_INIT(0xFFF1);  // 设备 -> APP (Notify)
static const ble_uuid16_t REMOTE_CHAR_TX_UUID = BLE_UUID16_INIT(0xFFF2);  // APP -> 设备 (Write)

// 状态变量
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_char_rx_handle = 0;
static uint16_t g_char_tx_handle = 0;
static bool g_ble_running = false;
static bool g_ble_initialized = false;
static char g_device_name[24] = "RIG-Arm0000";

// 前向声明
static void ble_remote_advertise();
static void ble_remote_on_sync();
static void ble_remote_on_reset(int reason);
static void ble_remote_host_task(void* param);

/**
 * @brief 从协议值范围转换到实际值范围
 * XGO 协议: 0x00-0xFF 对应 min-max
 */
static int from_order_range(uint8_t value, int min_val, int max_val) {
    return min_val + (max_val - min_val) * value / 255;
}

/**
 * @brief GATT 特征访问回调
 */
static int ble_remote_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (attr_handle == g_char_tx_handle && ctxt->om != NULL) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > 0 && len <= 256) {
                uint8_t buf[256];
                os_mbuf_copydata(ctxt->om, 0, len, buf);
                ble_remote_on_rx(buf, len);
            }
        }
    }
    return 0;
}

// GATT 服务定义
static const struct ble_gatt_svc_def g_gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &REMOTE_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // RX Characteristic (Notify/Read): 设备 -> APP
                .uuid = &REMOTE_CHAR_RX_UUID.u,
                .access_cb = ble_remote_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = &g_char_rx_handle,
            },
            {
                // TX Characteristic (Write/Write_NR): APP -> 设备
                .uuid = &REMOTE_CHAR_TX_UUID.u,
                .access_cb = ble_remote_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &g_char_tx_handle,
            },
            {0}  // 结束标记
        },
    },
    {0}  // 结束标记
};

/**
 * @brief GAP 事件回调
 */
static int ble_remote_gap_event(struct ble_gap_event* event, void* arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "BLE connected, handle=%d", g_conn_handle);
            } else {
                g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                ESP_LOGW(TAG, "BLE connect failed, status=%d", event->connect.status);
                ble_remote_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            // 重新广播
            if (g_ble_running) {
                ble_remote_advertise();
            }
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete");
            if (g_ble_running) {
                ble_remote_advertise();
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: conn_handle=%d, mtu=%d",
                     event->mtu.conn_handle, event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

/**
 * @brief 启动 BLE 广播
 */
static void ble_remote_advertise() {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)g_device_name;
    fields.name_len = strlen(g_device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertisement fields: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                           ble_remote_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Started advertising as: %s", g_device_name);
}

/**
 * @brief NimBLE 主机同步回调
 */
static void ble_remote_on_sync() {
    ESP_LOGI(TAG, "BLE host synced");
    ble_remote_advertise();
}

/**
 * @brief NimBLE 主机重置回调
 */
static void ble_remote_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

/**
 * @brief NimBLE 主机任务
 */
static void ble_remote_host_task(void* param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ============ 公共接口实现 ============

extern "C" bool ble_remote_init() {
    if (g_ble_running) {
        ESP_LOGW(TAG, "BLE remote already running");
        return true;
    }

    // 生成设备名（与 BluFi 保持一致格式）
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(g_device_name, sizeof(g_device_name), "%s%02X%02X",
             BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);

    ESP_LOGI(TAG, "Initializing BLE remote control as: %s", g_device_name);

    int rc;

    // 检查蓝牙控制器状态来判断 NimBLE 是否已由 BluFi 初始化
    esp_bt_controller_status_t bt_status = esp_bt_controller_get_status();
    bool nimble_already_running = (bt_status == ESP_BT_CONTROLLER_STATUS_ENABLED);
    
    ESP_LOGI(TAG, "BT controller status: %d", bt_status);
    
    if (nimble_already_running) {
        // NimBLE 已经在运行（由 BluFi 初始化），复用它
        ESP_LOGI(TAG, "NimBLE already running, reusing existing stack");
        
        // 设置新的回调
        ble_hs_cfg.sync_cb = ble_remote_on_sync;
        ble_hs_cfg.reset_cb = ble_remote_on_reset;
        
        // 添加 BLE 遥控的 GATT 服务
        rc = ble_gatts_count_cfg(g_gatt_services);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
            return false;
        }

        rc = ble_gatts_add_svcs(g_gatt_services);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
            return false;
        }

        // 更新设备名并开始广播
        ble_svc_gap_device_name_set(g_device_name);
        ble_remote_advertise();
        
        g_ble_initialized = true;
    } else if (!g_ble_initialized) {
        // NimBLE 未运行，需要完整初始化
        ESP_LOGI(TAG, "Initializing NimBLE from scratch");
        
        rc = nimble_port_init();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
            return false;
        }

        // 配置 NimBLE host
        ble_hs_cfg.sync_cb = ble_remote_on_sync;
        ble_hs_cfg.reset_cb = ble_remote_on_reset;

        // 初始化 GATT 服务
        ble_svc_gap_init();
        ble_svc_gatt_init();

        rc = ble_gatts_count_cfg(g_gatt_services);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
            return false;
        }

        rc = ble_gatts_add_svcs(g_gatt_services);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
            return false;
        }

        // 设置设备名
        ble_svc_gap_device_name_set(g_device_name);

        // 启动 NimBLE host task
        nimble_port_freertos_init(ble_remote_host_task);

        g_ble_initialized = true;
    } else {
        // 已经初始化过，只需要重新广播
        ble_remote_advertise();
    }

    g_ble_running = true;
    ESP_LOGI(TAG, "BLE remote control initialized successfully");
    return true;
}

extern "C" void ble_remote_deinit() {
    if (!g_ble_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping BLE remote control");

    // 停止广播
    ble_gap_adv_stop();

    // 断开连接
    if (g_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(g_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }

    g_ble_running = false;
    ESP_LOGI(TAG, "BLE remote control stopped");
}

extern "C" bool ble_remote_is_running() {
    return g_ble_running;
}

extern "C" void ble_remote_send(const uint8_t* data, size_t len) {
    if (!g_ble_running || g_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    struct os_mbuf* om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for send");
        return;
    }

    int rc = ble_gatts_notify_custom(g_conn_handle, g_char_rx_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gatts_notify_custom failed: %d", rc);
    }
}

/**
 * @brief 处理接收到的 BLE 数据
 * 协议格式与 XGO 一致：55 00 LENGTH ORDER PAYLOAD... CHECKSUM 00 AA
 */
extern "C" void ble_remote_on_rx(const uint8_t* data, size_t len) {
    if (!data || len < 7) {
        return;
    }

    // 查找帧头 55 00
    size_t i = 0;
    while (i + 1 < len && !(data[i] == 0x55 && data[i + 1] == 0x00)) {
        ++i;
    }
    if (i + 1 >= len) {
        return;
    }

    const uint8_t* frame = &data[i];
    size_t remaining = len - i;
    if (remaining < 7) {
        return;
    }

    uint8_t length = frame[2];
    if (length > remaining) {
        return;  // 不完整帧
    }

    uint8_t order = frame[3];
    const uint8_t* payload = &frame[4];
    size_t payload_len = length - 7;
    if (4 + payload_len + 3 > remaining) {
        return;
    }

    uint8_t checksum = frame[4 + payload_len];
    uint8_t tail0 = frame[4 + payload_len + 1];
    uint8_t tail1 = frame[4 + payload_len + 2];

    if (tail0 != 0x00 || tail1 != 0xAA) {
        return;
    }

    // 校验和
    uint32_t sum = length + order;
    for (size_t j = 0; j < payload_len; ++j) {
        sum += payload[j];
    }
    sum &= 0xFF;
    if (checksum != (uint8_t)(0xFF - sum)) {
        return;
    }

    // 处理读命令 (ORDER_READ = 0x02)
    if (order == 0x02) {
        if (payload_len < 1) return;
        uint8_t addr = payload[0];
        ESP_LOGI(TAG, "Read command: addr=0x%02X", addr);

        uint8_t resp[32];
        size_t resp_len = 0;

        if (addr == 0x07) {
            // 版本号
            const char* version = "P-1.0.0";  // P for Arm
            size_t ver_len = strlen(version);

            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8 + ver_len;
            resp[3] = 0x12;  // READ_READBACK
            resp[4] = addr;
            memcpy(&resp[5], version, ver_len);

            uint32_t s = resp[2] + resp[3] + resp[4];
            for (size_t j = 0; j < ver_len; j++) s += resp[5 + j];
            resp[5 + ver_len] = (uint8_t)(0xFF - (s & 0xFF));
            resp[6 + ver_len] = 0x00;
            resp[7 + ver_len] = 0xAA;
            resp_len = 8 + ver_len;
        } else if (addr == 0x01) {
            // 电池电量
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 100;  // TODO: 获取实际电量
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        } else if (addr == 0x02) {
            // 工作状态：0x00 倒地, 0x01 正常
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 0x01;  // 正常状态
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        } else {
            // 其他地址返回 0
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 0x00;
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        }

        if (resp_len > 0) {
            ble_remote_send(resp, resp_len);
        }
        return;
    }

    // 处理写命令 (0x00/0x01)
    if (order != 0x00 && order != 0x01) {
        return;
    }

    if (payload_len < 2) {
        return;
    }

    uint8_t addr = payload[0];
    uint8_t value = payload[1];

    ESP_LOGI(TAG, "Write command: addr=0x%02X, value=0x%02X", addr, value);

    // 距上次收到 0x32（旋转指令）的超时自动归零
    // 小程序左右按按下发送 0x32，松手不发，必须靠超时来自动停转向
    static uint32_t vyaw_last_ms = 0;
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (addr != 0x32 && vyaw_last_ms != 0 && (now_ms - vyaw_last_ms) > 300) {
        vyaw = 0.0f;
        vyaw_last_ms = 0;
    }
    if (addr == 0x32) {
        vyaw_last_ms = now_ms;
    }

    switch (addr) {
        case 0x30: {
            // 前后移动速度
            int v = from_order_range(value, -100, 100);
            vx = (float)(v * 2.2);  // 与 MCP 工具保持一致的缩放
            control_mode = 0;
            motor_speed = 0;  // 最快舵机速度，避免动作结束后motor_speed=1000导致步态异常
            Action_ID = 0;    // 中断开机动画等正在运行的动作，让 move() 接管步态控制
            ESP_LOGI(TAG, "Set vx=%d", v);
            break;
        }
        case 0x32: {
            // 旋转速度
            int w = from_order_range(value, -100, 100);
            vyaw = (float)(w * 2.8);  // 与 MCP 工具保持一致的缩放
            control_mode = 0;
            motor_speed = 0;  // 最快舵机速度
            Action_ID = 0;    // 中断正在运行的动作，让 move() 接管步态控制
            ESP_LOGI(TAG, "Set vyaw=%d", w);
            break;
        }
        case 0x35: {
            // 身体高度（暂不支持，忽略）
            break;
        }
        case 0x3E: {
            // 动作指令
            uint8_t act = value;
            ESP_LOGI(TAG, "Action command: %d", act);

            if (act == 0x00 || act == 0xFF) {
                // 停止动作，恢复默认姿态
                Clear_State(2);
            } else if (act <= ACTION_NUMBER) {
                // 映射 XGO 协议动作 ID 到 Arm 动作 ID
                // XGO: 1-左右摇摆, 2-高低起伏, 3-前进后退, 4-四方蛇形, 5-升降旋转, 6-圆周晃动
                // Arm: Wave, Naughty, Lookup, Swing, Rolling, Angry, Swimming...
                static const uint8_t action_map[] = {
                    0,           // 0: 无动作
                    Swing_ID,    // 1: 左右摇摆 -> Swing
                    Bouncing_ID, // 2: 高低起伏 -> Bouncing
                    Naughty_ID,  // 3: 前进后退 -> Naughty
                    Rolling_ID,  // 4: 四方蛇形 -> Rolling
                    Shaking_ID,  // 5: 升降旋转 -> Shaking
                    Swimming_ID, // 6: 圆周晃动 -> Swimming
                };

                uint8_t mapped_action = (act < sizeof(action_map)) ? action_map[act] : act;
                Action_ID = mapped_action;
                vx = 0.0f;
                vyaw = 0.0f;
                control_mode = 0;
            }
            break;
        }
        case 0x03: {
            // 表演模式
            if (value == 0x01) {
                set_action_loop_flag(1);
            } else {
                set_action_loop_flag(0);
            }
            break;
        }
        case 0x04: {
            // 标定模式（暂不支持通过BLE控制）
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown address: 0x%02X", addr);
            break;
    }
}

#else  // CONFIG_BT_NIMBLE_ENABLED

// BT/NimBLE 未启用时的空实现
extern "C" bool ble_remote_init() {
    return false;
}

extern "C" void ble_remote_deinit() {}

extern "C" bool ble_remote_is_running() {
    return false;
}

extern "C" void ble_remote_send(const uint8_t* data, size_t len) {}

extern "C" void ble_remote_on_rx(const uint8_t* data, size_t len) {}

#endif  // CONFIG_BT_NIMBLE_ENABLED
