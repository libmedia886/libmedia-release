#ifndef MEDIA_THERMAL_H
#define MEDIA_THERMAL_H

/* ThermalColor GLES 外部模块；链接 libmedia_thermal.so 或使用插件加载接口。 */
#include <stddef.h>
#include <stdint.h>
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_THERMAL_TYPE "THERMAL"
#define MEDIA_THERMAL_LUT_ENTRIES 256u
#define MEDIA_THERMAL_LUT_SIZE (MEDIA_THERMAL_LUT_ENTRIES * 3u)
#define MEDIA_THERMAL_MAX_INPUT_DEPTH 64

typedef enum {
    MEDIA_THERMAL_COLOR_RAINBOW = 0,   /* 蓝→青→绿→黄→红。 */
    MEDIA_THERMAL_COLOR_BLACK_HOT = 1, /* 沿用旧命名：低值黑、高值白。 */
    MEDIA_THERMAL_COLOR_WHITE_HOT = 2, /* 沿用旧命名：低值白、高值黑。 */
    MEDIA_THERMAL_COLOR_IRON = 3,      /* 铁红。 */
    MEDIA_THERMAL_COLOR_SEPIA = 4,     /* 棕褐色。 */
    MEDIA_THERMAL_COLOR_BLUE_RED = 5,  /* 蓝→红。 */
    MEDIA_THERMAL_COLOR_GRAYSCALE = 6, /* 原始灰度。 */
    MEDIA_THERMAL_COLOR_RAINBOW1 = 7,  /* 彩虹变体 1。 */
    MEDIA_THERMAL_COLOR_RAINBOW2 = 8,  /* 彩虹变体 2。 */
    MEDIA_THERMAL_COLOR_RAINBOW3 = 9,  /* 彩虹变体 3。 */
    MEDIA_THERMAL_COLOR_PSEUDO1 = 10,  /* 伪彩色 1。 */
    MEDIA_THERMAL_COLOR_PSEUDO2 = 11,  /* 伪彩色 2。 */
    MEDIA_THERMAL_COLOR_METAL1 = 12,   /* 金属色调 1。 */
    MEDIA_THERMAL_COLOR_METAL2 = 13,   /* 金属色调 2。 */
    MEDIA_THERMAL_COLOR_ZHOU = 14,     /* Zhou 变体。 */
    MEDIA_THERMAL_COLOR_NING = 15      /* Ning 变体。 */
} MEDIA_THERMAL_COLOR_MODE;

typedef struct {
    int width;                 /* 输入/输出可见宽度，16..8192，满足格式色度对齐。 */
    int height;                /* 输入/输出可见高度，16..8192，满足格式色度对齐。 */
    int format;                /* input0 的 8 位 YUV 格式，完整列表见 README。 */
    int color_mode;            /* MEDIA_THERMAL_COLOR_*；0 为彩虹。 */
    int input_depth;           /* input0 队列深度 1..64；0 使用框架默认 4。 */
    int output_depth;          /* output0 队列深度；0 使用框架默认值。 */
    int input_stride;          /* LINEAR plane0 pitch，0=canonical；AFBC 必须为 0。 */
    MEDIA_VIDEO_COMPRESS input_compress; /* 输入的精确存储契约，0=LINEAR。 */
    int output_format;         /* output0 格式，UNSET 默认为 NV12。 */
    int output_stride;         /* LINEAR plane0 pitch，0=canonical；AFBC 必须为 0。 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 输出的精确存储契约，0=LINEAR。 */
    int output_pool_id;        /* 已创建的专用 DMA-BUF Pool，容量至少为 QueryLayout 返回值。 */
} MEDIA_THERMAL_ATTR;

#if defined(__cplusplus)
#define MEDIA_THERMAL_STATIC_ASSERT static_assert
#else
#define MEDIA_THERMAL_STATIC_ASSERT _Static_assert
#endif

MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, width) == 0,
                           "MEDIA_THERMAL_ATTR.width offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, height) == 4,
                           "MEDIA_THERMAL_ATTR.height offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, format) == 8,
                           "MEDIA_THERMAL_ATTR.format offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, color_mode) == 12,
                           "MEDIA_THERMAL_ATTR.color_mode offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, input_depth) == 16,
                           "MEDIA_THERMAL_ATTR.input_depth offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, output_depth) == 20,
                           "MEDIA_THERMAL_ATTR.output_depth offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, input_stride) == 24,
                           "MEDIA_THERMAL_ATTR.input_stride offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, input_compress) == 32,
                           "MEDIA_THERMAL_ATTR.input_compress offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, output_format) == 40,
                           "MEDIA_THERMAL_ATTR.output_format offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, output_stride) == 44,
                           "MEDIA_THERMAL_ATTR.output_stride offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, output_compress) == 48,
                           "MEDIA_THERMAL_ATTR.output_compress offset");
MEDIA_THERMAL_STATIC_ASSERT(offsetof(MEDIA_THERMAL_ATTR, output_pool_id) == 56,
                           "MEDIA_THERMAL_ATTR.output_pool_id offset");
MEDIA_THERMAL_STATIC_ASSERT(sizeof(MEDIA_THERMAL_ATTR) == 64,
                           "MEDIA_THERMAL_ATTR size");

#undef MEDIA_THERMAL_STATIC_ASSERT

int MEDIA_THERMAL_CreateGrp(int grp, const MEDIA_THERMAL_ATTR *attr);
int MEDIA_THERMAL_DestroyGrp(int grp);
int MEDIA_THERMAL_Start(int grp);
int MEDIA_THERMAL_Stop(int grp);
/* 同步处理一帧；输入入队成功后转移引用，输出用 MEDIA_POOL_PutBuffer 归还。
 * output 为 NULL 时等待处理完成并将结果发送到 output0；失败时 output 重置为 {-1,-1}。 */
int MEDIA_THERMAL_Process(int grp, MEDIA_BUFFER input, MEDIA_BUFFER *output);
/* 更新在下一次 GPU 处理时生效；SetUvLut 在返回前复制调用方的 768 字节数据。 */
int MEDIA_THERMAL_SetColorMode(int grp, int color_mode);
int MEDIA_THERMAL_SetUvLut(int grp, const uint8_t *lut, size_t size);

#ifdef __cplusplus
}
#endif
#endif
