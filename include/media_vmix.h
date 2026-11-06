#ifndef MEDIA_VMIX_H
#define MEDIA_VMIX_H

/*
 * VMIX（GLES fragment 多路视频混合/拼接）外部模块 API，由 libmedia_vmix.so 提供。
 *
 * 只依赖公开 SDK（media_api.h / media_plugin.h），自带本头文件与动态库。两种用法：
 *   1) 直接链接 libmedia_vmix.so 并调用下面的 MEDIA_VMIX_* API（与旧内置 VMIX 源码兼容）；
 *   2) 作为插件 MEDIA_PLUGIN_Load("libmedia_vmix.so") 后用
 *      MEDIA_PLUGIN_CreateModule("VMIX", grp, &attr, sizeof(attr)) 创建（config 即 MEDIA_VMIX_ATTR）。
 * 实例名为 "VMIX_<grp>"，输入端口 input0..input<input_count-1>（序号等于通道号），输出端口 output0，
 * 可直接 MEDIA_SYS_Bind("VMIX", grp, "output0", ...)。
 *
 * 本头文件中的结构体、枚举与函数声明与迁出前 media_api.h 中的 VMIX 段完全一致（源码级兼容）；
 * 之后只允许在结构体末尾追加字段。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// VMix (GLES fragment 视频混流模块)
#define MEDIA_VMIX_MAX_INPUTS 16

typedef struct {
    int enabled;              // 是否启用该通道（0=禁用，1=启用）
    int x;                    // X 坐标偏移
    int y;                    // Y 坐标偏移
    int width;                // 目标宽度（0=不缩放）
    int height;               // 目标高度（0=不缩放）
    float alpha;              // Alpha 透明度（0.0-1.0）
    int stride;               // LINEAR 输入 byte pitch（0 表示按输入格式默认对齐）
    int format;               // 输入格式 (NV12/NV15/NV20/NV30/RGB family，0 或 -1 表示使用全局默认格式)
    MEDIA_VIDEO_COMPRESS compress; // inputN精确存储契约；GLES对NV12/BGR888/ABGR8888开放共享AFBC输入矩阵
} MEDIA_VMIX_CHANNEL;

typedef struct {
    MEDIA_VMIX_CHANNEL channels[MEDIA_VMIX_MAX_INPUTS];
    int input_count;          // 输入通道数（1-MEDIA_VMIX_MAX_INPUTS）
    int output_width;         // 输出宽度
    int output_height;        // 输出高度
    int format;               // 单路 output0 输出格式和默认输入格式（NV12/NV15/NV20/NV30/RGB family）
    int input_depth;          // 输入端口队列深度（0 表示默认=4）
    int output_pool_id;       // 输出 buffer 池 ID
    int output_stride;        // LINEAR 输出 byte pitch（0 表示按输出格式默认对齐）
    MEDIA_VIDEO_COMPRESS output_compress; // output0精确存储契约；GLES对NV12/BGR888/ABGR8888开放共享AFBC输出矩阵
    int primary_index;        // 主输入索引 (-1 表示自动选择第一个启用的通道)
} MEDIA_VMIX_ATTR;

int MEDIA_VMIX_SetAttr(int grp, const MEDIA_VMIX_ATTR *attr);
int MEDIA_VMIX_CreateGrp(int grp, const MEDIA_VMIX_ATTR *attr);
int MEDIA_VMIX_DestroyGrp(int grp);
int MEDIA_VMIX_Start(int grp);
int MEDIA_VMIX_Stop(int grp);
int MEDIA_VMIX_Enable(int grp);
int MEDIA_VMIX_Disable(int grp);
int MEDIA_VMIX_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VMIX_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VMIX_H */
