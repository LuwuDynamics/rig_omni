#ifndef HOVER_DEBUG_SERVER_H
#define HOVER_DEBUG_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 20个调试变量 (10~13 映射 lqr_k)
extern float debug_var[20];

// 启动/停止调试服务器
void hover_debug_server_start(void);
void hover_debug_server_stop(void);
bool hover_debug_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // HOVER_DEBUG_SERVER_H
