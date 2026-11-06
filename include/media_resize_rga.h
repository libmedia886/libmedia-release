#ifndef MEDIA_RESIZE_RGA_H
#define MEDIA_RESIZE_RGA_H

/*
 * RESIZE_RGA（RGA 硬件缩放 / 裁剪）外部模块 API，由 libmedia_resize_rga.so 提供。
 *
 * 与原内置 RESIZE_RGA 源码级兼容：属性结构体字段名/顺序、8 个 API 函数签名均保持
 * 不变。应用包含本头文件并链接 libmedia_resize_rga.so 后，原源码无需修改即可编译。
 *
 * 模块实例名 "RESIZE_RGA_<grp>"，端口名 input0 / output0，因此
 * MEDIA_SYS_Bind("RESIZE_RGA", grp, "output0", ...) 的规则与迁移前一致。
 * 调用任何 MEDIA_RESIZE_RGA_* 前必须先 MEDIA_SYS_Init()。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resize RGA 属性（字段名与顺序必须与旧内置模块完全一致）。 */
typedef struct {
    /* 输入参数 */
    int src_x;                  /* 源图像 X 坐标裁剪起点 */
    int src_y;                  /* 源图像 Y 坐标裁剪起点 */
    int src_width;              /* 源图像裁剪宽度 (0 表示使用输入端口宽度) */
    int src_height;             /* 源图像裁剪高度 (0 表示使用输入端口高度) */
    int input_width;            /* 输入图像宽度 (0 表示按 crop 区域兼容推导) */
    int input_height;           /* 输入图像高度 (0 表示按 crop 区域兼容推导) */
    int input_stride;           /* 输入 byte stride（0 表示按格式和宽度推导） */
    int input_format;           /* NV12/RGB family；YUV420/422SP_10BIT 仅允许 compact NV15/NV20 同格式路径 */
    int input_depth;            /* 输入端口队列深度（0 表示默认=4） */

    /* 输出参数 */
    int out_width;              /* 输出宽度 */
    int out_height;             /* 输出高度 */
    int out_stride;             /* 输出 byte stride（0 表示按格式和宽度推导） */
    int output_format;          /* NV12/RGB family；YUV420/422SP_10BIT 仅允许 compact NV15/NV20 同格式路径 */
    int output_pool_id;         /* 输出 buffer 池 ID */
    int keep_aspect_ratio;      /* 1=等比缩放居中填黑，0=拉伸到整幅输出 */

    /* 压缩（旧头文件即放在结构体末尾，顺序不可调整） */
    MEDIA_VIDEO_COMPRESS input_compress;  /* 0=LINEAR；NV12 AFBC 仅 0x001/0x041 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 0=LINEAR；NV12 AFBC 仅 0x001/0x041 */
} MEDIA_RESIZE_RGA_ATTR;

int MEDIA_RESIZE_RGA_CreateGrp(int grp, const MEDIA_RESIZE_RGA_ATTR *attr);
int MEDIA_RESIZE_RGA_DestroyGrp(int grp);
int MEDIA_RESIZE_RGA_Start(int grp);
int MEDIA_RESIZE_RGA_Stop(int grp);
int MEDIA_RESIZE_RGA_Enable(int grp);
int MEDIA_RESIZE_RGA_Disable(int grp);
int MEDIA_RESIZE_RGA_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_RESIZE_RGA_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_RESIZE_RGA_H */
