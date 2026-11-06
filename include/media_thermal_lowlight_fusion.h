#ifndef MEDIA_THERMAL_LOWLIGHT_FUSION_H
#define MEDIA_THERMAL_LOWLIGHT_FUSION_H

/*
 * THERMAL_LOWLIGHT_FUSION_CL（热成像 + 低照度 NV12 融合，GLES 后端）外部模块 API，
 * 由 libmedia_thermal_lowlight_fusion.so 提供。
 *
 * 模块 type 名沿用旧内置模块的 "THERMAL_LOWLIGHT_FUSION_CL"，实例名为
 * "THERMAL_LOWLIGHT_FUSION_CL_<grp>"。输入端口 input0 为热成像、input1 为低照度，
 * 输出端口 output0。MEDIA_THERMAL_LOWLIGHT_FUSION_CL_* 函数名、参数、结构体名和
 * 字段顺序与旧 include/media_api.h 完全一致，应用可以源码级替换旧模块。
 *
 * 输出只取两路输入的 Y 平面做融合；热成像的 Y 同时决定热点区域的
 * 伪彩叠加（HOT_OVERLAY 模式）。时间戳与序号取 thermal（input0）帧。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 模块 type 名（MEDIA_SYS_Bind 的 mod 参数）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_TYPE "THERMAL_LOWLIGHT_FUSION_CL"
/* 热成像输入端口。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_THERMAL_PORT "input0"
/* 低照度输入端口。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_LOWLIGHT_PORT "input1"
/* 融合结果输出端口。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_OUTPUT_PORT "output0"

/* input_depth <= 0 时的输入队列深度。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_INPUT_DEPTH 4
/* pyramid_levels == 0 时的缺省层数（当前算法路径不使用，仅为兼容保留）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_PYRAMID_LEVELS 2
/* tif_base_radius == 0 时的缺省半径（当前算法路径不使用，仅为兼容保留）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_TIF_BASE_RADIUS 8
/* detail_base_radius == 0 时的缺省局部基底半径（像素）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_DETAIL_BASE_RADIUS 6
/* detail_gain 非法（<=0 或 >1）或为 0 时的缺省细节增益。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_DETAIL_GAIN 0.35f
/* detail_threshold 非法（<=0 或 >=1）或为 0 时的缺省归一化细节阈值。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_DETAIL_THRESHOLD 0.035f
/* max_luma_delta 非法（<=0 或 >1）或为 0 时的缺省最大亮度注入量。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_MAX_LUMA_DELTA 0.12f
/* thermal_weight 非法（<=0 或 >1）时的缺省热成像权重（保留字段）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_THERMAL_WEIGHT 0.5f
/* hot_threshold 非法（<=0 或 >=1）时的缺省热点阈值。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_HOT_THRESHOLD 0.65f
/* hot_soft_width 非法（<=0 或 >1）时的缺省热点过渡宽度。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_HOT_SOFT_WIDTH 0.15f
/* overlay_alpha 非法（<=0 或 >1）时的缺省伪彩叠加透明度。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DEFAULT_OVERLAY_ALPHA 0.70f
/* pyramid_levels 允许的最大值（当前算法路径不使用）。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MAX_PYRAMID_LEVELS 4
/* tif_base_radius / detail_base_radius 允许的最大值。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MAX_RADIUS 32
/* NV12 宽高的最小值。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MIN_SIZE 2
/* NV12 宽高的最大值。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MAX_SIZE 8192
/* NV12 行 stride 要求的字节对齐。 */
#define MEDIA_THERMAL_LOWLIGHT_FUSION_CL_STRIDE_ALIGN 64

