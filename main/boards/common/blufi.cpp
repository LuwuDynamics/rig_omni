#include "blufi.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "esp_bt.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "wifi_board.h"
#include "board.h"

#define BLUFI_DEVICE_NAME_PREFIX "RIG-Puppy"
static char blufi_device_name[20] = "RIG-Puppy0000";

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "console/console.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#endif

extern "C" {
void esp_blufi_adv_start(void);

void esp_blufi_adv_stop(void);

void esp_blufi_disconnect(void);

void btc_blufi_report_error(esp_blufi_error_state_t state);

#ifdef CONFIG_BT_BLUEDROID_ENABLED
void esp_blufi_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);
int esp_blufi_gatt_svr_init(void);
void esp_blufi_gatt_svr_deinit(void);
void esp_blufi_btc_init(void);
void esp_blufi_btc_deinit(void);
#endif
}

#include <wifi_station.h>
#include "esp_crc.h"
#include "esp_random.h"
#include "mbedtls/md5.h"
#include "ssid_manager.h"

static const char* BLUFI_TAG = "BLUFI_CLASS";

static wifi_mode_t GetWifiModeWithFallback(const WifiManager& wifi) {
    if (wifi.IsConfigMode()) {
        return WIFI_MODE_AP;
    }
    if (wifi.IsInitialized() && wifi.IsConnected()) {
        return WIFI_MODE_STA;
    }

    wifi_mode_t mode = WIFI_MODE_STA;
    esp_wifi_get_mode(&mode);
    return mode;
}

Blufi& Blufi::GetInstance() {
    static Blufi instance;
    return instance;
}

Blufi::Blufi()
    : m_sec(nullptr),
      m_ble_is_connected(false),
      m_sta_connected(false),
      m_sta_got_ip(false),
      m_provisioned(false),
      m_deinited(false),
      m_sta_ssid_len(0),
      m_sta_is_connecting(false) {
    memset(&m_sta_config, 0, sizeof(m_sta_config));
    memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
    memset(m_sta_ssid, 0, sizeof(m_sta_ssid));
    memset(&m_sta_conn_info, 0, sizeof(m_sta_conn_info));
}

Blufi::~Blufi() {
    if (m_sec) {
        _security_deinit();
    }
}

esp_err_t Blufi::init() {
    esp_err_t ret = ESP_FAIL;
    inited_ = true;
    m_provisioned = false;
    m_deinited = false;

    // Start WiFi scan early to have results ready when user connects
    auto& wifi_manager = WifiManager::GetInstance();
    if (!wifi_manager.IsInitialized() || !wifi_manager.IsConfigMode()) {
        // start scan immediately
        start_wifi_scan();
    } else {
        ESP_LOGE(BLUFI_TAG,
                 "Blufi and WiFi hotspot network configuration cannot "
                 "be used simultaneously.");
        return ret;
    }

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = _controller_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    ret = _host_and_cb_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI host and cb init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(BLUFI_TAG, "BLUFI VERSION %04x", esp_blufi_get_version());
    return ESP_OK;
}

esp_err_t Blufi::deinit() {
    esp_err_t ret = ESP_OK;

    if (inited_) {
        if (m_deinited) {
            return ESP_OK;
        }
        m_deinited = true;
        ret = _host_deinit();
        if (ret) {
            ESP_LOGE(BLUFI_TAG, "Host deinit failed: %s", esp_err_to_name(ret));
        }
        // 注意：不再 deinit 蓝牙控制器，以支持 BLE 遥控模式
        // 蓝牙控制器保持运行状态
    }
    return ret;
}

