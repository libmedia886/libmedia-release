#ifndef MEDIA_BLEND_PYR_H
#define MEDIA_BLEND_PYR_H

/*
 * BLEND_PYR（金字塔融合，GLES 外部模块）应用 API，由 libmedia_blend_pyr.so 提供。
 *
 * 实例名为 "BLEND_PYR_<grp>"。插件端口由 SDK 统一命名，融合组固定为
 * input0（第一路）、input1（第二路）、input2（mask 输入，可选）与 output0；
 * 应用侧用 MEDIA_SYS_Bind("BLEND_PYR", grp, "input0", ...) 等名称绑定。
 * 旧内置模块的 mask 端口名 "mask_input" 无法沿用，原因见 MIGRATION_NOTES.md。
 *
 * 融合公式在 YUV 编码值域逐通道计算（Y/U/V 各自独立，不使用颜色矩阵转换）：
 *     out = clamp(in1 + (in0 - in1) * mask.Y, 0, 1)
 * 即 mask.Y = 0 时取 input1，mask.Y = 255 时取 input0，中间值线性混合。
 * mask 只使用 Y 平面，U/V 平面被忽略。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 模块类型名：MEDIA_SYS_Bind / MEDIA_PLUGIN_CreateModule 使用该名称。 */
#define MEDIA_BLEND_PYR_TYPE "BLEND_PYR"
/* input_depth = 0 时的输入端口队列深度。 */
#define MEDIA_BLEND_PYR_DEFAULT_DEPTH 4

/* 与旧模块一致的尺寸/对齐约束。 */
#define MEDIA_BLEND_PYR_MIN_WIDTH 16
#define MEDIA_BLEND_PYR_MAX_WIDTH 8192
#define MEDIA_BLEND_PYR_MIN_HEIGHT 16
#define MEDIA_BLEND_PYR_MAX_HEIGHT 8192
#define MEDIA_BLEND_PYR_WIDTH_ALIGN 16
#define MEDIA_BLEND_PYR_HEIGHT_ALIGN 2
#define MEDIA_BLEND_PYR_STRIDE_ALIGN 16

/*
 * 融合组属性。前 6 个字段与旧模块 include/media_api.h 的 MEDIA_BLEND_PYR_ATTR
 * 完全一致（名称、类型、顺序），新增字段只允许追加在末尾。
 */
typedef struct {
    int width;         /* 图像宽（像素）：16..8192 且 16 对齐；输入输出共用 */
    int height;        /* 图像高（像素）：16..8192 且 2 对齐；输入输出共用 */
    int input_stride;  /* 输入 plane0 每行字节数：0 = 按 width 做 16 字节对齐；
                          非 0 时必须 >= width 且 16 对齐 */
    int input_depth;   /* 输入端口队列深度（input0/input1/input2 共用）：
                          0 = MEDIA_BLEND_PYR_DEFAULT_DEPTH，其余 1..64 */
    int input_format;  /* 输入格式：只支持 MEDIA_FORMAT_NV12（输出同为 NV12） */
    int output_stride; /* 输出 plane0 每行字节数：0 = 等于 input_stride；
                          非 0 时必须 >= width 且 16 对齐 */
    int output_pool_id; /* 【新增字段】输出 buffer 池：>0 使用指定 Pool（尺寸须
                           >= 一帧 NV12 大小）；<=0 沿用 input0 所在 Pool（旧行为） */
} MEDIA_BLEND_PYR_ATTR;

/* 创建或重建融合组（已存在时先销毁旧实例，语义同旧模块 SetAttr）。 */
int MEDIA_BLEND_PYR_SetAttr(int grp_id, const MEDIA_BLEND_PYR_ATTR *attr);
/* 销毁融合组并释放其持有的 mask 引用。 */
int MEDIA_BLEND_PYR_DestroyGrp(int grp_id);
/* 启动处理线程并初始化 GLES 上下文。 */
int MEDIA_BLEND_PYR_Enable(int grp_id);
/* 停止处理线程并释放 GLES 上下文。 */
int MEDIA_BLEND_PYR_Disable(int grp_id);
/* 从 output0 取一帧融合结果，使用完必须调用 MEDIA_BLEND_PYR_ReleaseFrame。 */
int MEDIA_BLEND_PYR_GetFrame(int grp_id, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 MEDIA_BLEND_PYR_GetFrame 得到的 buffer。 */
int MEDIA_BLEND_PYR_ReleaseFrame(int grp_id, MEDIA_BUFFER buf);
/*
 * 设置融合 mask（8bit NV12，visible 尺寸须等于 width/height，只使用 Y 平面）。
 * 模块内部自持一份引用，调用方仍按自己的引用计数释放 *mask_buf。
 * 传入 NULL 或非法 buffer 返回 MEDIA_ERR_INVALID_PARAM。
 */
int MEDIA_BLEND_PYR_SetMask(int grp_id, MEDIA_BUFFER *mask_buf);
/* 返回 mask 状态：1 = 已设置，0 = 未设置或模块不存在。 */
int MEDIA_BLEND_PYR_GetMaskStatus(int grp_id);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_BLEND_PYR_H */