/* 融合模式，数值与旧模块及 OpenCL kernel 保持一致。 */
typedef enum {
    /* 只输出融合后的 Y，UV 恒为 128（中性灰）。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE_GRAY = 0,
    /* 在 Y 融合之外，用热成像 Y 超过阈值的热点区域叠加红/黑色调。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE_HOT_OVERLAY = 1,
    /* 旧别名，与 HOT_OVERLAY 完全等价。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE_BLACK_RED =
        MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE_HOT_OVERLAY,
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE;

/* 算法路径。当前只实现 PYRAMID（thermal-detail）路径，TIF 已废弃并被拒绝。 */
typedef enum {
    /* 当前 thermal-detail 融合路径（默认）。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ALGO_PYRAMID = 0,
    /* 旧 TIF 路径，已废弃：创建时返回 MEDIA_ERR_UNSUPPORTED。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ALGO_TIF = 1,
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ALGO;

/* 输入帧序号（SendFrame 的 input_id），也是端口编号。 */
typedef enum {
    /* 热成像图像，input0；其 Y 参与细节融合与热点判定，时间戳也取自该路。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_THERMAL = 0,
    /* 低照度图像，input1；其 Y 作为融合基准亮度。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_LOWLIGHT = 1,
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT;

/* 输出 Y 的来源。 */
typedef enum {
    /* 直接使用低照度 Y（UV 由 mode 决定）。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_LUMA_LOWLIGHT = 0,
    /* 低照度 Y 加上从热成像局部细节中提取的增量。 */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_LUMA_THERMAL_DETAIL = 1,
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_LUMA_MODE;

/*
 * 融合组属性。字段名称、类型与顺序与旧 include/media_api.h 完全一致，
 * 新增字段只允许追加在末尾。只支持 MEDIA_FORMAT_NV12、LINEAR 输入输出。
 */
typedef struct {
    /* 图像可见宽度（像素）：2..8192 且为偶数；输入输出共用。 */
    int width;
    /* 图像可见高度（像素）：2..8192 且为偶数；输入输出共用。 */
    int height;
    /* 输入输出像素格式，必须为 MEDIA_FORMAT_NV12。 */
    int format;
    /* 输入端口队列深度，<= 0 取 4，最大 64。 */
    int input_depth;
    /* 输出 Buffer 的 Pool id，必须 >= 0（不随输入 Pool 自动选择）。 */
    int output_pool_id;
    /* 输入 NV12 plane0 行字节，必须 >= width 且为 64 字节对齐。 */
    int input_stride;
    /* 输出 NV12 plane0 行字节，必须 >= width 且为 64 字节对齐。 */
    int output_stride;
    /* 融合模式，取值见 MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE。 */
    int mode;
    /* 热成像基准权重，取值 (0,1]，非法时取 0.5；当前 thermal-detail 路径不使用。 */
    float thermal_weight;
    /* 热点阈值，取值 (0,1)，非法时取 0.65；仅 HOT_OVERLAY 模式使用。 */
    float hot_threshold;
    /* 热点阈值过渡宽度，取值 (0,1]，非法时取 0.15。 */
    float hot_soft_width;
    /* 伪彩叠加透明度，取值 (0,1]，非法时取 0.70。 */
    float overlay_alpha;
    /* 金字塔层数，0 取 2，允许 1..4；已废弃，保留源码兼容。 */
    int pyramid_levels;
    /* 算法路径：必须为 0（PYRAMID），1（TIF）在创建时被拒绝。 */
    int algo;
    /* TIF 基底半径，0 取 8，允许 1..32；已废弃，保留源码兼容。 */
    int tif_base_radius;
    /* Y 输出方式，取值见 MEDIA_THERMAL_LOWLIGHT_FUSION_CL_LUMA_MODE。 */
    int luma_mode;
    /* 热成像细节增益，取值 (0,1]，非法时取 0.35。 */
    float detail_gain;
    /* 归一化热成像细节阈值，取值 (0,1)，非法时取 0.035。 */
    float detail_threshold;
    /* 单像素 Y 最大注入量，取值 (0,1]，非法时取 0.12。 */
    float max_luma_delta;
    /* 局部基底 box 半径（像素），0 取 6，允许 1..32。 */
    int detail_base_radius;
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ATTR;

/* 最近一帧的分阶段耗时（毫秒）。未成功出帧时各值为 -1。 */
typedef struct {
    /* Y 融合阶段耗时：两路 Y 提取 + 水平/垂直 box 基底。 */
    double y_kernel_ms;
    /* UV/输出阶段耗时：Y 与 UV 一次写出。 */
    double uv_kernel_ms;
    /* 两阶段之和（GPU 完成等待后的墙钟时间，非硬件计数器）。 */
    double gpu_total_ms;
} MEDIA_THERMAL_LOWLIGHT_FUSION_CL_PERF;

/* 创建融合组；同组已存在时返回错误，不隐式重建。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_CreateGrp(
    int grp,
    const MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ATTR *attr);
/* 销毁融合组，释放 GLES 上下文与所有持有的 buffer 引用。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DestroyGrp(int grp);
/* 启动处理线程并初始化 GLES 上下文。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Start(int grp);
/* 停止处理线程并释放 GLES 上下文。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Stop(int grp);
/* 与 Start 等价（旧 API 别名）。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Enable(int grp);
/* 与 Stop 等价（旧 API 别名）。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Disable(int grp);
/* 向 input_id（0=thermal，1=lowlight）送入一帧 NV12。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_SendFrame(
    int grp,
    int input_id,
    MEDIA_BUFFER buf,
    int timeout_ms);
/* 从 output0 取一帧融合结果，使用完必须调用 ReleaseFrame。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 GetFrame 得到的 buffer（与组是否存在无关，旧语义）。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
/* 读取最近一帧的分阶段耗时；尚未成功出帧时返回 MEDIA_ERR_UNSUPPORTED。 */
int MEDIA_THERMAL_LOWLIGHT_FUSION_CL_GetLastPerf(
    int grp,
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_PERF *perf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_THERMAL_LOWLIGHT_FUSION_H */
