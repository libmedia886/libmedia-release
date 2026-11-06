#ifndef MEDIA_EXPOSURE_FUSION_H
#define MEDIA_EXPOSURE_FUSION_H

/*
 * EXPOSURE_FUSION_CL（GLES 双输入曝光融合）应用 API，由 libmedia_exposure_fusion.so 提供。
 *
 * 插件 type 名仍为 "EXPOSURE_FUSION_CL"，实例名为 EXPOSURE_FUSION_CL_<grp>。端口由 SDK 固定命名为
 * input0（第一路）、input1（第二路）和 output0。应用可直接链接本库调用下面的 MEDIA_* API，也可通过
 * MEDIA_PLUGIN_Load 加载后使用 MEDIA_PLUGIN_CreateModule / MEDIA_SYS_Bind。
 *
 * 算法在 YUV 编码值域工作：先按局部对比度、色饱和度和曝光适度计算两路权重，再在 1/2 分辨率
 * 高斯低通上融合低频、按权重恢复高频细节，并用 2x2 块平均权重融合 U/V。输出只支持 NV12 LINEAR。
 */

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 插件类型名：MEDIA_SYS_Bind / MEDIA_PLUGIN_CreateModule 使用该名称。 */
#define MEDIA_EXPOSURE_FUSION_CL_TYPE "EXPOSURE_FUSION_CL"
/* input_depth = 0 时两路输入端口使用的默认队列深度。 */
#define MEDIA_EXPOSURE_FUSION_CL_DEFAULT_DEPTH 4
/* 旧模块支持的尺寸范围。 */
#define MEDIA_EXPOSURE_FUSION_CL_MIN_SIZE 2
#define MEDIA_EXPOSURE_FUSION_CL_MAX_SIZE 8192
/* NV12 输入/输出 stride 必须显式给出并按 64 字节对齐。 */
#define MEDIA_EXPOSURE_FUSION_CL_STRIDE_ALIGN 64

/*
 * 融合组属性。字段名称、类型和顺序与旧 master:include/media_api.h 的
 * MEDIA_EXPOSURE_FUSION_CL_ATTR 完全一致，便于旧应用源码直接切换头文件。
 */
typedef struct {
    int width;                    /* 图像宽（像素）：2..8192 且必须为偶数 */
    int height;                   /* 图像高（像素）：2..8192 且必须为偶数 */
    int format;                   /* 输入与输出格式：只支持 MEDIA_FORMAT_NV12 */
    int input_depth;              /* input0/input1 队列深度：0 = 默认 4，其余 1..64 */
    int output_pool_id;           /* 输出 buffer 所在 Pool：必须 >= 0，且容量至少一帧 NV12 */
    int input_stride;             /* 输入 plane0 pitch：必须显式 > 0、>= width 且 64 字节对齐 */
    int output_stride;            /* 输出 plane0 pitch：必须显式 > 0、>= width 且 64 字节对齐 */
    float contrast_power;         /* 局部对比度项指数：< 0 使用 1.0；0 使该项恒为 1 */
    float saturation_power;       /* 色饱和度项指数：< 0 使用 1.0；0 使该项恒为 1 */
    float exposedness_power;      /* 曝光适度项指数：< 0 使用 1.0；0 使该项恒为 1 */
    float sigma;                  /* 曝光适度高斯 sigma（Y 归一化值域）：<= 0 使用 0.2 */
    float epsilon;                /* 权重归一化与幂运算保护项：<= 0 使用 1e-6 */
} MEDIA_EXPOSURE_FUSION_CL_ATTR;

/* 创建融合组；同组已存在时返回 MEDIA_ERR_ALREADY_EXISTS。 */
int MEDIA_EXPOSURE_FUSION_CL_CreateGrp(int grp, const MEDIA_EXPOSURE_FUSION_CL_ATTR *attr);
/* 停止并销毁融合组，释放 last-frame 引用。 */
int MEDIA_EXPOSURE_FUSION_CL_DestroyGrp(int grp);
/* 启动插件工作线程和 GLES 上下文。 */
int MEDIA_EXPOSURE_FUSION_CL_Start(int grp);
/* 停止工作线程，等待 GPU 工作结束并归还输入引用。 */
int MEDIA_EXPOSURE_FUSION_CL_Stop(int grp);
/* 旧 API：等价于 Start。 */
int MEDIA_EXPOSURE_FUSION_CL_Enable(int grp);
/* 旧 API：等价于 Stop。 */
int MEDIA_EXPOSURE_FUSION_CL_Disable(int grp);
/* 从 output0 取一帧；使用完必须调用 MEDIA_EXPOSURE_FUSION_CL_ReleaseFrame。 */
int MEDIA_EXPOSURE_FUSION_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
/* 归还 GetFrame 得到的 buffer；grp 参数仅为旧 API 兼容而保留。 */
int MEDIA_EXPOSURE_FUSION_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_EXPOSURE_FUSION_H */