#ifdef CONFIG_BT_BLUEDROID_ENABLED
esp_err_t Blufi::_host_init() {
    esp_err_t ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s init bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ESP_LOGI(BLUFI_TAG, "BD ADDR: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit() {
    esp_err_t ret = esp_blufi_profile_deinit();
    if (ret != ESP_OK)
        return ret;

    ret = esp_bluedroid_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Blufi::_gap_register_callback() {
    esp_err_t rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if (rc) {
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = _gap_register_callback();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s gap register failed, error code = %x", __func__, ret);
        return ret;
    }
    return ESP_OK;
}
#endif /* CONFIG_BT_BLUEDROID_ENABLED */

#ifdef CONFIG_BT_NIMBLE_ENABLED

void Blufi::_nimble_on_reset(int reason) {
    ESP_LOGE(BLUFI_TAG, "NimBLE Resetting state; reason=%d", reason);
}

void Blufi::_nimble_on_sync() { esp_blufi_profile_init(); }

void Blufi::_nimble_host_task(void* param) {
    ESP_LOGI(BLUFI_TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t Blufi::_host_init() {
    ble_hs_cfg.reset_cb = _nimble_on_reset;
    ble_hs_cfg.sync_cb = _nimble_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;

    ble_hs_cfg.sm_io_cap = 4;
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif

    int rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    esp_blufi_btc_init();

    nimble_port_freertos_init(_nimble_host_task);
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit(void) {
    // 只 deinit BluFi 相关资源，保持 NimBLE 运行以支持 BLE 遥控模式
    esp_blufi_gatt_svr_deinit();
    esp_err_t ret = esp_blufi_profile_deinit();
    esp_blufi_btc_deinit();
    
    // 停止 BluFi 广播
    ble_gap_adv_stop();
    
    // 注意：不再停止 NimBLE，以支持后续 BLE 遥控功能
    // 如果需要完全释放蓝牙资源，可以调用 nimble_port_stop() 和 esp_nimble_deinit()
    ESP_LOGI(BLUFI_TAG, "BluFi services stopped, NimBLE kept running for BLE remote");
    return ret;
}

esp_err_t Blufi::_gap_register_callback(void) { return ESP_OK; }

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }

    // Host init must be called after registering callbacks for NimBLE
    ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
esp_err_t Blufi::_controller_init() {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

#ifdef CONFIG_BT_NIMBLE_ENABLED
    ret = esp_nimble_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "esp_nimble_init() failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    return ESP_OK;
}

esp_err_t Blufi::_controller_deinit() {
    esp_err_t ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable controller failed: %s", __func__, esp_err_to_name(ret));
    }
    ret = esp_bt_controller_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit controller failed: %s", __func__, esp_err_to_name(ret));
    }
    return ret;
}
#endif

static int myrand(void* rng_state, unsigned char* output, size_t len) {
    esp_fill_random(output, len);
    return 0;
}

void Blufi::_security_init() {
    m_sec = new BlufiSecurity();
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate security context");
        return;
    }
    memset(m_sec, 0, sizeof(BlufiSecurity));
    m_sec->dhm = new mbedtls_dhm_context();
    m_sec->aes = new mbedtls_aes_context();

    mbedtls_dhm_init(m_sec->dhm);
    mbedtls_aes_init(m_sec->aes);

    memset(m_sec->iv, 0x0, sizeof(m_sec->iv));
}

void Blufi::_security_deinit() {
    if (m_sec == nullptr)
        return;

    if (m_sec->dh_param) {
        free(m_sec->dh_param);
    }
    mbedtls_dhm_free(m_sec->dhm);
    mbedtls_aes_free(m_sec->aes);
    delete m_sec->dhm;
    delete m_sec->aes;
    delete m_sec;
    m_sec = nullptr;
}

void Blufi::_dh_negotiate_data_handler(uint8_t* data, int len, uint8_t** output_data,
                                       int* output_len, bool* need_free) {
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Security not initialized in DH handler");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    if (len < 1) {
        ESP_LOGE(BLUFI_TAG, "DH handler: data too short");
        btc_blufi_report_error(ESP_BLUFI_DATA_FORMAT_ERROR);
        return;
    }

    uint8_t type = data[0];
    switch (type) {
        case 0x00:
            if (len < 3) {
                ESP_LOGE(BLUFI_TAG, "DH_PARAM_LEN packet too short");
                btc_blufi_report_error(ESP_BLUFI_DATA_FORMAT_ERROR);
                return;
            }

            m_sec->dh_param_len = (data[1] << 8) | data[2];
            if (m_sec->dh_param) {
                free(m_sec->dh_param);
                m_sec->dh_param = nullptr;
            }
            m_sec->dh_param = (uint8_t*)malloc(m_sec->dh_param_len);
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH malloc failed");
                btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            }
            break;
        case 0x01: {
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH param not allocated");
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            uint8_t* param = m_sec->dh_param;
            memcpy(m_sec->dh_param, &data[1], m_sec->dh_param_len);
            int ret = mbedtls_dhm_read_params(m_sec->dhm, &param, &param[m_sec->dh_param_len]);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_read_params failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }

            const int dhm_len = mbedtls_dhm_get_len(m_sec->dhm);

            ret = mbedtls_dhm_make_public(m_sec->dhm, dhm_len, m_sec->self_public_key, dhm_len,
                                          myrand, NULL);
            if (ret != 0) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_make_public failed: %d", ret);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }
            ret = mbedtls_dhm_calc_secret(m_sec->dhm, m_sec->share_key, SHARE_KEY_LEN,
                                          &m_sec->share_len, myrand, NULL);
            if (ret != 0) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_calc_secret failed: %d", ret);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            ret = mbedtls_md5(m_sec->share_key, m_sec->share_len, m_sec->psk);
            if (ret != 0) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_md5 failed: %d", ret);
                btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
                return;
            }
            ret = mbedtls_aes_setkey_enc(m_sec->aes, m_sec->psk, PSK_LEN * 8);
            if (ret != 0) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_aes_setkey_enc failed: -0x%04X", -ret);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }
            *output_data = m_sec->self_public_key;
            *output_len = dhm_len;
            *need_free = false;
            ESP_LOGI(BLUFI_TAG, "DH negotiation completed successfully");

            free(m_sec->dh_param);
            m_sec->dh_param = nullptr;
            m_sec->dh_param_len = 0;
            break;
        }
        default:
            ESP_LOGE(BLUFI_TAG, "DH handler unknown type: %d", type);
    }
}

