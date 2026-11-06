#ifndef MEDIA_DCP_FAST_DEHAZE_H
#define MEDIA_DCP_FAST_DEHAZE_H

/*
 * DCP 快速去雾（Dark Channel Prior Fast Dehaze）外部模块 API，由
 * libmedia_dcp_fast_dehaze.so 提供，后端为 OpenGL ES。
 *
 * 实例名为 "DCP_FAST_DEHAZE_<grp>"，端口固定为 input0 / output0，可直接参与
 * MEDIA_SYS_Bind("DCP_FAST_DEHAZE", grp, "output0", ...)。
 *
 * 本头文件与旧内置模块源码兼容：MEDIA_DCP_FAST_DEHAZE_ATTR 的字段名、类型与顺序，
 * 以及全部函数签名均与旧 include/media_api.h 完全一致（不要求二进制兼容）。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 模块 type 名：MEDIA_SYS_Bind 的 mod 参数，应用侧 Bind 名保持旧值不变 */
#define MEDIA_DCP_FAST_DEHAZE_TYPE "DCP_FAST_DEHAZE"

/*
 * 去雾参数。width/height 必须为正，format 只能是 RGB888/BGR888，output_pool_id 必须 >= 0；
 * 其余可选参数 <= 0 时取默认值（与旧内置模块一致）。输入与输出使用同一格式。
 */
typedef struct {
    int width;                 /* 图像宽度（像素），必须 > 0 */
    int height;                /* 图像高度（像素），必须 > 0 */
    int format;                /* 输入/输出格式：MEDIA_FORMAT_RGB888 或 MEDIA_FORMAT_BGR888 */
    int input_depth;           /* 输入端口队列深度；<= 0 时取默认 4 */
    int output_pool_id;        /* 输出 Buffer 池 ID，必须 >= 0；输出帧从该池取 */
    int input_stride;          /* 输入 plane0 行字节数；<= 0 时取格式规范 pitch */
    int output_stride;         /* 输出 plane0 行字节数；<= 0 时取格式规范 pitch */
    int patch;                 /* dark channel 窗口尺寸；<= 0 取 15，偶数自动 +1；小图上最小 3 */
    float omega;               /* 雾保留系数，t = 1 - omega * dark；<= 0 取 0.95 */
    float t0;                  /* transmission 下限；<= 0 取 0.40 */
    float airlight_percent;    /* airlight 候选像素比例；<= 0 取 0.0005 */
    int guided_radius;         /* 引导滤波半径（以全分辨率像素计）；<= 0 取 24 */
    float guided_eps;          /* 引导滤波 epsilon，抑制除零；<= 0 取 1e-3 */
    float refine_scale;        /* transmission 精化缩放比例；<= 0 取 0.5，夹在 [64/W, 1] */
    int passthrough;           /* 初始直通模式：非 0 时输入帧零拷贝直接输出，可在运行期用
                                * MEDIA_DCP_FAST_DEHAZE_SetPassthrough 切换 */
} MEDIA_DCP_FAST_DEHAZE_ATTR;

/* 创建去雾实例 grp（实例名 DCP_FAST_DEHAZE_<grp>）；attr 不能为空 */
int MEDIA_DCP_FAST_DEHAZE_CreateGrp(int grp, const MEDIA_DCP_FAST_DEHAZE_ATTR *attr);
/* 销毁实例；需先 Stop */
int MEDIA_DCP_FAST_DEHAZE_DestroyGrp(int grp);
/* 启动处理线程并创建 GLES 上下文 */
int MEDIA_DCP_FAST_DEHAZE_Start(int grp);
/* 停止处理线程并释放 GLES 上下文 */
int MEDIA_DCP_FAST_DEHAZE_Stop(int grp);
/* 等价于 Start */
int MEDIA_DCP_FAST_DEHAZE_Enable(int grp);
/* 等价于 Stop */
int MEDIA_DCP_FAST_DEHAZE_Disable(int grp);
/* 向 input0 送一帧；成功后 buffer 所有权转移给模块 */
int MEDIA_DCP_FAST_DEHAZE_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
/* 从 output0 取一帧；成功后需调用 MEDIA_DCP_FAST_DEHAZE_ReleaseFrame */
int MEDIA_DCP_FAST_DEHAZE_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 GetFrame 得到的帧；grp 参数仅为接口兼容，未使用 */
int MEDIA_DCP_FAST_DEHAZE_ReleaseFrame(int grp, MEDIA_BUFFER buf);
/* 运行期切换直通模式（1 开 / 0 关），不重启实例生效 */
int MEDIA_DCP_FAST_DEHAZE_SetPassthrough(int grp, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_DCP_FAST_DEHAZE_H */

