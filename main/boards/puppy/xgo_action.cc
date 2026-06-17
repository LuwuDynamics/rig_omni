#include "xgo_action.h"
#include "math.h"
#include "esp_log.h"

static const char* TAG = "XGO_ACTION";

uint16_t Action_Counter[ACTION_NUMBER];

uint8_t ACTION_DONE = 0;

void action_loop(){
    //printf("ACTION_DONE: %d\r\n", ACTION_DONE);
    if(ACTION_DONE&&actionLoop_FLAG){
        ACTION_DONE++;
        if(ACTION_DONE==100)
        { 
            ACTION_DONE=0;
            Action_ID++;
            if(Action_ID==ACTION_NUMBER){
                Action_ID = 1;
            }
        }
    }
}

void xgo_action(){ 
    static uint8_t last_action_id = 0;
    if (Action_ID != last_action_id) {
        ESP_LOGI(TAG, "Action started: %d, Counter: %d", Action_ID, Action_Counter[Action_ID]);
        last_action_id = Action_ID;
    }
    action_loop(); 
    switch(Action_ID)
    {
        case 0:
            break;
        case Wave_ID:
            Wave();
            break;
        case Naughty_ID:
            Naughty();
            break;  
        case Swing_ID:
            Swing();
            break;
        case Lookup_ID:
            Lookup();
            break; 
        case Rolling_ID:
            Rolling();
            break; 
        case Angry_ID:
            Angry();
            break;
        case Swimming_ID:
            Swimming();
            break;
        case Pee_ID:
            Pee();
            break;
        case Stretch_ID:
            Stretch();
            break;
        case Bouncing_ID:
            Bouncing();
            break;
        case Shaking_ID:
            Shaking();
            break;
        case Sit_ID:
            Sit();
            break;
        case Scratch_ID:
            Scratch();
            break;
        case Hug_ID:
            Hug();
            break;
        case Keep_Sit_ID:
            Keep_Sit();
            break;
        case Sit_Reset_ID:
            Sit_Reset();
            break;
        case reset_ID:
            Clear_State(2);
            break;    
        default:
            Action_ID = 0;
            break;
    }    
    Updated_Counter();
}

void set_motor_pos(int p1, int p2, int p3, int p4, int p5){
    motor[0].DesPos = motor[0].ZeroPos + p1;
    motor[1].DesPos = motor[1].ZeroPos + p2;
    motor[2].DesPos = motor[2].ZeroPos + p3;
    motor[3].DesPos = motor[3].ZeroPos + p4;
    motor[4].DesPos = motor[4].ZeroPos + p5;
}

void set_motor_angle(float angle1, float angle2, float angle3, float angle4, float angle5){ 
    int d1 = angle1/270.0*3000.0-1000;
    int d2 = -angle2/270.0*3000.0+1000;
    int d3 = angle3/270.0*3000.0-1000;
    int d4 = -angle4/270.0*3000.0+1000;
    int d5 = angle5/270.0*3000.0;
    set_motor_pos(d1, d2, d3, d4, d5);
}

void normal_state(){   
    vx = 0.0;
    vyaw = 0.0;
    control_mode = 0;
    set_motor_pos(-600, 600, -600, 600, 0);
    motor_speed = 1000;
}

void Updated_Counter(){	
	for(int i=0;i<ACTION_NUMBER;i++){
        if(i==Action_ID)
            Action_Counter[i]++;
        else
            Action_Counter[i] = 0;
    }

}

void Clear_State(uint8_t mode){
    normal_state();
    if (mode==0){
        ACTION_DONE = 0;
    }   
    if (mode==1){
        ACTION_DONE = 1;
        if(actionLoop_FLAG==0){            
            Action_ID = 0;
        }
    }

    if (mode == 2){
        actionLoop_FLAG = 0;
        ACTION_DONE = 0;
        Action_ID = 0;
    }  
}

