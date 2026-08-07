/**
 * RIG-Arm CLIK 逆运动学 — ESP32-S3 优化版
 *
 * 与 rig_arm_ik.py 算法一致:
 *   - 简化运动链 FK + 解析几何雅可比
 *   - 加权 DLS (Sciavicco)
 *   - 热启动 q_prev；冷启动几何 seed
 *
 * 集成说明 (ESP32-S3 @ 240MHz):
 *   - 必须以 .cpp 编译，链接 libm
 *   - 建议编译选项: -O3 -ffast-math
 *   - 调用栈 >= 2 KB；无堆分配
 *   - 热启动典型 5~15 次迭代, < 1 ms
 *   - 冷启动 (双 seed) 通常 < 3 ms
 *
 * 关节链:
 *   Tz(H0) → Rz(q0) → Ry(q1) → Tz(L1) → Ry(q2) → Tz(L2) → Rz(q3) → Ry(-q4) → T(ee)
 */

#ifndef RIG_ARM_IK_H
#define RIG_ARM_IK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIG_ARM_IK_N 5

typedef struct {
    float q_prev[RIG_ARM_IK_N];
    bool has_prev;
    float lam;
} RigArmIK;

/** 初始化求解器，lam 为 DLS 阻尼 (默认 0.05) */
void rig_arm_ik_init(RigArmIK *ik, float lam);

/** 清除热启动状态 */
void rig_arm_ik_reset(RigArmIK *ik);

/**
 * 六维位姿 IK
 *
 * @param x,y,z           末端位置 (m, world)
 * @param roll,pitch,yaw  固定轴 RPY (rad): R = Rz(yaw)*Ry(pitch)*Rx(roll)
 * @param w               位置权重 ∈ [0,1]；姿态权重 = 1-w
 * @param q_out           输出关节角 (rad), 长度 5
 * @param pos_err         输出位置误差 (m)，可 NULL
 * @param ori_err         输出姿态误差 (rad)，可 NULL
 * @return true 收敛 (pos_err < 20mm)
 */
bool rig_arm_ik_solve(
    RigArmIK *ik,
    float x, float y, float z,
    float roll, float pitch, float yaw,
    float w,
    float q_out[RIG_ARM_IK_N],
    float *pos_err,
    float *ori_err);

/** 正运动学：仅输出末端位姿 */
void rig_arm_fk(const float q[RIG_ARM_IK_N], float p[3], float R[9]);

#ifdef __cplusplus
}
#endif

#endif /* RIG_ARM_IK_H */
