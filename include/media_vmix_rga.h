#ifndef MEDIA_VMIX_RGA_H
#define MEDIA_VMIX_RGA_H

/*
 * VMIX_RGA（RGA 硬件多路视频合成）外部模块 API，由 libmedia_vmix_rga.so 提供。
 *
 * 与原内置 VMIX_RGA 源码兼容：函数名、参数、结构体字段顺序均保持不变，原先
 * include "media_api.h" 即可见的部分改为 include 本头文件。实例名仍为
 * "VMIX_RGA_<grp>"，端口 input0..N（对应通道）与 output0，因此
 * MEDIA_SYS_Bind("VMIX_RGA", grp, "output0", ...) 的规则与迁移前一致。
 *
 * 两种用法：
 *   1) 直接链接 libmedia_vmix_rga.so 调用 MEDIA_VMIX_RGA_*（与旧内置 API 相同）；
 *   2) 作为插件 MEDIA_PLUGIN_Load("libmedia_vmix_rga.so") 后用
 *      MEDIA_PLUGIN_CreateModule("VMIX_RGA", grp, &attr, sizeof(attr)) 创建。
 *
 * 限制：output0 只支持 LINEAR（output_compress 必须为 0）；输入可为 LINEAR，或 NV12
 * 的 AFBC16x16 0x001 / 0x041。逐项规则见本仓库 README.md。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_VMIX_RGA_MAX_INPUTS 16

/* VMix RGA 通道配置 */
typedef struct {
    int enabled;                /* 是否启用该通道 */
    int x;                      /* X 坐标偏移 */
    int y;                      /* Y 坐标偏移 */
    int width;                  /* 目标宽度 (0 表示不缩放) */
    int height;                 /* 目标高度 (0 表示不缩放) */
    int stride;                 /* LINEAR 输入 byte pitch (0 表示按格式推导) */
    int format;                 /* 输入格式 (MEDIA_FORMAT_NV12, MEDIA_FORMAT_RGB888, MEDIA_FORMAT_RGBA8888)，-1 表示使用全局格式 */
    MEDIA_VIDEO_COMPRESS compress; /* inputN精确契约；NV12支持NONE/0x001/0x041，AFBC时stride=0 */
} MEDIA_VMIX_RGA_CHANNEL;

/* VMix RGA 属性 */
typedef struct {
    MEDIA_VMIX_RGA_CHANNEL channels[MEDIA_VMIX_RGA_MAX_INPUTS];  /* 通道配置 */
    int input_count;            /* 输入通道数 (1-16) */
    int output_width;           /* 输出宽度 */
    int output_height;          /* 输出高度 */
    int format;                 /* 格式 */
    int input_depth;            /* 输入端口队列深度（0 表示默认=4） */
    int output_pool_id;         /* 输出 buffer 池 ID */
    int output_stride;          /* LINEAR 输出 byte pitch (0 表示按格式推导) */
    MEDIA_VIDEO_COMPRESS output_compress; /* output0精确契约；最终有向矩阵仅支持NONE/LINEAR */
    int primary_index;          /* 主输入索引 (-1 表示自动选择第一个启用的通道) */
    int use_rga_compose;        /* 使用 RGA compose 算法 (0=逐个 blit, 1=使用 imcomposite) */
} MEDIA_VMIX_RGA_ATTR;

int MEDIA_VMIX_RGA_CreateGrp(int grp, const MEDIA_VMIX_RGA_ATTR *attr);
int MEDIA_VMIX_RGA_DestroyGrp(int grp);
int MEDIA_VMIX_RGA_Start(int grp);
int MEDIA_VMIX_RGA_Stop(int grp);
int MEDIA_VMIX_RGA_Enable(int grp);
int MEDIA_VMIX_RGA_Disable(int grp);
int MEDIA_VMIX_RGA_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VMIX_RGA_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VMIX_RGA_H */