void Wave(){
    float duration[] = {0.4, 1.6, 1.8, 0.4};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Wave_ID];
	if(counter==timepoint[0]){
		Clear_State(0);
        set_motor_pos(-100, +700, -600, 600, +200);
	}
	else if(counter>=timepoint[1] && counter<timepoint[2]){
        set_motor_pos(-100, -400, -600, 600, 400);
        motor_speed = 800;
	}else if(counter>=timepoint[2] && counter<timepoint[3]){
        motor_speed = 0;
        if(counter%30==0)
            set_motor_pos(-100, -200, -600, 600, 400);
        if(counter%30==15)
            set_motor_pos(-100, -600, -600, 600, 400);
    }else if(counter>timepoint[3] && counter<timepoint[4]){
        set_motor_pos(-300, 600, -600, 600, 200);
        motor_speed = 0;
	}
    else if(counter==timepoint[4]){
        Clear_State(1);
    }
}

void Naughty(){
	float duration[] = {2.0};
    uint16_t timepoint[2] = {0};
    
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Naughty_ID];
    float phase = counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 0;
        set_motor_pos(0, 0, 0, 0, 0);
	}
	else if(counter>timepoint[0] && counter<timepoint[1]){
        set_motor_pos(100 + 175*cos(4.5*phase), -100 + 175*cos(4.5*phase), 0, 0, -235*cos(4.5*phase));
	}else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Lookup(){
    float duration[] = {0.5, 1.0, 1.5, 1.0, 1.5};
    uint16_t timepoint[6] = {0};
    for(int i=1;i<6;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Lookup_ID];
    if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 0;
        set_motor_pos(-950, 950, -150, 150, 0);
    }
    else if(counter>timepoint[1] && counter<timepoint[2]){
        if(counter%40==0){
            set_motor_pos(-950, 950, -100 + 150, 100 + 150, 100);
        }else if(counter%40==20){
            set_motor_pos(-950, 950, -100 - 150, 100 - 150, -100);
        }
	}else if(counter>timepoint[2] && counter<timepoint[3]){
        set_motor_pos(-950, 950, -100, 100, 0);
    }else if(counter>timepoint[3] && counter<timepoint[4]){
        if(counter%40==0){
            set_motor_pos(-950, 950, -100 + 150, 100 + 150, 100);
        }else if(counter%40==20){
            set_motor_pos(-950, 950, -100 - 150, 100 - 150, -100);
        }
    }else if(counter>timepoint[4] && counter<timepoint[5]){
        set_motor_pos(-950, 950, -100, 100, 0);
    }else if(counter==timepoint[5]){
        Clear_State(1);
    }
}

void Swing(){
    float duration[] = {4.0};
    uint16_t timepoint[2] = {0};
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Swing_ID];
    float phase = counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 0;
        set_motor_pos(-700, 700, -700, 700, 0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        set_motor_pos(-700 - 350*sin(1.4*phase), 700 + 350*sin(1.4*phase), -700 + 350*sin(1.4*phase), 700 - 350*sin(1.4*phase), 0);
	}else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Rolling(){
    float duration[] = {6.0};
    uint16_t timepoint[2] = {0};
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Rolling_ID];
    float phase = 0.5*counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        motor_speed = 0;
        set_motor_pos(-700 - 370*sin(phase), 700 - 370*sin(phase), -700 + 370*sin(phase), 700 + 370*sin(phase), 0);
	}else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Angry(){
    float duration[] = {0.5, 2.0};
    uint16_t timepoint[3] = {0};
    for(int i=1;i<3;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Angry_ID];
    if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 0;
        set_motor_pos(0, 0, -1000, 1000, 0);
    }
    else if(counter>timepoint[1] && counter<timepoint[2]){
        if(counter%20==0){
            set_motor_pos(-100, -250, -1000, 1000, 0);
        }else if(counter%20==10){
            set_motor_pos(250, 100, -1000, 1000, 0);
        }
	}else if(counter==timepoint[2]){
        Clear_State(1);
    }
}

