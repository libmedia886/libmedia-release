#ifndef MEDIA_FLUO_VISIBLE_FUSION_H
#define MEDIA_FLUO_VISIBLE_FUSION_H

/*
 * FLUO_VISIBLE_FUSION_CL（可见光 + 荧光 NV12 融合，GLES 后端）外部模块 API，
 * 由 libmedia_fluo_visible_fusion.so 提供。
 *
 * 模块 type 名沿用旧内置模块的 "FLUO_VISIBLE_FUSION_CL"，实例名为
 * "FLUO_VISIBLE_FUSION_CL_<grp>"。输入端口 input0 为可见光、input1 为荧光，
 * 输出端口 output0。MEDIA_FLUO_VISIBLE_FUSION_CL_* 函数名、参数、结构体名和
 * 字段顺序与旧 include/media_api.h 一致，可直接替换旧模块使用。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 模块 type 名（MEDIA_SYS_Bind 的 mod 参数）。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_TYPE "FLUO_VISIBLE_FUSION_CL"
/* 可见光输入端口。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_VISIBLE_PORT "input0"
/* 荧光输入端口。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_FLUORESCENCE_PORT "input1"
/* 融合结果输出端口。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_OUTPUT_PORT "output0"

/* input_depth <= 0 时的缺省输入队列深度。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_DEFAULT_INPUT_DEPTH 4
/* thr_lo <= 0 时的缺省荧光阈值下限。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_DEFAULT_THR_LO 0.20f
/* thr_hi <= 0 时的缺省荧光阈值上限。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_DEFAULT_THR_HI 0.65f
/* overlay_alpha <= 0 时的缺省叠加透明度。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_DEFAULT_OVERLAY_ALPHA 0.72f
/* y_boost <= 0 时的缺省荧光亮度增益。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_DEFAULT_Y_BOOST 1.15f
/* NV12 行 stride 要求的字节对齐。 */
#define MEDIA_FLUO_VISIBLE_FUSION_CL_STRIDE_ALIGN 64

/* 融合方法，数值与旧模块及 OpenCL kernel 保持一致。 */
typedef enum {
    /* 荧光区域以绿色叠加在可见光上。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_GREEN_OVERLAY = 0,
    /* 荧光区域以青色叠加在可见光上。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_CYAN_OVERLAY = 1,
    /* 荧光区域以热红/红色叠加在可见光上。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_HOT_OVERLAY = 2,
    /* 可见光与荧光的亮度算术平均，输出中性灰。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_AVERAGE_GRAY = 3,
    /* 5x5 低频基底 + 两路中较强高频细节，输出中性灰。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_LAPLACIAN_GRAY = 4,
} MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD;

/* 输入帧序号（SendFrame 的 input_id），也是端口编号。 */
typedef enum {
    /* 可见光图像，input0；其 Y/UV 都参与输出，时间戳也取自该路。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_VISIBLE = 0,
    /* 荧光图像，input1；仅 Y 亮度参与融合，UV 不参与。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_FLUORESCENCE = 1,
} MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT;

/* 创建参数。只支持 MEDIA_FORMAT_NV12、LINEAR（compress=0）输入输出。 */
typedef struct {
    /* 图像可见宽度（像素），必须为 2..8192 的偶数。 */
    int width;
    /* 图像可见高度（像素），必须为 2..8192 的偶数。 */
    int height;
    /* 输入输出像素格式，必须是 MEDIA_FORMAT_NV12。 */
    int format;
    /* 输入端口队列深度，<= 0 取 4。 */
    int input_depth;
    /* 输出 Buffer 的 Pool id，必须 >= 0。 */
    int output_pool_id;
    /* 输入 NV12 plane0 行字节，必须 >= width 且为 64 字节对齐。 */
    int input_stride;
    /* 输出 NV12 plane0 行字节，必须 >= width 且为 64 字节对齐。 */
    int output_stride;
    /* 融合方法，取 MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_*，只接受 0..4。 */
    int method;
    /* 荧光掩码平滑下限（归一化 0..1），<= 0 取 0.20。 */
    float thr_lo;
    /* 荧光掩码平滑上限（归一化 0..1），<= 0 取 0.65；不得小于 thr_lo。 */
    float thr_hi;
    /* 荧光最大值处的叠加透明度，<= 0 取 0.72，超过 1 时钳制为 1。 */
    float overlay_alpha;
    /* 荧光亮度增益，<= 0 取 1.15。 */
    float y_boost;
} MEDIA_FLUO_VISIBLE_FUSION_CL_ATTR;

/* 最近一帧的 GPU 耗时统计（GetLastPerf 输出）。 */
typedef struct {
    /* 最近一帧 GLES 绘制和 GPU 完成等待的墙钟耗时（毫秒）。 */
    double kernel_ms;
    /* GPU 端到端耗时（毫秒）；GLES 后端与 kernel_ms 相同。 */
    double gpu_total_ms;
} MEDIA_FLUO_VISIBLE_FUSION_CL_PERF;

/* 创建实例（grp 即实例 id，实例名 FLUO_VISIBLE_FUSION_CL_<grp>）。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_CreateGrp(
    int grp, const MEDIA_FLUO_VISIBLE_FUSION_CL_ATTR *attr);
/* 销毁实例，需先 Stop。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_DestroyGrp(int grp);
/* 启动处理线程与 GLES 上下文。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_Start(int grp);
/* 停止处理线程并释放 GLES 资源，可重复调用。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_Stop(int grp);
/* 等同 Start。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_Enable(int grp);
/* 等同 Stop。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_Disable(int grp);
/* 向 input_id（0=可见光，1=荧光）投递一帧，成功即移交所有权。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_SendFrame(
    int grp, int input_id, MEDIA_BUFFER buf, int timeout_ms);
/* 从 output0 取一帧，所有权归调用者，用后 ReleaseFrame。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_GetFrame(
    int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 GetFrame 得到的 Buffer；grp 参数为旧 API 兼容保留。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
/* 读取最近一帧的耗时统计；尚无成功帧时返回负值。 */
int MEDIA_FLUO_VISIBLE_FUSION_CL_GetLastPerf(
    int grp, MEDIA_FLUO_VISIBLE_FUSION_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_FLUO_VISIBLE_FUSION_H */

