/**
 * RIG-Arm CLIK — ESP32-S3 实现
 *
 * 性能要点:
 *   1. R+p 递推 FK，避免 4×4 矩阵乘法
 *   2. 同次遍历输出关节原点/轴向 → 解析雅可比 (无有限差分)
 *   3. 6×6 SPD 系统 Cholesky 求解 DLS
 *   4. 热启动 + 几何 seed 冷启动
 *
 * 移植: 复制 ik.h + ik.cpp 到 firmware，Arduino 可加 IRAM_ATTR 到 fk_chain / dls_step
 */

 #include "ik.h"

 #include <math.h>
 #include <string.h>
 #define PI 3.14159
 namespace {
 
 constexpr int N = RIG_ARM_IK_N;
 constexpr float H0 = 0.053f;
 constexpr float L1 = 0.090f;
 constexpr float L2 = 0.090f;
 constexpr float EE_X = 0.035f;
 constexpr float EE_Y = -0.01f;
 constexpr float EE_Z = 0.025f;
 
 constexpr float DEFAULT_LAM = 0.05f;
 constexpr int MAX_ITER_WARM = 40;   /* 与 rig_arm_ik.py 一致 */
 constexpr int MAX_ITER_COLD = 60;
 constexpr float POS_TOL = 1e-4f;
 constexpr float ORI_TOL = 5e-3f;
 constexpr float OK_POS = 0.02f;
 
 constexpr float LIM[N][2] = {
     {-2.62f, 2.62f},
     {-1.57f, 1.57f},
     {-0.50f, 2.50f},
     {-1.50f, 1.50f},
     {-1.20f, 1.30f},
 };
 
 struct Vec3 {
     float x, y, z;
 };
 
 struct Mat3 {
     float m[3][3];
 };
 
 static inline float clampf(float v, float lo, float hi) {
     if (v < lo) return lo;
     if (v > hi) return hi;
     return v;
 }
 
 static inline Vec3 vec3(float x, float y, float z) {
     return {x, y, z};
 }
 
 static inline Vec3 vadd(Vec3 a, Vec3 b) {
     return {a.x + b.x, a.y + b.y, a.z + b.z};
 }
 
 static inline Vec3 vsub(Vec3 a, Vec3 b) {
     return {a.x - b.x, a.y - b.y, a.z - b.z};
 }
 
 static inline Vec3 vscale(Vec3 a, float s) {
     return {a.x * s, a.y * s, a.z * s};
 }
 
 static inline Vec3 vcross(Vec3 a, Vec3 b) {
     return {
         a.y * b.z - a.z * b.y,
         a.z * b.x - a.x * b.z,
         a.x * b.y - a.y * b.x,
     };
 }
 
 static inline float vnorm(Vec3 a) {
     return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
 }
 
 static Mat3 mat3Identity() {
     Mat3 R{};
     R.m[0][0] = R.m[1][1] = R.m[2][2] = 1.0f;
     return R;
 }
 
 static Vec3 mat3Col(const Mat3 &R, int c) {
     return {R.m[0][c], R.m[1][c], R.m[2][c]};
 }
 
 /** R_new = R * Rz(a)  — 与 _rot_z 右乘一致 */
 static Mat3 mat3MulRz(const Mat3 &R, float a) {
     float c = cosf(a), s = sinf(a);
     Mat3 O{};
     for (int i = 0; i < 3; ++i) {
         O.m[i][0] = R.m[i][0] * c + R.m[i][1] * s;
         O.m[i][1] = -R.m[i][0] * s + R.m[i][1] * c;
         O.m[i][2] = R.m[i][2];
     }
     return O;
 }
 
 /** R_new = R * Ry(a)  — 与 _rot_y 右乘一致 */
 static Mat3 mat3MulRy(const Mat3 &R, float a) {
     float c = cosf(a), s = sinf(a);
     Mat3 O{};
     for (int i = 0; i < 3; ++i) {
         O.m[i][0] = R.m[i][0] * c - R.m[i][2] * s;
         O.m[i][1] = R.m[i][1];
         O.m[i][2] = R.m[i][0] * s + R.m[i][2] * c;
     }
     return O;
 }
 
 /** p += R * (0,0,tz) */
 static void translateLocalZ(Vec3 &p, const Mat3 &R, float tz) {
     p.x += R.m[0][2] * tz;
     p.y += R.m[1][2] * tz;
     p.z += R.m[2][2] * tz;
 }
 
 /** p += R * v */
 static void translateLocal(Vec3 &p, const Mat3 &R, Vec3 v) {
     p.x += R.m[0][0] * v.x + R.m[0][1] * v.y + R.m[0][2] * v.z;
     p.y += R.m[1][0] * v.x + R.m[1][1] * v.y + R.m[1][2] * v.z;
     p.z += R.m[2][0] * v.x + R.m[2][1] * v.y + R.m[2][2] * v.z;
 }
 
 static Mat3 rpyToRot(float roll, float pitch, float yaw) {
     float cr = cosf(roll), sr = sinf(roll);
     float cp = cosf(pitch), sp = sinf(pitch);
     float cy = cosf(yaw), sy = sinf(yaw);
 
     Mat3 Rx{}, Ry{}, Rz{};
     Rx.m[0][0] = 1.0f;
     Rx.m[1][1] = cr;
     Rx.m[1][2] = -sr;
     Rx.m[2][1] = sr;
     Rx.m[2][2] = cr;
 
     Ry.m[0][0] = cp;
     Ry.m[0][2] = sp;
     Ry.m[1][1] = 1.0f;
     Ry.m[2][0] = -sp;
     Ry.m[2][2] = cp;
 
     Rz.m[0][0] = cy;
     Rz.m[0][1] = -sy;
     Rz.m[1][0] = sy;
     Rz.m[1][1] = cy;
     Rz.m[2][2] = 1.0f;
 
     /* R = Rz * Ry * Rx */
     Mat3 T{};
     for (int i = 0; i < 3; ++i) {
         for (int j = 0; j < 3; ++j) {
             T.m[i][j] = Rz.m[i][0] * Ry.m[0][j] + Rz.m[i][1] * Ry.m[1][j] + Rz.m[i][2] * Ry.m[2][j];
         }
     }
     Mat3 R{};
     for (int i = 0; i < 3; ++i) {
         for (int j = 0; j < 3; ++j) {
             R.m[i][j] = T.m[i][0] * Rx.m[0][j] + T.m[i][1] * Rx.m[1][j] + T.m[i][2] * Rx.m[2][j];
         }
     }
     return R;
 }
 
 static Mat3 mat3Mul(const Mat3 &A, const Mat3 &B) {
     Mat3 C{};
     for (int i = 0; i < 3; ++i) {
         for (int j = 0; j < 3; ++j) {
             C.m[i][j] = A.m[i][0] * B.m[0][j] + A.m[i][1] * B.m[1][j] + A.m[i][2] * B.m[2][j];
         }
     }
     return C;
 }
 
 static Mat3 mat3Transpose(const Mat3 &A) {
     Mat3 T{};
     for (int i = 0; i < 3; ++i) {
         for (int j = 0; j < 3; ++j) {
             T.m[i][j] = A.m[j][i];
         }
     }
     return T;
 }
 
 static void clampQ(float q[N]) {
     for (int i = 0; i < N; ++i) {
         q[i] = clampf(q[i], LIM[i][0], LIM[i][1]);
     }
 }
 
 /**
  * 单次遍历: origins[5], axes[5], ee_pos, ee_rot
  * 与 rig_arm_ik._fk_chain 一致
  */
 static void fkChain(
     const float q[N],
     Vec3 origins[N],
     Vec3 axes[N],
     Vec3 &p_ee,
     Mat3 &R_ee) {
     Mat3 R = mat3Identity();
     Vec3 p = vec3(0.0f, 0.0f, H0);
 
     origins[0] = p;
     axes[0] = mat3Col(R, 2);
     R = mat3MulRz(R, q[0]);
 
     origins[1] = p;
     axes[1] = mat3Col(R, 1);
     R = mat3MulRy(R, q[1]);
     translateLocalZ(p, R, L1);
 
     origins[2] = p;
     axes[2] = mat3Col(R, 1);
     R = mat3MulRy(R, q[2]);
     translateLocalZ(p, R, L2);
 
     origins[3] = p;
     axes[3] = mat3Col(R, 2);
     R = mat3MulRz(R, q[3]);
 
     origins[4] = p;
     axes[4] = vscale(mat3Col(R, 1), -1.0f);
     R = mat3MulRy(R, -q[4]);
     translateLocal(p, R, vec3(EE_X, EE_Y, EE_Z));
 
     p_ee = p;
     R_ee = R;
 }
 
 static Vec3 orientationError(const Mat3 &R_des, const Mat3 &R_cur) {
   /* e = 0.5 * vee(R_des * R_cur^T - (..)^T) */
     Mat3 Rt = mat3Transpose(R_cur);
     Mat3 E = mat3Mul(R_des, Rt);
     return vec3(
         0.5f * (E.m[2][1] - E.m[1][2]),
         0.5f * (E.m[0][2] - E.m[2][0]),
         0.5f * (E.m[1][0] - E.m[0][1]));
 }
 
 static void jacobian(
     const Vec3 origins[N],
     const Vec3 axes[N],
     const Vec3 &p_ee,
     float J[6][N]) {
     for (int i = 0; i < N; ++i) {
         Vec3 r = vsub(p_ee, origins[i]);
         Vec3 v = vcross(axes[i], r);
         J[0][i] = v.x;
         J[1][i] = v.y;
         J[2][i] = v.z;
         J[3][i] = axes[i].x;
         J[4][i] = axes[i].y;
         J[5][i] = axes[i].z;
     }
 }
 
 /** 6×6 Cholesky 分解并求解 A x = b (A 对称正定) */
 static bool choleskySolve6(float A[6][6], const float b[6], float x[6]) {
     float L[6][6]{};
 
     for (int i = 0; i < 6; ++i) {
         for (int j = 0; j <= i; ++j) {
             float s = A[i][j];
             for (int k = 0; k < j; ++k) {
                 s -= L[i][k] * L[j][k];
             }
             if (i == j) {
                 if (s <= 1e-12f) {
                     return false;
                 }
                 L[i][j] = sqrtf(s);
             } else {
                 L[i][j] = s / L[j][j];
             }
         }
     }
 
     float y[6];
     for (int i = 0; i < 6; ++i) {
         float s = b[i];
         for (int k = 0; k < i; ++k) {
             s -= L[i][k] * y[k];
         }
         y[i] = s / L[i][i];
     }
 
     for (int i = 5; i >= 0; --i) {
         float s = y[i];
         for (int k = i + 1; k < 6; ++k) {
             s -= L[k][i] * x[k];
         }
         x[i] = s / L[i][i];
     }
     return true;
 }
 
 /** dq = J^T W (W J J^T W + λ²I)⁻¹ W e */
 static void dlsStep(
     const float J[6][N],
     const float e[6],
     float w_pos,
     float w_ori,
     float lam,
     float dq[N]) {
     float wp = w_pos;
     float wo = w_ori;
     float lam2 = lam * lam;
 
     float Jw[6][N];
     float ew[6];
     for (int i = 0; i < 3; ++i) {
         ew[i] = wp * e[i];
         for (int j = 0; j < N; ++j) {
             Jw[i][j] = wp * J[i][j];
         }
     }
     for (int i = 3; i < 6; ++i) {
         ew[i] = wo * e[i];
         for (int j = 0; j < N; ++j) {
             Jw[i][j] = wo * J[i][j];
         }
     }
 
     float A[6][6]{};
     for (int i = 0; i < 6; ++i) {
         for (int j = i; j < 6; ++j) {
             float s = 0.0f;
             for (int k = 0; k < N; ++k) {
                 s += Jw[i][k] * Jw[j][k];
             }
             A[i][j] = s;
             A[j][i] = s;
         }
         A[i][i] += lam2;
     }
 
     float v[6];
     if (!choleskySolve6(A, ew, v)) {
         for (int j = 0; j < N; ++j) {
             dq[j] = 0.0f;
         }
         return;
     }
 
     for (int j = 0; j < N; ++j) {
         float s = 0.0f;
         for (int i = 0; i < 3; ++i) {
             s += J[i][j] * (wp * v[i]);
         }
         for (int i = 3; i < 6; ++i) {
             s += J[i][j] * (wo * v[i]);
         }
         dq[j] = s;
     }
 }
 
 static void geometricSeed(float x, float y, float z, float elbow, float q[N]) {
     float q0 = atan2f(y, x);
     float rho = sqrtf(x * x + y * y);
     float dz = z - H0 - EE_Z;
     float d2 = rho * rho + dz * dz;
     float d = sqrtf(d2);
     const float r_max = L1 + L2;
     const float r_min = fabsf(L1 - L2);
 
     if (d > r_max) {
         float s = r_max / fmaxf(d, 1e-12f);
         rho *= s;
         dz *= s;
         d2 = rho * rho + dz * dz;
     } else if (d < r_min && d > 1e-9f) {
         float s = r_min / d;
         rho *= s;
         dz *= s;
         d2 = rho * rho + dz * dz;
     }
 
     float cos2 = (d2 - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
     cos2 = clampf(cos2, -1.0f, 1.0f);
     float s2 = sqrtf(fmaxf(0.0f, 1.0f - cos2 * cos2)) * elbow;
     float q2 = atan2f(s2, cos2);
     /* atan2(rho, dz+H0) == atan2(rho, z - EE_Z)，与 rig_arm_ik._seed 一致 */
     float q1 = atan2f(rho, dz + H0) - atan2f(L2 * s2, L1 + L2 * cos2);
 
     q[0] = q0;
     q[1] = q1;
     q[2] = q2;
     q[3] = 0.0f;
     q[4] = 0.0f;
     clampQ(q);
 }
 
 static void clikIterate(
     const float q_init[N],
     const Vec3 &p_des,
     const Mat3 &R_des,
     int max_iter,
     float w_pos,
     float w_ori,
     float lam,
     float pos_goal,
     float q_out[N],
     float *pe_out,
     float *oe_out) {
     float q[N];
     memcpy(q, q_init, sizeof(q));
     clampQ(q);
 
     float pe = 1e9f;
     float oe = 1e9f;
 
     for (int k = 0; k < max_iter; ++k) {
         Vec3 origins[N], axes[N], p;
         Mat3 R;
         fkChain(q, origins, axes, p, R);
 
         Vec3 e_pos = vsub(p_des, p);
         Vec3 e_rot = orientationError(R_des, R);
         pe = vnorm(e_pos);
         oe = vnorm(e_rot);
 
         if (k > 2 && pe < pos_goal && oe < ORI_TOL) {
             break;
         }
         if (pe < POS_TOL && oe < ORI_TOL) {
             break;
         }
 
         float J[6][N];
         jacobian(origins, axes, p, J);
 
         float e[6] = {e_pos.x, e_pos.y, e_pos.z, e_rot.x, e_rot.y, e_rot.z};
         float dq[N];
         dlsStep(J, e, w_pos, w_ori, lam, dq);
 
         for (int i = 0; i < N; ++i) {
             q[i] += dq[i];
         }
         clampQ(q);
     }
 
     memcpy(q_out, q, sizeof(q));
     if (pe_out) *pe_out = pe;
     if (oe_out) *oe_out = oe;
 }
 
 static void coldStart(
     RigArmIK *ik,
     float x, float y, float z,
     const Vec3 &p_des,
     const Mat3 &R_des,
     float w_pos,
     float w_ori,
     float q_best[N],
     float *best_pe,
     float *best_oe) {
     float seeds[4][N];
     int n_seeds = 0;
 
     if (ik->has_prev) {
         memcpy(seeds[n_seeds++], ik->q_prev, sizeof(float) * N);
     }
     geometricSeed(x, y, z, 1.0f, seeds[n_seeds++]);
     geometricSeed(x, y, z, -1.0f, seeds[n_seeds++]);
     memset(seeds[n_seeds], 0, sizeof(float) * N);
     clampQ(seeds[n_seeds++]);
 
     float best_cost = 1e9f;
     float bp = 1e9f, bo = 1e9f;
     float qb[N];
 
     for (int s = 0; s < n_seeds; ++s) {
         float pe, oe;
         clikIterate(seeds[s], p_des, R_des, MAX_ITER_COLD, w_pos, w_ori, ik->lam, POS_TOL, qb, &pe, &oe);
         float cost = w_pos * pe + w_ori * oe;
         if (cost < best_cost) {
             best_cost = cost;
             bp = pe;
             bo = oe;
             memcpy(q_best, qb, sizeof(qb));
         }
     }
 
     if (best_pe) *best_pe = bp;
     if (best_oe) *best_oe = bo;
 }
 
 }  // namespace
 
 void rig_arm_ik_init(RigArmIK *ik, float lam) {
     memset(ik, 0, sizeof(*ik));
     ik->lam = (lam > 0.0f) ? lam : DEFAULT_LAM;
 }
 
 void rig_arm_ik_reset(RigArmIK *ik) {
     ik->has_prev = false;
     memset(ik->q_prev, 0, sizeof(ik->q_prev));
 }
 
 void rig_arm_fk(const float q[N], float p[3], float R_out[9]) {
     Vec3 origins[N], axes[N], p_ee;
     Mat3 R;
     fkChain(q, origins, axes, p_ee, R);
     p[0] = p_ee.x;
     p[1] = p_ee.y;
     p[2] = p_ee.z;
     for (int i = 0; i < 3; ++i) {
         for (int j = 0; j < 3; ++j) {
             R_out[i * 3 + j] = R.m[i][j];
         }
     }
 }
 
 bool rig_arm_ik_solve(
     RigArmIK *ik,
     float x, float y, float z,
     float roll, float pitch, float yaw,
     float w,
     float q_out[N],
     float *pos_err,
     float *ori_err) {
     if (ik == NULL || q_out == NULL) {
         return false;
     }
 
     w = clampf(w, 0.0f, 1.0f);
     float w_pos = w;
     float w_ori = 1.0f - w;
 
     Vec3 p_des = vec3(x, y, z);
     Mat3 R_des = rpyToRot(roll, pitch, yaw);
 
     float q[N];
     float pe, oe;
 
     if (ik->has_prev) {
         clikIterate(ik->q_prev, p_des, R_des, MAX_ITER_WARM, w_pos, w_ori, ik->lam, OK_POS, q, &pe, &oe);
     } else {
         coldStart(ik, x, y, z, p_des, R_des, w_pos, w_ori, q, &pe, &oe);
     }
 
     if (pe > 0.05f) {
         coldStart(ik, x, y, z, p_des, R_des, w_pos, w_ori, q, &pe, &oe);
     }
 
     Vec3 origins[N], axes[N], p;
     Mat3 R;
     fkChain(q, origins, axes, p, R);
     Vec3 e_pos = vsub(p_des, p);
     Vec3 e_rot = orientationError(R_des, R);
     pe = vnorm(e_pos);
     oe = vnorm(e_rot);
 
     memcpy(q_out, q, sizeof(q));
     //转换角度，考虑安装方向
     q_out[0] = q[0]*180.0/PI;
     q_out[1] = q[1]*180.0/PI;
     q_out[2] = -q[2]*180.0/PI;
     q_out[3] = q[3]*180.0/PI;
     q_out[4] = -q[4]*180.0/PI;

     bool ok = pe < OK_POS;
     if (ok) {
         memcpy(ik->q_prev, q, sizeof(q));
         ik->has_prev = true;
     }
 
     if (pos_err) *pos_err = pe;
     if (ori_err) *ori_err = oe;
     return ok;
 }
 