#ifndef MEDIA_VO_WBC_H
#define MEDIA_VO_WBC_H

/*
 * VO_WBC（DRM writeback 回写抓屏：把某个 CRTC 正在显示的内容抓成帧）外部模块 API，
 * 由 libmedia_vo_wbc.so 提供。与迁出前 media_api.h 中的 MEDIA_VO_WBC_* 声明逐字相同：
 * 包含本头文件并链接 libmedia_vo_wbc.so 即可，函数名、参数和结构体布局不变。
 * 模块 type 名 "VO_WBC"，实例名 "VO_WBC_<chn>"，单个输出端口 output0（SOURCE），
 * 可 MEDIA_SYS_Bind("VO_WBC", chn, "output0", ...) 也可用 MEDIA_VO_WBC_GetFrame 手动取帧。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *device;     // DRM card，NULL 表示 /dev/dri/card0
    const char *target;     // 显示 connector 名称，如 DSI-1 / HDMI-A-1；NULL 表示自动找 active CRTC
    int width;              // 抓取宽度按格式对齐：ARGB8888/AR24 16 像素，NV12/BGR888 64 像素
    int height;             // 抓取高度，VO_WBC 不额外对齐
    int stride;             // bytesperline，<=0 自动；显式配置必须匹配 width/format
    int fps;                // <=0 不做帧率限制
    int format;             // MEDIA_FORMAT_NV12 / BGR888 / ARGB8888
    int pool_id;            // 输出 pool id
    int output_depth;       // 输出端口队列深度，<=0 默认 4
} MEDIA_VO_WBC_ATTR;

int MEDIA_VO_WBC_Probe(const char *device, int *connector_id, int *crtc_id);
int MEDIA_VO_WBC_CreateChn(int chn, const MEDIA_VO_WBC_ATTR *attr);
int MEDIA_VO_WBC_DestroyChn(int chn);
int MEDIA_VO_WBC_Start(int chn);
int MEDIA_VO_WBC_Stop(int chn);
int MEDIA_VO_WBC_GetFrame(int chn, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VO_WBC_ReleaseFrame(int chn, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VO_WBC_H */
