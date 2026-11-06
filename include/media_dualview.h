#ifndef MEDIA_DUALVIEW_H
#define MEDIA_DUALVIEW_H

/* 双视 GLES 外部模块；属性与旧 media_api.h 保持源码兼容。 */
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_DUALVIEW_TYPE "DUALVIEW"

typedef enum {
    MEDIA_DUALVIEW_MODE_SIDE_BY_SIDE = 0, /* 并排显示，后一路覆盖重叠区域 */
    MEDIA_DUALVIEW_MODE_LINE_BY_LINE = 1, /* 偶数行取 input0，奇数行取 input1 */
} MEDIA_DUALVIEW_MODE;

typedef struct {
    int enabled; /* 非零启用；inputs 全部为零时默认启用两路 */
    int x;       /* 并排输出 X；<=0 按前面已启用视图的宽度累计排列 */
    int y;       /* 并排输出 Y；<=0 取 0，逐行模式忽略坐标 */
    int width;   /* 视图宽；0 取输入宽，并排缩放、逐行裁剪 */
    int height;  /* 视图高；0 取输入高，并排缩放、逐行裁剪 */
    int stride;  /* 本路输入字节 pitch；0 继承 input_stride，逐帧核对 metadata */
} MEDIA_DUALVIEW_INPUT;

typedef struct {
    MEDIA_DUALVIEW_INPUT inputs[2]; /* 两路视图配置，字段顺序与旧 API 一致 */
    int input_width;                /* 两路输入有效宽，1..4096 */
    int input_height;               /* 两路输入有效高，1..4096 */
    int input_stride;               /* 输入字节 pitch；0 采用 SDK 的 64 字节对齐值 */
    int output_width;               /* <=0：并排为输入宽乘启用路数，逐行为输入宽 */
    int output_height;              /* <=0 取输入高 */
    int output_stride;              /* 输出字节 pitch；0 采用 QueryLayout，非零须与其一致 */
    int mode;                      /* MEDIA_DUALVIEW_MODE_* */
    int format;                    /* 输入输出均为 MEDIA_FORMAT_RGB888，LINEAR */
    int input_depth;               /* 输入队列深度；<=0 默认 4 */
    int output_pool_id;            /* 应用预建的 DMA-BUF 输出 Pool，不能与输入 Pool 共用 */
} MEDIA_DUALVIEW_ATTR;

int MEDIA_DUALVIEW_CreateGrp(int grp, const MEDIA_DUALVIEW_ATTR *attr);
int MEDIA_DUALVIEW_DestroyGrp(int grp);
int MEDIA_DUALVIEW_Start(int grp);
int MEDIA_DUALVIEW_Stop(int grp);
int MEDIA_DUALVIEW_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_DUALVIEW_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_DUALVIEW_H */