void Swimming(){
    float duration[] = {6.0};
    uint16_t timepoint[2] = {0};
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Swimming_ID];
    float phase = counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
        set_motor_pos(0, 0, 0, 0, 0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        motor_speed = 3000;
        set_motor_pos(200 + 300*sin(0.5*phase),
                     -200 + 300*sin(0.5*phase),
                      100*sin(3.0*phase),
                      100*sin(3.0*phase),
                     -200*sin(0.5*phase));
	}else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Pee(){
    float duration[] = {2.0, 1.0, 0.5, 1.0};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Pee_ID];
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 700;
        set_motor_pos(0, 700, -150, 500, -300);        
    }else if(counter>timepoint[1] && counter<timepoint[2]){
        motor_speed = 0;
        if(counter%30==0){
            set_motor_pos(0, 700, -50, 500, -300);
        }else if(counter%30==15){
            set_motor_pos(0, 700, -250, 500, -300);
        }                        
	}else if(counter>timepoint[3] && counter<timepoint[4]){
        if(counter%30==0){
            set_motor_pos(0, 700, -250, 500, -300);
        }else if(counter%30==15){
            set_motor_pos(0, 700, -50, 500, -300);
        }              
	}else if(counter==timepoint[4]){
        Clear_State(1);
    }
}

void Stretch(){
    float duration[] = {3.0, 1.5, 1.0};
    uint16_t timepoint[4] = {0};
    for(int i=1;i<4;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Stretch_ID];
    float phase = counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 3000;        
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        set_motor_pos(-400 + 2.2*counter, 400 - 2.2*counter, -750 - 1.8*counter, 750 + 1.8*counter, 0);
    }else if(counter>timepoint[1] && counter<timepoint[2]){
        set_motor_pos(-900, 900, 100, -100, 0);
	}else if(counter>timepoint[2] && counter<timepoint[3]){
        motor_speed = 5000;
        set_motor_pos(-900 + 100*cos(phase+3*PI/4.0),
                       900 + 100*cos(phase+3*PI/4.0),
                       100,
                       -100,
                       100*cos(phase+PI/4.0));
    }else if(counter==timepoint[3]){
        Clear_State(1);
    }
}

void Bouncing(){
    float duration[] = {5.0};
    uint16_t timepoint[2] = {0};
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Bouncing_ID];
    float phase = counter*2.2*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        motor_speed = 0;
        set_motor_pos(-500 - 450*sin(phase),
                       500 + 450*sin(phase),
                       -500 - 450*sin(phase),
                       500 + 450*sin(phase),
                       0);
	}else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Shaking(){
    float duration[] = {5.0};
    uint16_t timepoint[2] = {0};
    for(int i=1;i<2;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Shaking_ID];
    float phase = 0.9*counter*2.0*PI/TS;
    if(counter==timepoint[0]){
        Clear_State(0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        motor_speed = 0;
        set_motor_pos(-500 + 300*sin(phase),
                       500 + 300*sin(phase),
                      -500 + 300*sin(phase),
                       500 + 300*sin(phase),
                       300*sin(phase));
    }else if(counter==timepoint[1]){
        Clear_State(1);
    }
}

void Sit(){
    float duration[] = {4.0, 0.5, 0.5, 0.5};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Sit_ID];
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 6000;
        set_motor_pos(-1000, 1000, -1400, 1400, 0);
    }else if(counter>timepoint[1] && counter<timepoint[2]){
        motor_speed = 4000;
        set_motor_pos(0, 0, -1500, 1500, 0);
    }else if(counter>timepoint[2] && counter<timepoint[3]){
        set_motor_pos(0, 0, 0, 1500, 0);
    }else if(counter>timepoint[3] && counter<timepoint[4]){
        set_motor_pos(0, 0, 0, 0, 0);
    }else if(counter==timepoint[4]){
        Clear_State(1);
    }
}

