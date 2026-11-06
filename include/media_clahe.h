#ifndef MEDIA_CLAHE_H
#define MEDIA_CLAHE_H

/*
 * CLAHE（Contrast Limited Adaptive Histogram Equalization，GLES 实现）
 * 外部模块 API，由 libmedia_clahe.so 提供。旧内置模块的 MEDIA_CLAHE_* 源码级兼容入口：
 * 包含本头文件并链接 libmedia_clahe.so，函数名、参数和 MEDIA_CLAHE_ATTR 字段顺序保持不变。
 *
 * 模块 type 名为 "CLAHE"，实例名为 "CLAHE_<grp>"；输入端口 input0，输出端口 output0，
 * 均为 NV12。应用侧可以直接 MEDIA_SYS_Bind("TPG", 0, "output0", "CLAHE", grp, "input0")。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MEDIA_SYS_Bind / MEDIA_PLUGIN_CreateModule 使用的模块类型名。 */
#define MEDIA_CLAHE_TYPE "CLAHE"
/* 默认分块网格（tile_grid_x/tile_grid_y <= 0 时使用）。 */
#define MEDIA_CLAHE_DEFAULT_TILE_GRID 8
/* 当前实现固定 256 个直方图 bin；bins <= 0 视为 256。 */
#define MEDIA_CLAHE_BINS 256

/*
 * CLAHE 配置。字段顺序与旧 master:include/media_api.h 完全一致，便于旧应用源码直接迁移；
 * 新字段只能在结构体末尾追加。
 */
typedef struct {
    int width;                       /* 图像宽度（像素），必须与输入/输出端口一致。 */
    int height;                      /* 图像高度（像素），必须与输入/输出端口一致。 */
    int format;                      /* 逻辑格式；当前只支持 MEDIA_FORMAT_NV12。 */
    int tile_grid_x;                 /* 水平分块数；<=0 使用 8，且不得超过图像宽度。 */
    int tile_grid_y;                 /* 垂直分块数；<=0 使用 8，且不得超过图像高度。 */
    int bins;                        /* 直方图 bin 数；<=0 视为 256，其它非 256 的正值创建失败。 */
    int input_depth;                 /* 输入端口 Bind 队列深度；<=0 使用 4。 */
    int output_pool_id;              /* 非负；输出 Buffer 从该 Pool 申请，必须能容纳一帧 NV12。 */
    int input_stride;                /* 输入 Y plane 字节 pitch；0 使用格式 canonical pitch，非零须为 64 的倍数且不小于 canonical。 */
    int output_stride;               /* 输出 Y/UV plane 字节 pitch；0 使用格式 canonical pitch，非零须为 64 的倍数且不小于 canonical。 */
    float clip_limit;                /* 直方图裁剪系数；<=0 使用 2.0，运行期必须 >0。 */
    float highlight_protect_start;   /* 高光保护起始归一化亮度（0..1）；<=0 表示关闭高光保护。 */
    float highlight_protect_strength;/* 高光保护强度（0..1）；<=0 在创建时使用 1.0，运行期设为 0 可关闭。 */
    int passthrough;                 /* 非零表示直通：直接把输入 Buffer 原样送到 output0，不申请输出 Pool。 */
} MEDIA_CLAHE_ATTR;

int MEDIA_CLAHE_CreateGrp(int grp, const MEDIA_CLAHE_ATTR *attr);
int MEDIA_CLAHE_DestroyGrp(int grp);
int MEDIA_CLAHE_Start(int grp);
int MEDIA_CLAHE_Stop(int grp);
int MEDIA_CLAHE_Enable(int grp);
int MEDIA_CLAHE_Disable(int grp);
int MEDIA_CLAHE_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_CLAHE_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_CLAHE_SetClipLimit(int grp, float clip_limit);
int MEDIA_CLAHE_SetHighlightProtect(int grp, float start, float strength);
int MEDIA_CLAHE_SetPassthrough(int grp, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CLAHE_H */

