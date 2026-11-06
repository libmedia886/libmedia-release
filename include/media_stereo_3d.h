#ifndef MEDIA_STEREO_3D_H
#define MEDIA_STEREO_3D_H

/*
 * STEREO_3D（GLES 双目合成）外部模块 API，由 libmedia_stereo_3d.so 提供。
 * 两路 NV12 输入合成一路 NV12 输出，支持"左右并排"与"隔行交替"两种排列，
 * 其中一路可按角度旋转（正角度为画面顺时针，旋转后空白区域填 0）。
 *
 * 实例名 "STEREO_3D_<grp>"，端口固定为 input0 / input1 / output0，可直接
 * MEDIA_SYS_Bind("STEREO_3D", grp, "output0", ...)。函数名、参数与结构体字段
 * 顺序与旧内置模块保持一致（源码级兼容，不保证二进制兼容）。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MEDIA_SYS_Bind / MEDIA_PLUGIN_CreateModule 使用的模块类型名。 */
#define MEDIA_STEREO_3D_TYPE "STEREO_3D"

/* 输出排列方式。数值沿用旧模块，不要改动。 */
typedef enum {
    MEDIA_STEREO_3D_MODE_SIDE_BY_SIDE = 0, /* 左半幅取 direct 输入（横向拉伸），右半幅取 rotated 输入 */
    MEDIA_STEREO_3D_MODE_LINE_BY_LINE = 1, /* 偶数行取 direct 输入，奇数行取 rotated 输入 */
} MEDIA_STEREO_3D_MODE;

/*
 * 分组属性。字段顺序与旧 include/media_api.h 完全一致，新增字段只能追加在末尾。
 */
typedef struct {
    int width;              /* 图像宽度（像素），必须为偶数，范围 [2, 8192] */
    int height;             /* 图像高度（像素），必须为偶数，范围 [2, 8192] */
    int format;             /* 输入/输出图像格式，目前仅支持 MEDIA_FORMAT_NV12 */
    int input_depth;        /* 输入端口队列深度，<=0 时取默认值 4 */
    int output_pool_id;     /* 输出 buffer 所在的 Pool ID，必须 >= 0 */
    int input_stride;       /* 输入 Y/UV 行字节跨度；<=0 时取 align64(width)，须 >= width 且 64 对齐 */
    int output_stride;      /* 输出 Y/UV 行字节跨度；<=0 时取 align64(width)，须 >= width 且 64 对齐 */
    int mode;               /* MEDIA_STEREO_3D_MODE 之一 */
    int rotate_input;       /* 0 = 旋转 input0、input1 作为直通源；1 = 旋转 input1、input0 作为直通源 */
    /*
     * 旋转角（度），作用在 rotate_input 指定的那一路，以画面中心 ((W-1)/2, (H-1)/2) 为轴。
     * 正角度使该路画面顺时针旋转（图像坐标 y 向下）；旋转后落在源图之外的区域填 0。
     * 语义与旧 OpenCL kernel 的 cos/sin 完全一致，负角度可用于反向旋转。
     */
    float rotation_degrees;
} MEDIA_STEREO_3D_ATTR;

/*
 * 创建分组。grp 为分组号，同一 (类型, grp) 重复创建返回 MEDIA_ERR_ALREADY_EXISTS。
 * attr 的属性会深拷贝，调用返回后可以释放。
 */
int MEDIA_STEREO_3D_CreateGrp(int grp, const MEDIA_STEREO_3D_ATTR *attr);

/* 销毁分组；内部会先停止工作线程并归还历史帧。 */
int MEDIA_STEREO_3D_DestroyGrp(int grp);

/* 启动工作线程并创建本实例的 GLES 上下文（创建失败时返回错误码）。 */
int MEDIA_STEREO_3D_Start(int grp);

/* 停止工作线程，归还历史帧并释放 GLES 上下文；可再次 Start。 */
int MEDIA_STEREO_3D_Stop(int grp);

/* 与 Start 等价，保留旧 API 名字。 */
int MEDIA_STEREO_3D_Enable(int grp);

/* 与 Stop 等价，保留旧 API 名字。 */
int MEDIA_STEREO_3D_Disable(int grp);

/* 从 output0 取一帧；成功时 buf 归调用者，必须用 ReleaseFrame 归还。 */
int MEDIA_STEREO_3D_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);

/* 归还 GetFrame 取得的 buffer；与旧实现一致，只使用 buf，忽略 grp。 */
int MEDIA_STEREO_3D_ReleaseFrame(int grp, MEDIA_BUFFER buf);

/* 运行期修改旋转角（度）；下一帧生效，取值为参数 key "rotation_degrees"（float）。 */
int MEDIA_STEREO_3D_SetRotation(int grp, float rotation_degrees);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_STEREO_3D_H */

