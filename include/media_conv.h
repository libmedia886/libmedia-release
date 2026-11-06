#ifndef MEDIA_CONV_H
#define MEDIA_CONV_H

/*
 * CONV_CL（GLES 二维卷积）外部模块 API，由 libmedia_conv.so 提供。
 *
 * 与原 libmedia 的 MEDIA_CONV_CL_* 源码级兼容：函数名、参数、结构体名和字段顺序保持不变，
 * 应用包含本头文件并链接 libmedia_conv.so 即可继续使用旧调用方式。实例名为
 * "CONV_CL_<grp>"，端口固定为 input0 / output0，可直接
 * MEDIA_SYS_Bind("CONV_CL", grp, "output0", ...)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 插件注册的模块类型名（沿用旧名，应用侧 Bind 名不变）。 */
#define MEDIA_CONV_TYPE "CONV_CL"

/* kernel_size 为 0 时使用的默认卷积核尺寸。 */
#define MEDIA_CONV_DEFAULT_KERNEL_SIZE 5

/* 支持的最大卷积核尺寸（必须为奇数，实际范围 1..31）。 */
#define MEDIA_CONV_MAX_KERNEL_SIZE 31

/* 宽高下限，沿用旧模块的创建约束。 */
#define MEDIA_CONV_MIN_WIDTH 16
#define MEDIA_CONV_MIN_HEIGHT 16

/* 模块创建配置，字段顺序与 master:include/media_api.h 完全一致。 */
typedef struct {
    int width;                  /* 图像宽度（像素），必须 >= 16 */
    int height;                 /* 图像高度（像素），必须 >= 16 */
    int format;                 /* 输入/输出共用的 MEDIA_FORMAT_*；仅支持 LINEAR 存储 */
    int kernel_size;            /* 卷积核尺寸（默认 5；必须为 1..31 的奇数） */
    int input_depth;            /* 输入端口队列深度（0 表示默认 4） */
    int output_pool_id;         /* 应用创建的输出 Buffer Pool ID，必须 >= 0 */
    int input_stride;           /* 输入 plane0 字节 pitch（0 按格式推导；非 0 需 >= canonical 且 64 对齐；逐帧看 metadata） */
    int output_stride;          /* 输出 plane0 字节 pitch（0 跟随 canonical；非 0 需 >= canonical 且 64 对齐） */
} MEDIA_CONV_CL_ATTR;

/* 最近一帧的性能数据，字段顺序与 master:include/media_api.h 完全一致。 */
typedef struct {
    double gpu_kernel_total_ms; /* 上一帧从提交绘制到 GPU fence 完成的时间（毫秒） */
    double gpu_queue_total_ms;  /* 旧模块未统计队列时间，固定返回 -1 */
} MEDIA_CONV_CL_PERF;

/* 创建卷积实例；attr 的生命周期只需覆盖本调用。 */
int MEDIA_CONV_CL_CreateGrp(int grp, const MEDIA_CONV_CL_ATTR *attr);
/* 销毁实例；已 Stop 后调用，输出 Pool 由应用自行销毁。 */
int MEDIA_CONV_CL_DestroyGrp(int grp);
/* 启动实例（创建 GLES 上下文并加载 program binary）。 */
int MEDIA_CONV_CL_Start(int grp);
/* 停止实例并释放 GLES 资源；可重复调用。 */
int MEDIA_CONV_CL_Stop(int grp);
/* 从 output0 取一帧；timeout_ms < 0 表示一直等待。 */
int MEDIA_CONV_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 MEDIA_CONV_CL_GetFrame 得到的帧。 */
int MEDIA_CONV_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
/* 修改卷积核尺寸并重置为单位脉冲表（1..31 的奇数）。 */
int MEDIA_CONV_CL_SetKernelSize(int grp, int kernel_size);
/* 上传卷积表；table_size 必须等于 kernel_size * kernel_size，所有值必须有限。 */
int MEDIA_CONV_CL_SetTable(int grp, const float *table, int table_size);
/* 直通开关：非 0 时跳过卷积，输出与输入像素内容一致（仍写入输出 Pool）。 */
int MEDIA_CONV_CL_SetPassthrough(int grp, int enable);
/* 读取最近一帧 GPU 耗时；无有效帧时返回负错误码。 */
int MEDIA_CONV_CL_GetLastPerf(int grp, MEDIA_CONV_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CONV_H */
