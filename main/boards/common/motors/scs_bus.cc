#include "scs_bus.h"
#include <string.h>

uint8_t serial_lock = 0;

static uart_port_t s_uart = SCS_BUS_UART;
static scs_frame_cb_t s_on_frame = nullptr;
static uint8_t rx_flag = 0;
static uint8_t rx_len = 0;
static uint8_t rx_data_len = 0;
static uint8_t rx_id = 0;
static uint8_t rx_buffer[32] = {0};

void scs_bus_set_uart(uart_port_t uart) {
    s_uart = uart;
}

void scs_bus_on_frame(scs_frame_cb_t cb) {
    s_on_frame = cb;
}

void scs_store_le16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)(value >> 8);
}

void scs_store_be16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)(value & 0xff);
}

int16_t scs_load_le16(uint8_t lo, uint8_t hi) {
    return (int16_t)(lo | ((uint16_t)hi << 8));
}

int16_t scs_load_be16(uint8_t hi, uint8_t lo) {
    return (int16_t)(lo | ((uint16_t)hi << 8));
}

bool scs_decode_voltage(const uint8_t *frame, uint8_t length_field, float *volts) {
    if (length_field != 3 || frame == nullptr || volts == nullptr) {
        return false;
    }
    *volts = frame[5] * 0.1f;
    return true;
}

void scs_send(const uint8_t *body, uint16_t body_len) {
    if (serial_lock || body == nullptr || body_len == 0) {
        return;
    }
    serial_lock = 1;

    uint8_t packet[128];
    if (body_len + 3 > sizeof(packet)) {
        serial_lock = 0;
        return;
    }

    packet[0] = 0xFF;
    packet[1] = 0xFF;
    uint8_t check = 0;
    for (uint16_t i = 0; i < body_len; i++) {
        packet[2 + i] = body[i];
        check = (uint8_t)(check + body[i]);
    }
    packet[2 + body_len] = (uint8_t)~check;
    uart_write_bytes(s_uart, packet, body_len + 3);
    uart_wait_tx_done(s_uart, pdMS_TO_TICKS(50));
    serial_lock = 0;
}

void scs_write_byte(uint8_t id, uint8_t addr, uint8_t value) {
    uint8_t body[5] = {id, 0x04, SCS_INST_WRITE, addr, value};
    scs_send(body, sizeof(body));
}

void scs_write_bytes(uint8_t id, uint8_t addr, const uint8_t *data, uint8_t n) {
    uint8_t body[8 + 16];
    if (data == nullptr || n == 0 || n > 16) {
        return;
    }
    body[0] = id;
    body[1] = (uint8_t)(n + 3);
    body[2] = SCS_INST_WRITE;
    body[3] = addr;
    memcpy(&body[4], data, n);
    scs_send(body, (uint16_t)(4 + n));
}

void scs_read(uint8_t id, uint8_t addr, uint8_t n) {
    uint8_t body[5] = {id, 0x04, SCS_INST_READ, addr, n};
    scs_send(body, sizeof(body));
}

void scs_sync_write(uint8_t addr, uint8_t bytes_per_id, uint8_t id_count,
                    const uint8_t *ids, const uint8_t *payloads) {
    if (ids == nullptr || payloads == nullptr || id_count == 0 || bytes_per_id == 0) {
        return;
    }
    uint8_t body[8 + 7 * 16];
    const uint16_t data_bytes = (uint16_t)((1 + bytes_per_id) * id_count);
    const uint16_t body_len = (uint16_t)(5 + data_bytes);
    if (body_len > sizeof(body)) {
        return;
    }

    body[0] = SCS_BROADCAST_ID;
    body[1] = (uint8_t)(4 + data_bytes);
    body[2] = SCS_INST_SYNC_WRITE;
    body[3] = addr;
    body[4] = bytes_per_id;

    uint16_t idx = 5;
    for (uint8_t i = 0; i < id_count; i++) {
        body[idx++] = ids[i];
        memcpy(&body[idx], &payloads[i * bytes_per_id], bytes_per_id);
        idx = (uint16_t)(idx + bytes_per_id);
    }
    scs_send(body, idx);
}

void scs_bus_poll(TickType_t timeout_ticks) {
    uint8_t byte = 0;
    while (uart_read_bytes(s_uart, &byte, 1, timeout_ticks) > 0) {
        switch (rx_flag) {
            case 0:
                if (byte == 0xFF) {
                    rx_flag = 1;
                    rx_buffer[0] = 0xFF;
                }
                break;
            case 1:
                if (byte == 0xFF) {
                    rx_flag = 2;
                    rx_buffer[1] = 0xFF;
                } else {
                    rx_flag = 0;
                }
                break;
            case 2:
                rx_buffer[2] = byte;
                rx_id = byte;
                rx_flag = 3;
                break;
            case 3:
                if (byte == 0x08 || byte == 0x0B || byte == 0x03) {
                    rx_flag = 4;
                    rx_buffer[3] = byte;
                    rx_len = 0;
                    rx_data_len = byte;
                } else {
                    rx_flag = 0;
                }
                break;
            case 4:
                if (4 + rx_len >= sizeof(rx_buffer)) {
                    rx_flag = 0;
                    break;
                }
                rx_buffer[4 + rx_len] = byte;
                rx_len++;
                if (rx_len == rx_data_len) {
                    uint8_t check = 0;
                    for (int i = 0; i < 1 + rx_data_len; i++) {
                        check = (uint8_t)(check + rx_buffer[2 + i]);
                    }
                    check = (uint8_t)~check;
                    if (check == rx_buffer[3 + rx_data_len] && s_on_frame) {
                        s_on_frame(rx_id, rx_buffer, rx_data_len);
                    }
                    rx_flag = 0;
                }
                break;
            default:
                rx_flag = 0;
                break;
        }
    }
}
