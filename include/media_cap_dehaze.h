#ifndef MEDIA_CAP_DEHAZE_H
#define MEDIA_CAP_DEHAZE_H

/*
 * CAP_DEHAZE（基于 CAP 深度先验 + 引导滤波的单帧去雾，GLES 实现）外部模块 API。
 *
 * 由 libmedia_cap_dehaze.so 提供，只依赖公开 SDK（media_api.h / media_plugin.h）。
 * 实例名 "CAP_DEHAZE_<grp>"，端口 input0 / output0，可直接
 * MEDIA_SYS_Bind("CAP_DEHAZE", grp, "output0", ...)。属性结构与函数签名沿用原内置
 * 模块 media_api.h 中的 MEDIA_CAP_DEHAZE_*，源码级兼容。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MEDIA_SYS_Bind / MEDIA_PLUGIN_* 使用的模块 type 名。 */
#define MEDIA_CAP_DEHAZE_TYPE "CAP_DEHAZE"

/* 模块实例的输入/输出端口名（只有一个输入、一个输出）。 */
#define MEDIA_CAP_DEHAZE_INPUT_PORT "input0"
#define MEDIA_CAP_DEHAZE_OUTPUT_PORT "output0"

typedef struct {
    int width;                  /* 图像宽度（输入输出相同） */
    int height;                 /* 图像高度（输入输出相同） */
    int format;                 /* 图像格式：仅 MEDIA_FORMAT_RGB888 / MEDIA_FORMAT_BGR888 */
    int input_depth;            /* 输入端口队列深度（0 表示默认=4） */
    int output_pool_id;         /* 输出 buffer 池 ID，必须 >= 0 */
    int input_stride;           /* 输入 plane0 行字节数：0=按格式自动计算；非 0 时须 >= width*3 且 64 字节对齐 */
    int output_stride;          /* 输出 plane0 行字节数：0=按格式自动计算；非 0 时须 >= width*3 且 64 字节对齐 */
    int guided_radius;          /* 引导滤波半径（<=0 表示默认=24，最终不超过图像宽高） */
    float guided_eps;           /* 引导滤波 epsilon（<=0 表示默认=1e-3） */
    float t0;                   /* 透射率下限（<=0 表示默认=0.12，大于 1 时夹到 1） */
    float beta0;                /* CAP 深度先验常数项（严格等于 0 表示默认=0.121779） */
    float beta1;                /* CAP 深度先验最大通道系数（严格等于 0 表示默认=0.959710） */
    float beta2;                /* CAP 深度先验饱和度系数（严格等于 0 表示默认=-0.780245） */
    float depth_scale;          /* 深度整体缩放（<=0 表示默认=1.0） */
    float refine_scale;         /* 透射率细化缩放：<=0 表示默认=0.25，1.0 表示全分辨率；上限 1.0 */
    int passthrough;            /* 创建时直通模式：非 0 时只做一次格式拷贝，不做去雾 */
} MEDIA_CAP_DEHAZE_ATTR;

/* 创建/销毁实例；CreateGrp 成功后实例即可 Start。 */
int MEDIA_CAP_DEHAZE_CreateGrp(int grp, const MEDIA_CAP_DEHAZE_ATTR *attr);
int MEDIA_CAP_DEHAZE_DestroyGrp(int grp);

/* 启动/停止工作线程与 GLES 上下文。 */
int MEDIA_CAP_DEHAZE_Start(int grp);
int MEDIA_CAP_DEHAZE_Stop(int grp);

/* 与原模块同义的启停别名。 */
int MEDIA_CAP_DEHAZE_Enable(int grp);
int MEDIA_CAP_DEHAZE_Disable(int grp);

/* 送一帧输入（成功即转移 buffer 引用），取一帧输出/归还输出。 */
int MEDIA_CAP_DEHAZE_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_CAP_DEHAZE_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_CAP_DEHAZE_ReleaseFrame(int grp, MEDIA_BUFFER buf);

/* 运行期切换直通模式（1=直通，0=正常去雾），无需重启实例。 */
int MEDIA_CAP_DEHAZE_SetPassthrough(int grp, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CAP_DEHAZE_H */

