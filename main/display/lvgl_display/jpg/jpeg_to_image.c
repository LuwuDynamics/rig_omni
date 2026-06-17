#include <esp_check.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <sys/param.h>

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#include "jpeg_to_image.h"

#define TAG "jpeg_to_image"

static esp_err_t decode_with_new_jpeg(const uint8_t* src, size_t src_len, uint8_t** out, size_t* out_len, size_t* width,
                                      size_t* height, size_t* stride) {
    ESP_LOGD(TAG, "Decoding JPEG with software decoder");
    esp_err_t ret = ESP_OK;
    jpeg_error_t jpeg_ret = JPEG_ERR_OK;
    uint8_t* out_buf = NULL;
    jpeg_dec_io_t jpeg_io = {0};
    jpeg_dec_header_info_t out_info = {0};

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    config.rotate = JPEG_ROTATE_0D;

    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_ret = jpeg_dec_open(&config, &jpeg_dec);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open JPEG decoder");
        ret = ESP_FAIL;
        goto jpeg_dec_failed;
    }

    jpeg_io.inbuf = (uint8_t*)src;
    jpeg_io.inbuf_len = (int)src_len;

    jpeg_ret = jpeg_dec_parse_header(jpeg_dec, &jpeg_io, &out_info);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to parse JPEG header");
        ret = ESP_ERR_INVALID_ARG;
        goto jpeg_dec_failed;
    }

    ESP_LOGD(TAG, "JPEG header info: width=%d, height=%d", out_info.width, out_info.height);

    out_buf = jpeg_calloc_align(out_info.width * out_info.height * 2, 16);
    if (out_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for JPEG output buffer");
        ret = ESP_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    jpeg_io.outbuf = out_buf;
    jpeg_ret = jpeg_dec_process(jpeg_dec, &jpeg_io);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to decode JPEG");
        ret = ESP_FAIL;
        goto jpeg_dec_failed;
    }

    ESP_LOG_BUFFER_HEXDUMP(TAG, out_buf, MIN(out_info.width * out_info.height * 2, 256), ESP_LOG_DEBUG);

    *out = out_buf;
    out_buf = NULL;
    *out_len = (size_t)(out_info.width * out_info.height * 2);
    *width = (size_t)out_info.width;
    *height = (size_t)out_info.height;
    *stride = (size_t)out_info.width * 2;
    jpeg_dec_close(jpeg_dec);
    jpeg_dec = NULL;

    return ret;

jpeg_dec_failed:
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
        jpeg_dec = NULL;
    }
    if (out_buf) {
        jpeg_free_align(out_buf);
        out_buf = NULL;
    }

    *out = NULL;
    *out_len = 0;
    *width = 0;
    *height = 0;
    *stride = 0;
    return ret;
}

esp_err_t jpeg_to_image(const uint8_t* src, size_t src_len, uint8_t** out, size_t* out_len, size_t* width,
                        size_t* height, size_t* stride) {
    if (src == NULL || src_len == 0 || out == NULL || out_len == NULL || width == NULL || height == NULL ||
        stride == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    return decode_with_new_jpeg(src, src_len, out, out_len, width, height, stride);
}