int Blufi::_aes_encrypt(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    if (!m_sec || !m_sec->aes || !crypt_data || crypt_len <= 0) {
        ESP_LOGE(BLUFI_TAG, "Invalid parameters for AES encryption");
        return -ESP_ERR_INVALID_ARG;
    }

    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    int ret = mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_ENCRYPT, crypt_len, &iv_offset, iv0,
                                       crypt_data, crypt_data);

    if (ret == 0) {
        return crypt_len;
    } else {
        ESP_LOGE(BLUFI_TAG, "AES encrypt failed: %d", ret);
        return ret;
    }
}

int Blufi::_aes_decrypt(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    if (!m_sec || !m_sec->aes || !crypt_data || crypt_len < 0) {
        ESP_LOGE(BLUFI_TAG, "Invalid parameters for AES decryption %p %p %d", m_sec->aes,
                 crypt_data, crypt_len);
        return -ESP_ERR_INVALID_ARG;
    }

    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    int ret = mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_DECRYPT, crypt_len, &iv_offset, iv0,
                                       crypt_data, crypt_data);
    if (ret != 0) {
        ESP_LOGE(BLUFI_TAG, "AES decrypt failed: %d", ret);
        return ret;
    } else {
        return crypt_len;
    }
}

uint16_t Blufi::_crc_checksum(uint8_t iv8, uint8_t* data, int len) {
    return esp_crc16_be(0, data, len);
}

int Blufi::_get_softap_conn_num() {
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized() || !wifi.IsConfigMode()) {
        return 0;
    }

    wifi_sta_list_t sta_list{};
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

void Blufi::start_wifi_scan() {
    ESP_LOGI(BLUFI_TAG, "Starting dedicated WiFi scan");

    // Check if a scan is already in progress
    if (m_scan_in_progress) {
        ESP_LOGW(BLUFI_TAG, "Scan already in progress, skipping");
        return;
    }

    m_scan_in_progress = true;
    // 确保 WiFi 已初始化
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }
    // WiFi STA netif 已由 wifi_station 创建，这里无需重复创建
    // 调用 esp_netif_create_default_wifi_sta() 会因 handlers 已注册导致 abort

    // 设置为 STA 模式并启动 WiFi（如果未启动）
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(BLUFI_TAG, "Failed to set WiFi mode to STA: %s", esp_err_to_name(err));
        m_scan_in_progress = false;
        return;
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        // ESP_ERR_WIFI_CONN 表示 WiFi 已经启动，可以忽略
        ESP_LOGE(BLUFI_TAG, "Failed to start WiFi: %s", esp_err_to_name(err));
        m_scan_in_progress = false;
        return;
    }

    // 注册扫描事件处理器（如果还没注册）
    static bool scan_handler_registered = false;
    if (!scan_handler_registered) {
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                            &Blufi::_wifi_scan_event_handler, this, nullptr);
        scan_handler_registered = true;
    }

    // 启动扫描
    err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) {
        ESP_LOGE(BLUFI_TAG, "Failed to start WiFi scan: %s", esp_err_to_name(err));
        m_scan_in_progress = false;
        return;
    }

    ESP_LOGI(BLUFI_TAG, "WiFi scan started");
}

