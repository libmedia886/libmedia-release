#ifndef MEDIA_CSC_RGA_H
#define MEDIA_CSC_RGA_H

/*
 * CSC_RGA（RGA 硬件颜色空间转换）外部模块 API，由 libmedia_csc_rga.so 提供。
 *
 * 与原内置 CSC_RGA 源码级兼容：MEDIA_CSC_RGA_ATTR 字段名/顺序、8 个
 * MEDIA_CSC_RGA_* 函数签名与旧声明完全一致；CSC_RGA_CSC_* 宏取值也不变。
 *
 * 实例名为 "CSC_RGA_<grp>"，输入端口 input0、输出端口 output0，因此
 * MEDIA_SYS_Bind("CSC_RGA", grp, "output0", ...) 的用法与迁移前相同。
 *
 * 调用任何 MEDIA_CSC_RGA_* 之前必须先 MEDIA_SYS_Init()，并链接 libmedia.so
 * 与本库。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CSC_RGA 颜色空间转换模式（与旧内置模块的宏值一致） */
#define CSC_RGA_CSC_DEFAULT         0   /* 默认颜色空间 */
#define CSC_RGA_CSC_BT601_LIMIT     1   /* BT.601 有限范围 */
#define CSC_RGA_CSC_BT601_FULL      2   /* BT.601 全范围 */
#define CSC_RGA_CSC_BT709_LIMIT     3   /* BT.709 有限范围 */
#define CSC_RGA_CSC_BT709_FULL      4   /* BT.709 全范围 */

/* CSC_RGA 属性（字段名与顺序与原内置 csc_rga_config_t / 旧 media_api.h 一致） */
typedef struct {
    int input_width;            /* 输入宽度 */
    int input_height;           /* 输入高度 */
    int input_format;           /* 输入格式 (MEDIA_FORMAT_*) */
    int output_format;          /* 输出格式 (MEDIA_FORMAT_*) */
    int input_depth;            /* 输入端口队列深度（0 表示默认=4） */
    int output_pool_id;         /* 输出 buffer 池 ID */
    int input_stride;           /* 输入 stride (0 表示根据格式自动计算) */
    int output_stride;          /* 输出 stride (0 表示根据格式自动计算) */
    int csc_mode;               /* 颜色空间转换模式 (CSC_RGA_CSC_*) */
    MEDIA_VIDEO_COMPRESS input_compress;  /* 精确存储契约，AFBC时stride=0 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 精确存储契约，AFBC时stride=0 */
} MEDIA_CSC_RGA_ATTR;

int MEDIA_CSC_RGA_CreateGrp(int grp, const MEDIA_CSC_RGA_ATTR *attr);
int MEDIA_CSC_RGA_DestroyGrp(int grp);
int MEDIA_CSC_RGA_Start(int grp);
int MEDIA_CSC_RGA_Stop(int grp);
int MEDIA_CSC_RGA_Enable(int grp);
int MEDIA_CSC_RGA_Disable(int grp);
int MEDIA_CSC_RGA_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_CSC_RGA_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CSC_RGA_H */
