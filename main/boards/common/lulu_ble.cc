#include "sdkconfig.h"

#if defined(CONFIG_PUPPY_ENABLE_BLE_CONTROL) && defined(CONFIG_BT_NIMBLE_ENABLED)

#include <string.h>
#include <esp_log.h>
#include <esp_mac.h>
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "xgo.h"

static const char *TAG = "LuluBle";

// 使用与 XGO 相同的 UUID（16-bit 形式）
// Service: 0xFFF0, RX(Notify): 0xFFF1, TX(Write): 0xFFF2
static const ble_uuid16_t LULU_SERVICE_UUID  = BLE_UUID16_INIT(0xFFF0);
static const ble_uuid16_t LULU_CHAR_RX_UUID  = BLE_UUID16_INIT(0xFFF1); // 设备 -> APP (Notify)
static const ble_uuid16_t LULU_CHAR_TX_UUID  = BLE_UUID16_INIT(0xFFF2); // APP -> 设备 (Write)

static uint16_t lulu_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t lulu_char_rx_val_handle = 0;
static uint16_t lulu_char_tx_val_handle = 0;
static bool lulu_ble_running = false;

static char lulu_device_name[16] = "lulu-0000";

// 广播参数
static void lulu_ble_advertise(void);

// GATT 特征访问回调
static int lulu_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // 收到 APP 写入 FFF2，交给协议解析函数
        if (attr_handle == lulu_char_tx_val_handle && ctxt->om != NULL) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > 0 && len <= 256) {
                uint8_t buf[256];
                os_mbuf_copydata(ctxt->om, 0, len, buf);
                lulu_ble_on_rx_bytes(buf, len);
            }
        }
    }
    return 0;
}

// GATT 服务定义
static const struct ble_gatt_svc_def lulu_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &LULU_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX Characteristic (Notify/Read): 设备 -> APP
                .uuid = &LULU_CHAR_RX_UUID.u,
                .access_cb = lulu_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = &lulu_char_rx_val_handle,
            },
            {
                // TX Characteristic (Write/Write_NR): APP -> 设备
                .uuid = &LULU_CHAR_TX_UUID.u,
                .access_cb = lulu_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &lulu_char_tx_val_handle,
            },
            { 0 } // 结束标记
        },
    },
    { 0 } // 结束标记
};

// GAP 事件回调
static int lulu_gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE connected, conn_handle=%d, status=%d",
                 event->connect.conn_handle, event->connect.status);
        if (event->connect.status == 0) {
            lulu_conn_handle = event->connect.conn_handle;
        } else {
            lulu_ble_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);
        lulu_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        lulu_ble_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete");
        lulu_ble_advertise();
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

// BLE 发送通知（设备 -> APP）
extern "C" void lulu_ble_send(const uint8_t* data, size_t len) {
    if (lulu_conn_handle == BLE_HS_CONN_HANDLE_NONE || !lulu_ble_running) {
        return;
    }
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notify");
        return;
    }
    
    int rc = ble_gatts_notify_custom(lulu_conn_handle, lulu_char_rx_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Notify failed: %d", rc);
    }
}

// 开始广播
static void lulu_ble_advertise(void) {
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};

    // 设置广播数据
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 0;
    fields.name = (uint8_t *)lulu_device_name;
    fields.name_len = strlen(lulu_device_name);
    fields.name_is_complete = 1;

    // 16-bit Service UUID
    static ble_uuid16_t svc_uuid = BLE_UUID16_INIT(0xFFF0);
    fields.uuids16 = &svc_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    // 广播参数
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x20;  // 20ms
    adv_params.itvl_max = 0x40;  // 40ms

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, lulu_gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started as '%s'", lulu_device_name);
    }
}

// NimBLE host 同步回调
static void lulu_ble_on_sync(void) {
    ESP_LOGI(TAG, "BLE host synced");

    // 根据 MAC 地址设置设备名 LULU-XXXX
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(lulu_device_name, sizeof(lulu_device_name), "lulu-%02x%02x",
             mac[4], mac[5]);

    ble_svc_gap_device_name_set(lulu_device_name);

    lulu_ble_advertise();
}

// NimBLE host reset 回调
static void lulu_ble_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

// NimBLE host task
static void lulu_nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

extern "C" void lulu_ble_init() {
    // 如果已经初始化过，直接重新启动广播
    if (lulu_ble_running) {
        ESP_LOGI(TAG, "BLE already initialized, restarting advertising");
        lulu_ble_advertise();
        return;
    }

    ESP_LOGI(TAG, "Initializing BLE (NimBLE)");

    // nimble_port_init 会自动初始化 BT 控制器 (CONFIG_BT_CONTROLLER_ENABLED=y)
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return;
    }

    // 配置 NimBLE host
    ble_hs_cfg.reset_cb = lulu_ble_on_reset;
    ble_hs_cfg.sync_cb = lulu_ble_on_sync;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0;

    // 初始化 GATT 服务
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(lulu_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(lulu_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    // 启动 NimBLE host task
    nimble_port_freertos_init(lulu_nimble_host_task);

    lulu_ble_running = true;
    ESP_LOGI(TAG, "BLE initialized successfully");
}

extern "C" void lulu_ble_deinit() {
    // NimBLE 不支持完全的 deinit/reinit，只停止广播
    if (!lulu_ble_running) {
        return;
    }
    ESP_LOGI(TAG, "Stopping BLE advertising");
    ble_gap_adv_stop();
    
    // 断开连接
    if (lulu_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(lulu_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        lulu_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    // 保持 lulu_ble_running = true，下次开启时只需重新广播
}

extern "C" bool lulu_ble_is_running() {
    return lulu_ble_running;
}

#else

extern "C" void lulu_ble_init() {
    // BT/NimBLE 未启用时，不做任何事
}

extern "C" void lulu_ble_deinit() {
}

extern "C" bool lulu_ble_is_running() {
    return false;
}

#endif