void Blufi::_send_wifi_list() {
    if (m_ap_records.empty()) {
        ESP_LOGW(BLUFI_TAG, "No AP records available to send");
        return;
    }

    // 按信号强度排序（RSSI 越大越强）
    std::sort(m_ap_records.begin(), m_ap_records.end(), [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) {
        return a.rssi > b.rssi;
    });

    // 限制发送数量，避免蓝牙传输超时
    const size_t MAX_AP_COUNT = 10;
    size_t send_count = std::min(m_ap_records.size(), MAX_AP_COUNT);

    ESP_LOGI(BLUFI_TAG, "Sending WiFi list: %d/%d APs (limited to %d)", 
             send_count, m_ap_records.size(), MAX_AP_COUNT);

    std::vector<esp_blufi_ap_record_t> blufi_ap_list;
    for (size_t i = 0; i < send_count; i++) {
        const auto& ap = m_ap_records[i];
        esp_blufi_ap_record_t blufi_ap;
        memset(&blufi_ap, 0, sizeof(blufi_ap));
        memcpy(blufi_ap.ssid, ap.ssid, std::min((size_t)32, sizeof(ap.ssid)));
        blufi_ap.rssi = ap.rssi;
        blufi_ap_list.push_back(blufi_ap);
    }

    esp_blufi_send_wifi_list(blufi_ap_list.size(), blufi_ap_list.data());
    // 不再自动扫描，等待下次用户请求时再扫描
}

void Blufi::_wifi_scan_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                     void* event_data) {
    Blufi* self = static_cast<Blufi*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(BLUFI_TAG, "WiFi scan done");

        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);

        if (ap_num == 0) {
            ESP_LOGW(BLUFI_TAG, "No APs found");
            self->m_ap_records.clear();
        } else {
            if (static_cast<Blufi*>(arg)->m_scan_should_save_ssid == true) {
                self->m_ap_records.resize(ap_num);
                esp_wifi_scan_get_ap_records(&ap_num, self->m_ap_records.data());

                ESP_LOGI(BLUFI_TAG, "Found %d APs", ap_num);
                for (const auto& ap : self->m_ap_records) {
                    ESP_LOGI(BLUFI_TAG, "  SSID: %s, RSSI: %d, Authmode: %d", (char*)ap.ssid,
                             ap.rssi, ap.authmode);
                }
            }
        }
        self->m_scan_in_progress = false;
    }
}

void Blufi::_handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t* param) {
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI init finish");
            // 根据 MAC 地址动态生成设备名 RIG-PuppyXXXX
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_BT);
            snprintf(blufi_device_name, sizeof(blufi_device_name), "%s%02X%02X",
                     BLUFI_DEVICE_NAME_PREFIX, mac[4], mac[5]);
#ifdef CONFIG_BT_NIMBLE_ENABLED
            ble_svc_gap_device_name_set(blufi_device_name);
#else
            esp_ble_gap_set_device_name(blufi_device_name);
