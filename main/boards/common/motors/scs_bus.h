#ifndef RIG_SCS_BUS_H
#define RIG_SCS_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCS_BUS_UART
#define SCS_BUS_UART UART_NUM_2
#endif

#define SCS_BROADCAST_ID  0xFE
#define SCS_INST_READ     0x02
#define SCS_INST_WRITE    0x03
#define SCS_INST_SYNC_WRITE 0x83

/*
 * 一帧：FF FF | ID | LEN | ...payload... | CHK
 * frame[3] 为 LEN；校验通过后回调。
 * LEN==3 一般为 1 字节电压；LEN==8/0x0B 为状态回读。
 */
typedef void (*scs_frame_cb_t)(uint8_t id, const uint8_t *frame, uint8_t length_field);

/* 发送互斥。Tars 深睡前会清零，避免挂起时锁死导致卸载指令被丢弃。 */
extern uint8_t serial_lock;

void scs_bus_set_uart(uart_port_t uart);
void scs_bus_on_frame(scs_frame_cb_t cb);
void scs_bus_poll(TickType_t timeout_ticks);

void scs_send(const uint8_t *body, uint16_t body_len);
void scs_write_byte(uint8_t id, uint8_t addr, uint8_t value);
void scs_write_bytes(uint8_t id, uint8_t addr, const uint8_t *data, uint8_t n);
void scs_read(uint8_t id, uint8_t addr, uint8_t n);
void scs_sync_write(uint8_t addr, uint8_t bytes_per_id, uint8_t id_count,
                    const uint8_t *ids, const uint8_t *payloads);

void scs_store_le16(uint8_t *out, uint16_t value);
void scs_store_be16(uint8_t *out, uint16_t value);
int16_t scs_load_le16(uint8_t lo, uint8_t hi);
int16_t scs_load_be16(uint8_t hi, uint8_t lo);

/* 电压回读：LEN==3 时 frame[5] 单位 0.1V。 */
bool scs_decode_voltage(const uint8_t *frame, uint8_t length_field, float *volts);

#ifdef __cplusplus
}
#endif

#endif
