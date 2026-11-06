#ifndef MEDIA_RAW_WB_H
#define MEDIA_RAW_WB_H

/*
 * RAW_WB（RAW16_LE Bayer 手动白平衡）外部模块 API，由 libmedia_raw_wb.so 提供。
 * 应用包含本头文件并链接模块库；实例名 "RAW_WB_<grp>"，端口 input0 / output0。
 * 本头文件沿用旧模块的 MEDIA_RAW_WB_* 源码接口；插件 type 名为 "RAW_WB"。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_RAW_WB_TYPE "RAW_WB"

/* RAW_WB 创建参数。字段顺序与旧 include/media_api.h 保持一致。 */
typedef struct {
    int width;                       /* 有效图像宽，单位像素，范围 1..8192 */
    int height;                      /* 有效图像高，单位像素，范围 1..8192 */
    int input_depth;                 /* 输入端口队列深度，范围 1..64 */
    int output_pool_id;              /* 应用创建的外部 RAW16 输出 Pool ID */
    int input_stride;                /* 输入每行字节数：64 字节对齐且不小于 width*2 */
    int output_stride;               /* 输出每行字节数：64 字节对齐且不小于 width*2 */
    MEDIA_BAYER_PATTERN bayer_pattern; /* CFA 排列：BGGR/GBRG/GRBG/RGGB */
    float r_gain;                    /* 红色通道线性增益，有限且不小于 0 */
    float g_gain;                    /* 绿色通道线性增益，有限且不小于 0 */
    float b_gain;                    /* 蓝色通道线性增益，有限且不小于 0 */
} MEDIA_RAW_WB_ATTR;

/* RAW_WB 运行统计。字段顺序与旧 include/media_api.h 保持一致。 */
typedef struct {
    uint64_t frame_count;            /* 已成功送出的输出帧总数 */
    uint64_t process_count;          /* 已完成的处理次数（含丢弃） */
    uint64_t process_fail_count;     /* 处理失败次数 */
    uint64_t drop_count;             /* 因元数据、Pool 或 GPU 错误丢弃的帧数 */
    double kernel_last_ms;           /* 最近一次 GPU 提交等待耗时（毫秒） */
    double kernel_avg_ms;            /* GPU 提交等待平均耗时（毫秒） */
    double process_last_ms;          /* 最近一次端到端处理耗时（毫秒） */
    double process_avg_ms;           /* 端到端处理平均耗时（毫秒） */
    double process_max_ms;           /* 端到端处理最大耗时（毫秒） */
    uint64_t dmabuf_import_count;    /* 实际 EGLImage 导入次数 */
    uint64_t import_cache_hit_count; /* EGLImage 缓存命中次数 */
    uint64_t import_cache_miss_count;/* EGLImage 缓存未命中次数 */
    uint64_t import_cache_entries;   /* 当前缓存表项数 */
    uint64_t last_frame_import_count;/* 最近一帧新建导入数 */
    uint64_t max_frame_import_count; /* 单帧新建导入峰值 */
    uint64_t cpu_fallback_count;     /* GLES 插件的 CPU 回退次数，固定为 0 */
    uint64_t copy_count;             /* CPU 逐帧复制次数，固定为 0 */
    uint64_t map_count;              /* CPU 映射次数，固定为 0 */
    uint64_t unmap_count;            /* CPU 解映射次数，固定为 0 */
    uint64_t readback_count;         /* GPU 回读次数，固定为 0 */
    uint64_t output_completion_wait_count; /* GPU 完成等待次数 */
    uint64_t strong_sync_count;      /* fence 超时后强制同步次数 */
} MEDIA_RAW_WB_PERF;

int MEDIA_RAW_WB_Create(int grp, const MEDIA_RAW_WB_ATTR *attr);
int MEDIA_RAW_WB_Destroy(int grp);
int MEDIA_RAW_WB_Start(int grp);
int MEDIA_RAW_WB_Stop(int grp);
int MEDIA_RAW_WB_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_RAW_WB_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_RAW_WB_GetLastPerf(int grp, MEDIA_RAW_WB_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_RAW_WB_H */

