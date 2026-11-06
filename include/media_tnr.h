#ifndef MEDIA_TNR_H
#define MEDIA_TNR_H

/* TNR_CL（GLES 时域降噪）外部模块 API。 */
#include <stdint.h>
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建 TNR_CL_<grp> 实例时使用的配置。 */
typedef struct {
    int width;          /* 输入和输出的有效宽度，必须大于 0。 */
    int height;         /* 输入和输出的有效高度，必须大于 0。 */
    int format;         /* 当前 GLES 实现支持 MEDIA_FORMAT_NV12。 */
    int input_depth;    /* input0 队列深度，<=0 使用框架默认值。 */
    int output_pool_id; /* 输出 Buffer Pool；小于 0 时复用输入 Pool。 */
    int input_stride;   /* 输入 plane0 pitch；0 表示使用逐帧 metadata。 */
    int output_stride;  /* 输出 plane0 pitch；0 表示 QueryLayout 的 canonical pitch。 */
    int block_size;     /* 运动分块大小，保留兼容字段；<=0 使用 16。 */
    float threshold;    /* 运动阈值；大于 1 的值按 8 位码值除以 255。 */
    float static_alpha; /* 低运动区域采用当前帧的权重，范围 [0,1]。 */
    float motion_alpha; /* 高运动区域采用当前帧的权重，范围 [0,1]。 */
    int passthrough;    /* 非零时直接转发输入帧，不执行降噪。 */
} MEDIA_TNR_CL_ATTR;

/* 最近一次处理的 GPU 统计。 */
typedef struct {
    double gpu_motion_ms;      /* GLES 绘制耗时；兼容旧字段名。 */
    double gpu_blend_ms;       /* GLES 绘制耗时；兼容旧字段名。 */
    double gpu_queue_total_ms; /* 从提交到 Finish 的总耗时。 */
    int has_prev;              /* 是否已经建立上一帧历史。 */
} MEDIA_TNR_CL_PERF;

#define MEDIA_TNR_CL_TYPE "TNR_CL"

int MEDIA_TNR_CL_CreateGrp(int grp, const MEDIA_TNR_CL_ATTR *attr);
int MEDIA_TNR_CL_DestroyGrp(int grp);
int MEDIA_TNR_CL_Start(int grp);
int MEDIA_TNR_CL_Stop(int grp);
int MEDIA_TNR_CL_Enable(int grp);
int MEDIA_TNR_CL_Disable(int grp);
int MEDIA_TNR_CL_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_TNR_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_TNR_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_TNR_CL_SetPassthrough(int grp, int enable);
int MEDIA_TNR_CL_SetThreshold(int grp, float threshold);
int MEDIA_TNR_CL_SetThresholdRange(int grp, float threshold_low, float threshold_high);
int MEDIA_TNR_CL_SetStaticAlpha(int grp, float alpha);
int MEDIA_TNR_CL_SetMotionAlpha(int grp, float alpha);
int MEDIA_TNR_CL_SetSpatialStrength(int grp, float strength);
int MEDIA_TNR_CL_GetLastPerf(int grp, MEDIA_TNR_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_TNR_H */
