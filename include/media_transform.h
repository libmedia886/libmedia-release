#ifndef MEDIA_TRANSFORM_H
#define MEDIA_TRANSFORM_H

/* 基于 float32 XY LUT 的 GLES 图像重映射插件。 */
#include <stddef.h>
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_TRANSFORM_MAX_INPUT_DEPTH 64
/* 沿用旧模块类型名，兼容 MEDIA_SYS_Bind("TRANSFORM", ...) 的连接方式。 */
#define MEDIA_TRANSFORM_TYPE "TRANSFORM"

typedef struct {
    int out_width;                 /* 输出可见宽度。 */
    int out_height;                /* 输出可见高度。 */
    int out_stride;                /* LINEAR 输出 plane0 pitch；压缩输出必须为 0。 */
    int output_format;             /* 输出逻辑格式（MEDIA_FORMAT_*）。 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 输出存储 modifier，0 表示 LINEAR。 */
    int input_depth;               /* input0 队列深度，0 使用默认值 4。 */
    int pool_id;                   /* 输出 Pool；必须由应用预先创建。 */
    int in_width;                  /* 输入可见宽度，0 表示等于 out_width。 */
    int in_height;                 /* 输入可见高度，0 表示等于 out_height。 */
    int in_stride;                 /* LINEAR 输入 plane0 pitch；压缩输入必须为 0。 */
    int input_format;              /* 输入逻辑格式（MEDIA_FORMAT_*）。 */
    MEDIA_VIDEO_COMPRESS input_compress; /* 输入存储 modifier，0 表示 LINEAR。 */
    int lut_width;                 /* LUT 宽度，0 表示等于输出宽度。 */
    int lut_height;                /* LUT 高度，0 表示等于输出高度。 */
    const float *lut;              /* 每个 LUT 点为 (x,y)，单位为输入像素。 */
    size_t lut_size;               /* lut 字节数，必须为 lut_width*lut_height*2*sizeof(float)。 */
} MEDIA_TRANSFORM_ATTR;

int MEDIA_TRANSFORM_CreateGrp(int grp, const MEDIA_TRANSFORM_ATTR *attr);
int MEDIA_TRANSFORM_DestroyGrp(int grp);
int MEDIA_TRANSFORM_Start(int grp);
int MEDIA_TRANSFORM_Stop(int grp);
int MEDIA_TRANSFORM_Process(int grp, MEDIA_BUFFER input, MEDIA_BUFFER *output);
int MEDIA_TRANSFORM_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_TRANSFORM_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_TRANSFORM_UpdateLut(int grp, const float *lut, size_t lut_size);

#ifdef __cplusplus
}
#endif
#endif
