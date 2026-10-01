#ifndef ROBOT_ACTION_H
#define ROBOT_ACTION_H
#include "robot.h"

#define ACTION_NUMBER 1
#define reset_ID      255

void robot_action();
void tars_gait_update();
void tars_set_vx(int speed);
void tars_set_vyaw(int speed);
void tars_set_forward(int speed, int time_ms);
void tars_set_turn(int speed, int time_ms);
void Reset();

#endif
