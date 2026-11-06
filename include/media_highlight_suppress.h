#ifndef MEDIA_HIGHLIGHT_SUPPRESS_H
#define MEDIA_HIGHLIGHT_SUPPRESS_H

/*
 * HIGHLIGHT_SUPPRESS（NV12 高光软压制，GLES 后端）外部模块 API，由
 * libmedia_highlight_suppress.so 提供。
 *
 * 沿用原内置模块的名字与语义：模块 type 名为 "HIGHLIGHT_SUPPRESS"，实例名为
 * "HIGHLIGHT_SUPPRESS_<grp>"，端口为 input0 / output0，可直接
 * MEDIA_SYS_Bind("HIGHLIGHT_SUPPRESS", grp, "output0", ...)。
 * MEDIA_HIGHLIGHT_SUPPRESS_* 函数名、参数、结构体名与字段顺序与旧
 * include/media_api.h 完全一致（源码级兼容）；结构体只在末尾追加字段。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 模块 type 名（MEDIA_SYS_Bind 的 mod 参数）与端口名。 */
#define MEDIA_HIGHLIGHT_SUPPRESS_TYPE "HIGHLIGHT_SUPPRESS"
#define MEDIA_HIGHLIGHT_SUPPRESS_INPUT_PORT "input0"
#define MEDIA_HIGHLIGHT_SUPPRESS_OUTPUT_PORT "output0"

/* 参数缺省值：配置字段 <= 0 时采用（与旧模块一致）。 */
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_INPUT_DEPTH 4
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_THRESHOLD_LOW 0.58f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_THRESHOLD_HIGH 0.86f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_KNEE 0.68f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_RATIO 0.08f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_STRENGTH 0.94f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_CHROMA_LOW 0.05f
#define MEDIA_HIGHLIGHT_SUPPRESS_DEFAULT_CHROMA_HIGH 0.52f

/* 输入/输出 plane0 行字节（pitch）的缺省对齐。 */
#define MEDIA_HIGHLIGHT_SUPPRESS_STRIDE_ALIGN 64

/* 创建参数。只支持 MEDIA_FORMAT_NV12、LINEAR（compress=0）输入输出。 */
typedef struct {
    /* 图像可见宽度（像素），必须 > 0 且为偶数。 */
    int width;
    /* 图像可见高度（像素），必须 > 0 且为偶数。 */
    int height;
    /* 输入输出像素格式，只接受 MEDIA_FORMAT_NV12。 */
    int format;
    /* 输入端口队列深度，<= 0 取 4；即旧模块的 input_depth。 */
    int input_depth;
    /* 输出 Buffer 的 Pool id；< 0 表示与输入同 Pool（旧语义）。 */
    int output_pool_id;
    /* 输入 plane0 行字节；<= 0 取 width 按 64 字节对齐。 */
    int input_stride;
    /* 输出 plane0 行字节；<= 0 取 input_stride。 */
    int output_stride;
    /* 高光起点（Y 归一化 0..1），<= 0 取 0.58；下限钳制到 0。 */
    float threshold_low;
    /* 高光终点，<= 0 取 0.86；钳制到 [threshold_low+0.001, 1]。 */
    float threshold_high;
    /* 软膝点（Y 归一化 0..1），<= 0 取 0.68；钳制到 [0, 1]。 */
    float knee;
    /* 膝点以上保留比例（0..1），<= 0 取 0.08；钳制到 [0, 1]。 */
    float ratio;
    /* 压制强度（0..1），<= 0 取 0.94；钳制到 [0, 1]。 */
    float strength;
    /* 低饱和起点（|U-0.5|+|V-0.5|，0..1），<= 0 取 0.05。 */
    float chroma_low;
    /* 低饱和终点，<= 0 取 0.52；钳制到 [chroma_low+0.001, 1]。 */
    float chroma_high;
    /* 非 0 时整帧原样透传（不做像素运算）；0 表示正常压制。 */
    int passthrough;
} MEDIA_HIGHLIGHT_SUPPRESS_ATTR;

/* 最近一帧的处理耗时统计（GetLastPerf 输出）。 */
typedef struct {
    /* 本帧在插件内的总处理耗时（毫秒），含 GPU 提交与等待。 */
    double cpu_ms;
    /* GLES 绘制+GPU 完成等待耗时（毫秒）；透传帧为 -1。 */
    double gpu_kernel_ms;
    /* 旧 OpenCL 后端的排队耗时；GLES 后端恒为 -1。 */
    double gpu_queue_ms;
    /* 是否启用 GPU 后端；GLES 插件在实例存活期间恒为 1。 */
    int gpu_enabled;
} MEDIA_HIGHLIGHT_SUPPRESS_PERF;

/* 创建实例（grp 即实例 id，实例名 HIGHLIGHT_SUPPRESS_<grp>）。 */
int MEDIA_HIGHLIGHT_SUPPRESS_CreateGrp(int grp, const MEDIA_HIGHLIGHT_SUPPRESS_ATTR *attr);
/* 销毁实例，需先 Stop。 */
int MEDIA_HIGHLIGHT_SUPPRESS_DestroyGrp(int grp);
/* 启动处理线程与 GLES 上下文。 */
int MEDIA_HIGHLIGHT_SUPPRESS_Start(int grp);
/* 停止处理线程并释放 GLES 资源，可重复调用。 */
int MEDIA_HIGHLIGHT_SUPPRESS_Stop(int grp);
/* 等同 Start。 */
int MEDIA_HIGHLIGHT_SUPPRESS_Enable(int grp);
/* 等同 Stop。 */
int MEDIA_HIGHLIGHT_SUPPRESS_Disable(int grp);
/* 向 input0 投递一帧（需带 NV12 VIDEO_FRAME metadata 或使用缺省 LINEAR 布局）。 */
int MEDIA_HIGHLIGHT_SUPPRESS_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
/* 从 output0 取一帧，所有权归调用者，用后 ReleaseFrame。 */
int MEDIA_HIGHLIGHT_SUPPRESS_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 GetFrame 得到的 Buffer。 */
int MEDIA_HIGHLIGHT_SUPPRESS_ReleaseFrame(int grp, MEDIA_BUFFER buf);
/* 运行期切换透传（set_param "passthrough"）。 */
int MEDIA_HIGHLIGHT_SUPPRESS_SetPassthrough(int grp, int enable);
/* 运行期修改压制强度（set_param "strength"，钳制到 [0, 1]）。 */
int MEDIA_HIGHLIGHT_SUPPRESS_SetStrength(int grp, float strength);
/* 运行期修改软膝点（set_param "knee"，钳制到 [0, 1]）。 */
int MEDIA_HIGHLIGHT_SUPPRESS_SetKnee(int grp, float knee);
/* 读取最近一帧的耗时统计。 */
int MEDIA_HIGHLIGHT_SUPPRESS_GetLastPerf(int grp, MEDIA_HIGHLIGHT_SUPPRESS_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_HIGHLIGHT_SUPPRESS_H */
