#ifndef MEDIA_WAVELET_NR_H
#define MEDIA_WAVELET_NR_H

/*
 * WAVELET_NR_CL（NV12 GLES 小波空间降噪）外部模块 API。
 *
 * 本头文件与旧 libmedia 的 MEDIA_WAVELET_NR_CL_* API 源码兼容：结构体名、字段顺序、
 * 函数名与参数均保持不变。实例名为 "WAVELET_NR_CL_<grp>"，输入端口 input0，
 * 输出端口 output0，可直接 MEDIA_SYS_Bind("WAVELET_NR_CL", grp, "output0", ...)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 插件类型名，也是 MEDIA_SYS_Bind 的 mod 参数。 */
#define MEDIA_WAVELET_NR_CL_TYPE "WAVELET_NR_CL"

/* plane0 / plane1 pitch 的默认对齐字节数，与旧模块一致。 */
#define MEDIA_WAVELET_NR_CL_STRIDE_ALIGN 64
/* 输入端口默认队列深度。 */
#define MEDIA_WAVELET_NR_CL_DEFAULT_INPUT_DEPTH 4
/* 默认亮度软阈值（归一化 0..1）。 */
#define MEDIA_WAVELET_NR_CL_DEFAULT_THRESHOLD_Y (6.0f / 255.0f)
/* 默认高频收缩强度。 */
#define MEDIA_WAVELET_NR_CL_DEFAULT_STRENGTH 0.60f
/* 默认小波分解层数。 */
#define MEDIA_WAVELET_NR_CL_DEFAULT_LEVELS 1

typedef struct {
    int width;            /* 输入/输出图像宽度，必须 > 0 且为偶数 */
    int height;           /* 输入/输出图像高度，必须 > 0 且为偶数 */
    int format;           /* 像素格式，仅支持 MEDIA_FORMAT_NV12 */
    int input_depth;      /* 输入端口队列深度；<= 0 取默认 4 */
    int output_pool_id;   /* 输出 Buffer 池 ID；< 0 表示复用每帧输入所在池 */
    int input_stride;     /* 输入 plane0 字节 pitch；<= 0 按宽度 64 字节对齐推导 */
    int output_stride;    /* 输出 plane0/plane1 字节 pitch；<= 0 沿用输入 pitch */
    int levels;           /* 小波分解层数，允许 1..2；<= 0 取默认 1 */
    float threshold_y;    /* 亮度高频软阈值（归一化 0..1），<= 0 取 6/255 */
    float strength;       /* 亮度高频收缩强度 0..1，<= 0 取 0.60 */
    int uv_enable;        /* 非 0 时启用色度平滑，0 时色度直通 */
    float uv_strength;    /* 色度平滑混合强度 0..1，<= 0 不处理 */
    int passthrough;      /* 非 0 时跳过算法；同池输出为零拷贝转发 */
} MEDIA_WAVELET_NR_CL_ATTR;

typedef struct {
    double cpu_ms;        /* 单帧 CPU 侧总处理耗时（毫秒，-1 表示尚无数据） */
    double gpu_kernel_ms; /* 单帧提交绘制到 MEDIA_GPU_Finish 返回的耗时（毫秒） */
    double gpu_queue_ms;  /* 单帧 MEDIA_GPU_Finish 等待 GPU 完成的耗时（毫秒） */
    int gpu_enabled;      /* 1 表示当前实例已启用 GLES 后端 */
} MEDIA_WAVELET_NR_CL_PERF;

/* 返回旧模块使用的默认 pitch（按 MEDIA_WAVELET_NR_CL_STRIDE_ALIGN 对齐）。 */
static inline int MEDIA_WAVELET_NR_CL_DefaultStride(int width)
{
    if (width <= 0) {
        return 0;
    }
    return (width + MEDIA_WAVELET_NR_CL_STRIDE_ALIGN - 1) /
           MEDIA_WAVELET_NR_CL_STRIDE_ALIGN * MEDIA_WAVELET_NR_CL_STRIDE_ALIGN;
}

int MEDIA_WAVELET_NR_CL_CreateGrp(int grp, const MEDIA_WAVELET_NR_CL_ATTR *attr);
int MEDIA_WAVELET_NR_CL_DestroyGrp(int grp);
int MEDIA_WAVELET_NR_CL_Start(int grp);
int MEDIA_WAVELET_NR_CL_Stop(int grp);
int MEDIA_WAVELET_NR_CL_Enable(int grp);
int MEDIA_WAVELET_NR_CL_Disable(int grp);
int MEDIA_WAVELET_NR_CL_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_WAVELET_NR_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_WAVELET_NR_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_WAVELET_NR_CL_SetPassthrough(int grp, int enable);
int MEDIA_WAVELET_NR_CL_SetLevels(int grp, int levels);
int MEDIA_WAVELET_NR_CL_SetThresholdY(int grp, float threshold_y);
int MEDIA_WAVELET_NR_CL_SetStrength(int grp, float strength);
int MEDIA_WAVELET_NR_CL_SetUvEnable(int grp, int enable);
int MEDIA_WAVELET_NR_CL_SetUvStrength(int grp, float strength);
int MEDIA_WAVELET_NR_CL_GetLastPerf(int grp, MEDIA_WAVELET_NR_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_WAVELET_NR_H */