#endif
            ESP_LOGI(BLUFI_TAG, "BluFi device name: %s", blufi_device_name);
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI deinit finish");
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble connect");
            m_ble_is_connected = true;
            m_scan_in_progress = false;  // 重置扫描状态
            m_scan_should_save_ssid = true;  // 重置标志，允许保存扫描结果
            m_ap_records.clear();        // 清除旧的扫描结果
            esp_blufi_adv_stop();
            _security_init();
            // 新连接时立即开始扫描，确保有 WiFi 列表
            start_wifi_scan();
            break;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble disconnect");
            m_ble_is_connected = false;
            m_scan_in_progress = false;  // 重置扫描状态
            m_ap_records.clear();        // 清除扫描结果
            _security_deinit();
            if (!m_provisioned) {
                esp_blufi_adv_start();
            } else {
                esp_blufi_adv_stop();
                if (!m_deinited) {
                    xTaskCreate(
                        [](void* ctx) {
                            static_cast<Blufi*>(ctx)->deinit();
                            vTaskDelete(nullptr);
                        },
                        "blufi_deinit", 4096, this, 5, nullptr);
                }
            }
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE: {
            ESP_LOGI(BLUFI_TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            auto& wifi_manager = WifiManager::GetInstance();
            if (!wifi_manager.IsInitialized() && !wifi_manager.Initialize()) {
                ESP_LOGE(BLUFI_TAG, "Failed to initialize WifiManager for opmode change");
                break;
            }
            switch (param->wifi_mode.op_mode) {
                case WIFI_MODE_STA:
                    wifi_manager.StartStation();
                    break;
                case WIFI_MODE_AP:
                    wifi_manager.StartConfigAp();
                    break;
                case WIFI_MODE_APSTA:
                    ESP_LOGW(BLUFI_TAG, "APSTA mode not supported, starting station only");
                    wifi_manager.StartStation();
                    break;
                default:
                    wifi_manager.StopStation();
                    wifi_manager.StopConfigAp();
                    break;
            }
            break;
        }
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP: {
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi connect to AP directly");
            std::string ssid(reinterpret_cast<const char*>(m_sta_config.sta.ssid));
            std::string password(reinterpret_cast<const char*>(m_sta_config.sta.password));

            // 保存到 NVS 供后续使用
            SsidManager::GetInstance().AddSsid(ssid, password);
            m_scan_should_save_ssid = false;

            m_sta_ssid_len = static_cast<int>(std::min(ssid.size(), sizeof(m_sta_ssid)));
            memcpy(m_sta_ssid, ssid.c_str(), m_sta_ssid_len);
            memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
            m_sta_connected = false;
            m_sta_got_ip = false;
            m_sta_is_connecting = true;
            m_sta_conn_info = {};
            m_sta_conn_info.sta_ssid = m_sta_ssid;
            m_sta_conn_info.sta_ssid_len = m_sta_ssid_len;

            // 停止之前的 WiFi 状态
            esp_wifi_scan_stop();
            esp_wifi_disconnect();
            esp_wifi_stop();
            vTaskDelay(pdMS_TO_TICKS(200));

            // WiFi STA netif 已由 wifi_station 创建，无需重复创建
            // 创建 netif（如果不存在）
            esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (sta_netif == nullptr) {
                sta_netif = esp_netif_create_default_wifi_sta();
            }

            // 直接连接 WiFi（不扫描）
            ESP_LOGI(BLUFI_TAG, "Connecting WiFi directly to SSID: %s", ssid.c_str());
            wifi_config_t wifi_config = {};
            memcpy(wifi_config.sta.ssid, ssid.c_str(), std::min(ssid.size(), sizeof(wifi_config.sta.ssid) - 1));
            memcpy(wifi_config.sta.password, password.c_str(), std::min(password.size(), sizeof(wifi_config.sta.password) - 1));
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_start();
            esp_wifi_connect();

            xTaskCreate(
                [](void* ctx) {
                    auto* self = static_cast<Blufi*>(ctx);
                    constexpr int kConnectTimeoutMs = 15000;
                    constexpr TickType_t kDelayTick = pdMS_TO_TICKS(500);

                    bool connected = false;
                    int waited_ms = 0;

                    // 用 esp_wifi API 检查连接状态
                    while (waited_ms < kConnectTimeoutMs && !connected) {
                        vTaskDelay(kDelayTick);
                        waited_ms += 500;
                        
                        wifi_ap_record_t ap_info{};
                        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                            // 检查是否获取到 IP
                            esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                            if (netif) {
                                esp_netif_ip_info_t ip_info;
                                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                                    connected = true;
                                }
                            }
                        }
                        
                        if (waited_ms % 3000 == 0) {
                            ESP_LOGI(BLUFI_TAG, "Waiting for WiFi... %d/%d ms", waited_ms, kConnectTimeoutMs);
                        }
                    }

                    wifi_mode_t mode = WIFI_MODE_STA;
                    esp_wifi_get_mode(&mode);
                    const int softap_conn_num = _get_softap_conn_num();

                    if (connected) {
                        self->m_sta_is_connecting = false;
                        self->m_sta_connected = true;
                        self->m_sta_got_ip = true;
                        self->m_provisioned = true;

                        // 获取 AP 的 BSSID
                        wifi_ap_record_t ap_info{};
                        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                            memcpy(self->m_sta_bssid, ap_info.bssid, sizeof(self->m_sta_bssid));
                        }

                        // 获取 STA 的 MAC 地址
                        uint8_t sta_mac[6] = {};
                        esp_wifi_get_mac(WIFI_IF_STA, sta_mac);

                        esp_blufi_extra_info_t info = {};
                        memcpy(info.sta_bssid, self->m_sta_bssid, sizeof(self->m_sta_bssid));
                        info.sta_bssid_set = true;
                        info.sta_ssid = self->m_sta_ssid;
                        info.sta_ssid_len = self->m_sta_ssid_len;
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_conn_num, &info);
                        
                        // 发送自定义数据帧：STA MAC 地址（6 字节）
                        esp_blufi_send_custom_data(sta_mac, 6);
                        
                        ESP_LOGI(BLUFI_TAG, "WiFi connected, STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                                 sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]);

                        // 配网成功，先触发配网结束回调（让机器狗起身），然后延迟重启
                        ESP_LOGI(BLUFI_TAG, "BluFi provisioning complete, triggering stand up action...");
                        Board::GetInstance().OnWifiConfigEnd();
                        
                        // 等待起身动作和语音播放完成后重启
                        ESP_LOGI(BLUFI_TAG, "Waiting for stand up action and audio, rebooting in 5 seconds...");
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        esp_restart();
                    } else {
                        self->m_sta_is_connecting = false;
                        self->m_sta_connected = false;
                        self->m_sta_got_ip = false;

                        esp_blufi_extra_info_t info = {};
                        info.sta_ssid = self->m_sta_ssid;
                        info.sta_ssid_len = self->m_sta_ssid_len;
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_conn_num, &info);
                        ESP_LOGE(BLUFI_TAG, "WiFi connection timeout");
                    }
                    vTaskDelete(nullptr);
                },
                "blufi_wifi_conn", 4096, this, 5, nullptr);
            break;
        }
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi disconnect from AP");
            if (WifiManager::GetInstance().IsInitialized()) {
                WifiManager::GetInstance().StopStation();
            }
            m_sta_is_connecting = false;
            m_sta_connected = false;
            m_sta_got_ip = false;
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            auto& wifi = WifiManager::GetInstance();
            wifi_mode_t mode = GetWifiModeWithFallback(wifi);
            const int softap_conn_num = _get_softap_conn_num();

            if (wifi.IsInitialized() && wifi.IsConnected()) {
                m_sta_connected = true;
                m_sta_got_ip = true;

                auto current_ssid = wifi.GetSsid();
                if (!current_ssid.empty()) {
                    m_sta_ssid_len =
                        static_cast<int>(std::min(current_ssid.size(), sizeof(m_sta_ssid)));
                    memcpy(m_sta_ssid, current_ssid.c_str(), m_sta_ssid_len);
                }

                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, m_sta_bssid, 6);
                info.sta_ssid = m_sta_ssid;
                info.sta_ssid_len = m_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_conn_num,
                                                &info);
            } else if (m_sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_conn_num,
                                                &m_sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_conn_num,
                                                &m_sta_conn_info);
            }
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi status");
            break;
        }
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(m_sta_config.sta.bssid, param->sta_bssid.bssid, 6);
            m_sta_config.sta.bssid_set = true;
            ESP_LOGI(BLUFI_TAG, "Recv STA BSSID");
            break;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy((char*)m_sta_config.sta.ssid, (char*)param->sta_ssid.ssid,
                    param->sta_ssid.ssid_len);
            m_sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA SSID: %s", m_sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char*)m_sta_config.sta.password, (char*)param->sta_passwd.passwd,
                    param->sta_passwd.passwd_len);
            m_sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA PASSWORD : %s", m_sta_config.sta.password);
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi list request");
            
            // 如果扫描正在进行，等待完成（最多 10 秒）
            int wait_count = 0;
            while (m_scan_in_progress && wait_count < 20) {  // 20 * 500ms = 10s
                vTaskDelay(pdMS_TO_TICKS(500));
                wait_count++;
            }
            
            if (m_scan_in_progress) {
                ESP_LOGW(BLUFI_TAG, "Scan timeout, sending current results");
                m_scan_in_progress = false;
            }
            
            _send_wifi_list();
            break;
        }
        case ESP_BLUFI_EVENT_REPORT_ERROR: {
            ESP_LOGW(BLUFI_TAG, "BLUFI report error: %d", param->report_error.state);
            esp_blufi_send_error_info(param->report_error.state);
            break;
        }
        default:
            ESP_LOGW(BLUFI_TAG, "Unhandled event: %d", event);
            break;
    }
}

void Blufi::_event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t* param) {
    GetInstance()._handle_event(event, param);
}

void Blufi::_negotiate_data_handler_trampoline(uint8_t* data, int len, uint8_t** output_data,
                                               int* output_len, bool* need_free) {
    GetInstance()._dh_negotiate_data_handler(data, len, output_data, output_len, need_free);
}

int Blufi::_encrypt_func_trampoline(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    return GetInstance()._aes_encrypt(iv8, crypt_data, crypt_len);
}

int Blufi::_decrypt_func_trampoline(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    return GetInstance()._aes_decrypt(iv8, crypt_data, crypt_len);
}

uint16_t Blufi::_checksum_func_trampoline(uint8_t iv8, uint8_t* data, int len) {
    return _crc_checksum(iv8, data, len);
}
