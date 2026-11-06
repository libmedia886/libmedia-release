#ifndef MEDIA_EDOF_H
#define MEDIA_EDOF_H

/*
 * EDOF_CL（扩展景深：双焦面图像融合，GLES 实现）外部模块 API，由 libmedia_edof.so 提供。
 *
 * 旧内置模块 src/module/edof_cl 的 MEDIA_EDOF_CL_* 源码级兼容入口：包含本头文件并链接
 * libmedia_edof.so 即可，函数名、参数列表、MEDIA_EDOF_CL_ATTR / MEDIA_EDOF_CL_PERF 的
 * 结构体名与字段顺序都与旧 master:include/media_api.h 完全一致（只追加了新的宏，没有改动
 * 结构体）。需要新字段时只能追加在结构体末尾。
 *
 * 模块 type 名为 "EDOF_CL"，实例名为 "EDOF_CL_<grp>"；输入端口 input0 / input1，
 * 输出端口 output0，三者都是 NV12。应用侧可以直接
 *   MEDIA_SYS_Bind("TPG", 0, "output0", "EDOF_CL", grp, "input0")
 * 之类的绑定方式接入，也可以 MEDIA_SYS_SendFrame 手动送帧。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MEDIA_SYS_Bind / MEDIA_PLUGIN_CreateModule 使用的模块类型名（沿用旧内置模块）。 */
#define MEDIA_EDOF_CL_TYPE "EDOF_CL"
/* focus_radius <= 0 时使用的默认窗口半径（与旧模块一致）。 */
#define MEDIA_EDOF_CL_DEFAULT_FOCUS_RADIUS 4
/* input_depth <= 0 时使用的默认输入队列深度（与旧模块一致）。 */
#define MEDIA_EDOF_CL_DEFAULT_INPUT_DEPTH 4
/* score_eps <= 0 时使用的默认权重分母保护值（与旧模块一致）。 */
#define MEDIA_EDOF_CL_DEFAULT_SCORE_EPS 1e-3f
/*
 * 窗口半径上限。旧模块 DMI 只要设备 local memory 放得下就允许更大值；GLES 版本用
 * 两个方向各 (2r+1) 次采样完成盒式求和，超过 32 后耗时随 r 线性增长且收益很小，
 * 因此创建时拒绝更大的值。
 */
#define MEDIA_EDOF_CL_MAX_FOCUS_RADIUS 32

/*
 * EDOF_CL 配置。字段顺序与旧 master:include/media_api.h 完全一致。
 *
 * 输入语义：两路输入分别是同一个场景在两个焦面上的成像，input0 视为“主帧”，
 * 输出的时间戳 / 序号取自它。
 */
typedef struct {
    int width;            /* 图像宽度（像素）；必须为偶数，且与两路输入 / 输出端口一致。 */
    int height;           /* 图像高度（像素）；必须为偶数，且与两路输入 / 输出端口一致。 */
    int format;           /* 逻辑像素格式；当前只支持 MEDIA_FORMAT_NV12。 */
    int focus_radius;     /* 清晰度统计窗口半径（像素）：窗口边长 2*r+1；<=0 使用 4，范围 1..32。 */
    int input_depth;      /* 两路输入端口的 Bind 队列深度；<=0 使用 4。 */
    int output_pool_id;   /* 非负；输出 Buffer 从该 Pool 申请，Pool 必须能容纳一帧 NV12。 */
    int input_stride;     /* 输入 Y/UV 平面字节 pitch；0 表示按格式推导，非零须 >= width 且 64 对齐。 */
    int output_stride;    /* 输出 Y/UV 平面字节 pitch；0 表示按格式推导，非零须 >= width 且 64 对齐。 */
    float score_eps;      /* 权重分母保护值，避免两路都极清晰时除零；<=0 使用 1e-3。 */
} MEDIA_EDOF_CL_ATTR;

/*
 * 最近一次处理的性能统计。旧模块由 OpenCL event 计时；GLES 版本没有逐 kernel 事件，
 * 字段含义见 README / MIGRATION_NOTES：
 *   gpu_kernel_total_ms —— 最近一次处理从首趟绘制到 MEDIA_GPU_Finish 完成的墙钟时间（毫秒），
 *                          没有成功处理过任何帧时为 -1.0；
 *   gpu_queue_total_ms  —— 旧模块的 acquire→release 事件跨度，GLES 后端没有对应物，恒为 -1.0。
 */
typedef struct {
    double gpu_kernel_total_ms;
    double gpu_queue_total_ms;
} MEDIA_EDOF_CL_PERF;

/* 创建 / 销毁实例；grp 与实例名 "EDOF_CL_<grp>" 一一对应。 */
int MEDIA_EDOF_CL_CreateGrp(int grp, const MEDIA_EDOF_CL_ATTR *attr);
int MEDIA_EDOF_CL_DestroyGrp(int grp);

/* 启停处理线程（Start/Stop 与 Enable/Disable 等价，沿用旧 API 语义）。 */
int MEDIA_EDOF_CL_Start(int grp);
int MEDIA_EDOF_CL_Stop(int grp);
int MEDIA_EDOF_CL_Enable(int grp);
int MEDIA_EDOF_CL_Disable(int grp);

/* 从 output0 取一帧 / 归还一帧，timeout_ms 语义与其它模块一致（-1 一直等，0 不等）。 */
int MEDIA_EDOF_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_EDOF_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);

/*
 * 设置 input1 相对 input0 的仿射配准矩阵，行优先 6 个浮点：
 *   x1 = m[0]*x0 + m[1]*y0 + m[2]
 *   y1 = m[3]*x0 + m[4]*y0 + m[5]
 * 运行中可随时调用，从下一帧开始生效；调用一次后 warp 一直保持生效（has_warp 不复位，
 * 与旧模块一致）。旧 OpenCL 的线性采样在非整数坐标处插值，因此即使传单位矩阵
 * {1,0,0,0,1,0}，input1 也会经过一次半像素的双线性重采样，只是没有几何位移。
 * 要回到完全不重采样 input1 的状态需要重建实例。matrix 为 NULL 返回 MEDIA_ERR_INVALID_PARAM。
 */
int MEDIA_EDOF_CL_SetAffineWarp(int grp, float matrix[6]);

/* 读取最近一次处理性能；perf 为 NULL 返回 MEDIA_ERR_INVALID_PARAM。 */
int MEDIA_EDOF_CL_GetLastPerf(int grp, MEDIA_EDOF_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_EDOF_H */

