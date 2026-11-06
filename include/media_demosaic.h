#ifndef MEDIA_DEMOSAIC_H
#define MEDIA_DEMOSAIC_H

/*
 * DEMOSAIC 外部模块 API：RAW16_LE Bayer 输入转换为 RGB_U16 交错输出。
 * 模块实例名为 DEMOSAIC_<grp>，端口固定为 input0 和 output0。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * demosaic 创建属性。字段顺序与旧 libmedia 的 MEDIA_DEMOSAIC_ATTR 保持一致。
 */
typedef struct {
    int width;                  /* 输入和输出有效宽度（像素）。 */
    int height;                 /* 输入和输出有效高度（像素）。 */
    int input_depth;            /* input0 队列深度；0 使用框架默认值。 */
    int output_pool_id;         /* output0 使用的外部 Pool ID。 */
    int input_stride;           /* RAW16 每行字节数；0 按布局默认值。 */
    int output_stride;          /* RGB_U16 每行字节数；0 使用 width*6。 */
    MEDIA_BAYER_PATTERN bayer_pattern; /* Bayer 排列：BGGR/GBRG/GRBG/RGGB。 */
} MEDIA_DEMOSAIC_ATTR;

/*
 * 运行统计。字段顺序与旧 libmedia 的 MEDIA_DEMOSAIC_PERF 保持一致；
 * GLES/CPU 后端专有计数在公开 SDK 不可用时保持为零。
 */
typedef struct {
    uint64_t frame_count;
    uint64_t process_count;
    uint64_t process_fail_count;
    uint64_t drop_count;
    double green_kernel_last_ms;
    double green_kernel_avg_ms;
    double pack_kernel_last_ms;
    double pack_kernel_avg_ms;
    double kernel_total_last_ms;
    double kernel_total_avg_ms;
    double process_last_ms;
    double process_avg_ms;
    double process_max_ms;
    uint64_t dmabuf_import_count;
    uint64_t import_cache_hit_count;
    uint64_t import_cache_miss_count;
    uint64_t import_cache_entries;
    uint64_t last_frame_import_count;
    uint64_t max_frame_import_count;
    uint64_t cpu_fallback_count;
    uint64_t copy_count;
    uint64_t map_count;
    uint64_t unmap_count;
    uint64_t readback_count;
    uint64_t output_completion_wait_count;
    uint64_t strong_sync_count;
} MEDIA_DEMOSAIC_PERF;

int MEDIA_DEMOSAIC_Create(int grp, const MEDIA_DEMOSAIC_ATTR *attr);
int MEDIA_DEMOSAIC_Destroy(int grp);
int MEDIA_DEMOSAIC_Start(int grp);
int MEDIA_DEMOSAIC_Stop(int grp);
int MEDIA_DEMOSAIC_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_DEMOSAIC_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_DEMOSAIC_GetLastPerf(int grp, MEDIA_DEMOSAIC_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_DEMOSAIC_H */
