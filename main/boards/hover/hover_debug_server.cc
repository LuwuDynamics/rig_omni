#include "hover_debug_server.h"
#include "xgo.h"
#include "imu.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "HoverDebug";

#define DEBUG_VAR_COUNT 20

float debug_var[DEBUG_VAR_COUNT] = {0};

static httpd_handle_t server = NULL;

// HTML页面
static const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hover Debug</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background: #1a1a2e; color: #eee; }
        h1 { color: #00d9ff; text-align: center; }
        .section { background: #16213e; border-radius: 10px; padding: 15px; margin: 15px 0; }
        .section h2 { color: #00d9ff; margin-top: 0; font-size: 18px; }
        .imu-data { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
        .imu-item { background: #0f3460; padding: 15px; border-radius: 8px; text-align: center; }
        .imu-item .label { color: #888; font-size: 12px; }
        .imu-item .value { font-size: 24px; font-weight: bold; color: #00d9ff; }
        .var-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
        .var-item { display: flex; align-items: center; gap: 10px; }
        .var-item label { min-width: 100px; color: #888; }
        .var-item input { flex: 1; padding: 8px; border: 1px solid #0f3460; border-radius: 5px; background: #0f3460; color: #fff; font-size: 16px; }
        .var-item button { padding: 8px 15px; background: #00d9ff; border: none; border-radius: 5px; color: #000; cursor: pointer; font-weight: bold; }
        .var-item button:hover { background: #00b8d4; }
        .var-item.reserved input { opacity: 0.5; }
        .status { text-align: center; padding: 10px; color: #0f0; font-size: 14px; }
        .refresh-btn { display: block; width: 100%; padding: 12px; background: #e94560; border: none; border-radius: 5px; color: #fff; font-size: 16px; cursor: pointer; margin-top: 10px; }
        .refresh-btn:hover { background: #c73e54; }
    </style>
</head>
<body>
    <h1>Hover Debug Panel</h1>
    
    <div class="section">
        <h2>IMU Data</h2>
        <div class="imu-data">
            <div class="imu-item"><div class="label">Roll</div><div class="value" id="roll">--</div></div>
            <div class="imu-item"><div class="label">Pitch</div><div class="value" id="pitch">--</div></div>
            <div class="imu-item"><div class="label">Yaw</div><div class="value" id="yaw">--</div></div>
        </div>
    </div>
    
    <div class="section">
        <h2>Debug Variables</h2>
        <div class="var-grid" id="var-grid"></div>
    </div>
    
    <div class="status" id="status">Ready</div>
    <button class="refresh-btn" onclick="fetchData()">Refresh</button>
    
    <script>
        const VAR_COUNT = 20;
        const VAR_LABELS = [
            'head', 'delta_pos', 'POS_kp', 'POS_kd', 'VEL_kp', 'VEL_ki', 'PIT_kp', 'PIT_kd', 'YAW_kp', 'delta_yaw',
            'LQR_k0', 'LQR_k1', 'LQR_k2', 'LQR_k3',
            'var14', 'var15', 'var16', 'var17', 'var18', 'var19'
        ];
        function createVarInputs() {
            const grid = document.getElementById('var-grid');
            for (let i = 0; i < VAR_COUNT; i++) {
                const div = document.createElement('div');
                div.className = 'var-item' + (i >= 14 ? ' reserved' : '');
                div.innerHTML = `
                    <label>${VAR_LABELS[i]}:</label>
                    <input type="number" step="any" id="var${i}" value="0" ${i >= 14 ? 'disabled' : ''}>
                    <button onclick="setVar(${i})" ${i >= 14 ? 'disabled' : ''}>Set</button>
                `;
                grid.appendChild(div);
            }
        }
        
        function fetchData() {
            fetch('/api/data')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('roll').textContent = data.imu.roll.toFixed(2);
                    document.getElementById('pitch').textContent = data.imu.pitch.toFixed(2);
                    document.getElementById('yaw').textContent = data.imu.yaw.toFixed(2);
                    document.getElementById('status').textContent = 'Updated: ' + new Date().toLocaleTimeString();
                })
                .catch(e => {
                    document.getElementById('status').textContent = 'Error: ' + e.message;
                });
        }
        
        function setVar(index) {
            if (index >= 14) return;
            const value = parseFloat(document.getElementById('var' + index).value);
            fetch('/api/set?i=' + index + '&v=' + value)
                .then(r => r.json())
                .then(data => {
                    document.getElementById('status').textContent = VAR_LABELS[index] + ' set to ' + value;
                })
                .catch(e => {
                    document.getElementById('status').textContent = 'Error: ' + e.message;
                });
        }
        
        createVarInputs();
        fetchData();
        setInterval(fetchData, 500);
    </script>
</body>
</html>
)rawliteral";

// GET / - 返回HTML页面
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

// GET /api/data - 返回IMU和变量数据
static esp_err_t data_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    cJSON *imu = cJSON_CreateObject();
    cJSON_AddNumberToObject(imu, "roll", roll);
    cJSON_AddNumberToObject(imu, "pitch", pitch);
    cJSON_AddNumberToObject(imu, "yaw", yaw);
    cJSON_AddItemToObject(root, "imu", imu);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/set?i=0&v=1.5 - 设置变量
static esp_err_t set_handler(httpd_req_t *req) {
    char buf[100];
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1 && buf_len < sizeof(buf)) {
        httpd_req_get_url_query_str(req, buf, buf_len);

        char param[32];
        int index = -1;
        float value = 0.0f;

        if (httpd_query_key_value(buf, "i", param, sizeof(param)) == ESP_OK) {
            index = atoi(param);
        }
        if (httpd_query_key_value(buf, "v", param, sizeof(param)) == ESP_OK) {
            value = atof(param);
        }

        switch (index) {
            case 0:
                target_head_pos = value;
                break;
            case 1:
                stable_pos = stable_pos + value;
                break;
            case 2:
                pid_pos.fpKp = value;
                break;
            case 3:
                pid_pos.fpKd = value;
                break;
            case 4:
                pid_vel.fpKp = value;
                break;
            case 5:
                pid_vel.fpKi = value;
                break;
            case 6:
                pid_pit.fpKp = value;
                break;
            case 7:
                kd_pit = value;
                break;
            case 8:
                imu_zero = value;
                break;
            case 9:
                stable_yaw = stable_yaw + value;
                break;
            case 10:
                lqr_k[0] = value;
                break;
            case 11:
                lqr_k[1] = value;
                break;
            case 12:
                lqr_k[2] = value;
                break;
            case 13:
                lqr_k[3] = value;
                break;
            default:
                break;
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

void hover_debug_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return;
    }

    httpd_uri_t uri_index = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
    httpd_uri_t uri_data = { .uri = "/api/data", .method = HTTP_GET, .handler = data_handler };
    httpd_uri_t uri_set = { .uri = "/api/set", .method = HTTP_GET, .handler = set_handler };

    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_data);
    httpd_register_uri_handler(server, &uri_set);

    ESP_LOGI(TAG, "Debug server started on port 80");
}

void hover_debug_server_stop(void) {
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Debug server stopped");
    }
}

bool hover_debug_server_is_running(void) {
    return server != NULL;
}
