#ifndef MEDIA_TPG_H
#define MEDIA_TPG_H

/*
 * TPG（测试图案源）外部模块 API，由 libmedia_tpg.so 提供。
 *
 * 这是第一个以外部模块形式交付的 libmedia 模块：只依赖公开 SDK
 * （media_api.h / media_plugin.h），自带本头文件与动态库。两种用法：
 *   1) 直接链接 libmedia_tpg.so 并调用下面的 MEDIA_TPG_* API（与旧内置 TPG 源码兼容）；
 *   2) 作为插件 MEDIA_PLUGIN_Load("libmedia_tpg.so") 后用
 *      MEDIA_PLUGIN_CreateModule("TPG", chn, &attr, sizeof(attr)) 创建。
 * 实例名为 "TPG_<chn>"，输出端口 output0，可直接 MEDIA_SYS_Bind("TPG", chn, "output0", ...)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEDIA_TPG_PATTERN_COLOR_BARS = 0,
    MEDIA_TPG_PATTERN_CHECKERBOARD = 1,
    MEDIA_TPG_PATTERN_GRADIENT = 2,
    MEDIA_TPG_PATTERN_SOLID = 3,
    MEDIA_TPG_PATTERN_MOVING_BOX = 4,
} MEDIA_TPG_PATTERN;

typedef struct {
    int width;       // required > 0
    int height;      // required > 0
    int stride;      // plane0 byte pitch，0=按格式最小值
    int format;      // MEDIA_FORMAT_NV12/YUV420SP_10BIT/YUV422SP_10BIT/YUV444SP_10BIT/RGB888/BGR888/RGBA/BGRA/ARGB/ABGR/GRAY8
    int fps;         // <=0 取 30
    int pool_id;     // required >= 0，单个 Buffer 至少为一帧大小
    int pattern;     // MEDIA_TPG_PATTERN_*
    int block_size;  // checker/moving-box 块大小，<=0 取默认
    uint32_t fg_color; // 0xRRGGBB
    uint32_t bg_color; // 0xRRGGBB
    MEDIA_VIDEO_COMPRESS output_compress; // 仅支持 NONE/LINEAR
} MEDIA_TPG_ATTR;

int MEDIA_TPG_CreateChn(int chn, const MEDIA_TPG_ATTR *attr);
int MEDIA_TPG_SetAttr(int chn, const MEDIA_TPG_ATTR *attr);   // 等同 CreateChn
int MEDIA_TPG_DestroyChn(int chn);
int MEDIA_TPG_Enable(int chn);
int MEDIA_TPG_Disable(int chn);
int MEDIA_TPG_Start(int chn);
int MEDIA_TPG_Stop(int chn);
int MEDIA_TPG_GetFrame(int chn, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_TPG_ReleaseFrame(int chn, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_TPG_H */
