#ifndef MEDIA_RGA_H
#define MEDIA_RGA_H

/*
 * RGA（RGA 硬件矩阵运算/拷贝/缩放/旋转/翻转/裁剪/合成）外部模块 API，
 * 由 libmedia_rga.so 提供。与原内置 RGA 源码兼容：包含本头文件并链接
 * libmedia_rga.so 即可，函数名、参数、结构体字段顺序均保持不变。
 *
 * 实例名 "RGA_<chn>"，端口 input0..3 / output0..3（实际个数由属性决定），
 * 可直接 MEDIA_SYS_Bind("RGA", chn, "output0", ...)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_RGA_MAX_INPUTS 4
#define MEDIA_RGA_MAX_OUTPUTS 4

typedef enum {
    MEDIA_RGA_ALG_RESIZE = 0,
    MEDIA_RGA_ALG_CSC = 1,
    MEDIA_RGA_ALG_ROTATE = 2,
    MEDIA_RGA_ALG_COMPOSE = 3,
    MEDIA_RGA_ALG_COPY = 4,
    MEDIA_RGA_ALG_TRANSLATE = 5,
    MEDIA_RGA_ALG_FLIP = 6,
    MEDIA_RGA_ALG_CROP = 7,
    MEDIA_RGA_ALG_MOSAIC = 8,
    MEDIA_RGA_ALG_PYRAMID = 9,
    MEDIA_RGA_ALG_OSD = 10,
} MEDIA_RGA_ALG;

typedef struct {
    int port_id;
    int width;
    int height;
    int stride;     // bytes per line (0=auto)
    int format;     // MEDIA_FORMAT_*
    int pool_id;    // output pool id; input ignored
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
    int dst_x;
    int dst_y;
    int mosaic_mode; // IM_MOSAIC_*
    int pyramid_dir; // IM_UP_SCALE / IM_DOWN_SCALE
    int rotate;     // 0/90/180/270
    int flip_h;
    int flip_v;
    int queue_depth; // 端口队列深度（0 表示默认=4）
    MEDIA_VIDEO_COMPRESS compress; // 精确存储契约；NONE，或最终有向矩阵内的NV12 0x001/0x041（AFBC时stride=0）
} MEDIA_RGA_PORT_ATTR;

typedef struct {
    int algo;
    int input_count;   // 输入数量（1-MEDIA_RGA_MAX_INPUTS）
    int output_count;  // 输出数量（1-MEDIA_RGA_MAX_OUTPUTS）
    int input_depth;
    int output_depth;
    MEDIA_RGA_PORT_ATTR inputs[MEDIA_RGA_MAX_INPUTS];
    MEDIA_RGA_PORT_ATTR outputs[MEDIA_RGA_MAX_OUTPUTS];
} MEDIA_RGA_GRP_ATTR;

int MEDIA_RGA_CreateChn(int chn, int width, int height, int format,
                        MEDIA_VIDEO_COMPRESS input_compress,
                        MEDIA_VIDEO_COMPRESS output_compress);
int MEDIA_RGA_SetAttr(int chn, int width, int height, int format,
                      MEDIA_VIDEO_COMPRESS output_compress);
int MEDIA_RGA_DestroyChn(int chn);
int MEDIA_RGA_Start(int chn);
int MEDIA_RGA_Stop(int chn);
int MEDIA_RGA_CreateGrp(int grp, const MEDIA_RGA_GRP_ATTR *attr);
int MEDIA_RGA_SetGrpAttr(int grp, const MEDIA_RGA_GRP_ATTR *attr);
int MEDIA_RGA_SetInAttr(int grp, int in_idx, const MEDIA_RGA_PORT_ATTR *attr);
int MEDIA_RGA_SetOutAttr(int grp, int out_idx, const MEDIA_RGA_PORT_ATTR *attr);
int MEDIA_RGA_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_RGA_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_RGA_H */