// 一直保持坐姿，不起身
void Keep_Sit(){
    float duration[] = {4.0};
    uint16_t timepoint[2] = {0};
    timepoint[1] = duration[0]*TS;
    
    uint16_t counter = Action_Counter[Keep_Sit_ID];
    if(counter==timepoint[0]){
        Clear_State(0);
        motor_speed = 6000;
        set_motor_pos(-1000, 1000, -1400, 1400, 0);
    }else if(counter>timepoint[1]){
        // 保持坐姿
        motor_speed = 4000;
        set_motor_pos(-1000, 1000, -1400, 1400, 0);
    }
    // 不调用 Clear_State(1)，一直保持坐姿
}

// 从坐姿起身（Sit动作的后半段）
void Sit_Reset(){
    float duration[] = {0.5, 0.5, 0.5};
    uint16_t timepoint[4] = {0};
    for(int i=1;i<4;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Sit_Reset_ID];
    if(counter==timepoint[0]){
        Clear_State(0);
        motor_speed = 4000;
        set_motor_pos(0, 0, -1500, 1500, 0);
    }else if(counter>timepoint[0] && counter<timepoint[1]){
        set_motor_pos(0, 0, 0, 1500, 0);
    }else if(counter>timepoint[1] && counter<timepoint[2]){
        set_motor_pos(0, 0, 0, 0, 0);
    }else if(counter==timepoint[3]){
        Clear_State(1);
    }
}

void Scratch(){
    float duration[] = {1.5, 2.0, 0.5, 2.0, 1.0, 0.5};
    uint16_t timepoint[7] = {0};
    for(int i=1;i<7;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Scratch_ID];
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 6000;
        set_motor_pos(-1000, 1000, -1400, 1400, 0);
    }else if(counter>timepoint[1] && counter<timepoint[2]){
        motor_speed = 0;
        if(counter%40==0){
            set_motor_pos(-550, 750, -1800, 1500, 100);
        }else if(counter%40==20){
            set_motor_pos(-550, 750, -2100, 1500, -100);
        }
    }else if(counter>timepoint[2] && counter<timepoint[3]){
        motor_speed = 3000;
        set_motor_pos(-550, 750, -1500, 1500, 100);
    }else if(counter>timepoint[3] && counter<timepoint[4]){
        motor_speed = 0;
        if(counter%40==0){
            set_motor_pos(-550, 750, -1800, 1500, 100);
        }else if(counter%40==20){
            set_motor_pos(-550, 750, -2100, 1500, -100);
        }
    }else if(counter>timepoint[4] && counter<timepoint[5]){
        motor_speed = 4000;
        set_motor_pos(0, 0, 0, 1400, 0);
    }
    else if(counter>timepoint[5] && counter<timepoint[6]){
        motor_speed = 4000;
        set_motor_pos(0, 0, 0, 0, 0);
    }  
    else if(counter==timepoint[6]){
        Clear_State(1);
    }
} 

void Hug(){
    float duration[] = {1.5, 2.0, 3.0, 0.5, 0.5, 0.5};
    uint16_t timepoint[7] = {0};
    for(int i=1;i<7;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Hug_ID];
    float phase = counter*2.0*PI/TS;
	if(counter==timepoint[0]){
		Clear_State(0);
        motor_speed = 6000;
        set_motor_pos(-1000, 1000, -1400, 1400, 0);
    }else if(counter==timepoint[1]){
        motor_speed = 800;
        set_motor_pos(-600, 600, -1150, 1150, 0);
    }else if(counter>timepoint[2] && counter<timepoint[3]){
        motor_speed = 1500;
        set_motor_pos(-450 + 300*sin(phase),
                       450 + 300*sin(phase),
                       -1150,
                       1150,
                       200*sin(phase));
    }else if(counter>timepoint[3] && counter<timepoint[4]){
        motor_speed = 4500;
        set_motor_pos(0, 0, -1700, 1700, 0);
    }else if(counter>timepoint[4] && counter<timepoint[5]){
        set_motor_pos(0, 0, 0, 1700, 0);
    }else if(counter>timepoint[5] && counter<timepoint[6]){
        set_motor_pos(0, 0, 0, 0, 0);
    }else if(counter==timepoint[6]){
        Clear_State(1);
    }
}