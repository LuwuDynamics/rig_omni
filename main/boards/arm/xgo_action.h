#ifndef XGO_ACTION_H
#define XGO_ACTION_H
#include "xgo.h"

#define TS 100
#define ACTION_NUMBER 0x18  // 保持原编号空间（最高 WagTail_ID=0x17）
#define Stretch_ID       0x09
#define TeachPlayback_ID 0x11
#define Apology_ID       0x12
#define Shy_ID           0x13
#define Coquetry_ID      0x14
#define Bow_ID           0x15
#define FeignDeath_ID    0x16
#define WagTail_ID       0x17
#define reset_ID         255


void xgo_action();
void normal_state();
void Updated_Counter();
void Clear_State(uint8_t mode);
void set_motor_pos(int p1, int p2, int p3, int p4, int p5);

void Stretch();
void teach_playback();
void Apology();
void Shy();
void Coquetry();
void Bow();
void FeignDeath();
void WagTail();
void Reset();
void node();
void shake();
#endif
