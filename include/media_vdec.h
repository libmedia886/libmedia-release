#ifndef MEDIA_VDEC_H
#define MEDIA_VDEC_H

/*
 * VDEC（MPP 视频解码）外部模块 API，由 libmedia_vdec.so 提供。
 *
 * 迁自 libmedia 内置 VDEC（src/module/vdec/vdec.c + src/media_api/media_api_codec.c）。
 * 只依赖公开 SDK（media_api.h / media_plugin.h），MPP 是本插件私有依赖。
 * 两种用法：
 *   1) 直接链接 libmedia_vdec.so 并调用下面的 MEDIA_VDEC_* API（与旧内置 VDEC 源码兼容）；
 *   2) 作为插件 MEDIA_PLUGIN_Load("libmedia_vdec.so") 后用
 *      MEDIA_PLUGIN_CreateModule("VDEC", chn, &attr, sizeof(attr)) 创建。
 * 实例名为 "VDEC_<chn>"；has_input_port=1 时可直接 MEDIA_SYS_Bind(VENC/..., "output0",
 * "VDEC", chn, "input0")；has_input_port=0 时用 MEDIA_VDEC_SendPacket 手动送包。
 */

#include <stddef.h>
#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    MEDIA_BUFFER buffer;
    uint64_t lease_id;
} MEDIA_VDEC_FRAME;

typedef struct {
    int width;
    int height;
    int stride;     // 输出 byte pitch；NV15 必须显式使用 RK3588 MPP 要求的 pitch
    int buf_cnt;
    int video_type; // 视频格式 (MEDIA_VIDEO_*)
    int pool_id;    // 输出 buffer pool ID
    int has_input_port;  // 是否创建输入端口 (1=bind 模式，0=手动模式)
    int input_depth;     // 输入端口队列深度 (0=默认 64)
    int output_format;   // 0=NV12；H.265 Main10 可选 compact NV15，NV20/NV30/P010 不支持
    MEDIA_VIDEO_COMPRESS output_compress; // 0=LINEAR；当前8bit AFBC仅支持16x16(0x001)
} MEDIA_VDEC_ATTR;

/*
 * VDEC producer-specific external output-pool contract.  This query result is
 * intentionally limited to allocation requirements: the authoritative frame
 * layout remains the per-buffer MEDIA_BUFFER_META_VIDEO_FRAME metadata.
 */
typedef struct {
    size_t slot_size;
    int slot_count;
} MEDIA_VDEC_OUTPUT_POOL_REQUIREMENTS;

int MEDIA_VDEC_QueryOutputPoolRequirements(
    const MEDIA_VDEC_ATTR *attr,
    MEDIA_VDEC_OUTPUT_POOL_REQUIREMENTS *requirements);
int MEDIA_VDEC_CreateChn(int chn, const MEDIA_VDEC_ATTR *attr);
int MEDIA_VDEC_SetAttr(int chn, const MEDIA_VDEC_ATTR *attr);
int MEDIA_VDEC_DestroyChn(int chn);
int MEDIA_VDEC_Enable(int chn);
int MEDIA_VDEC_Disable(int chn);
int MEDIA_VDEC_Start(int chn);
int MEDIA_VDEC_Stop(int chn);
int MEDIA_VDEC_SendPacket(int chn, void *data, size_t size, uint64_t pts);
// GetFrame 返回 buffer 与 lease_id；ReleaseFrame 必须原样传回，同一 token 只能成功释放一次。
int MEDIA_VDEC_GetFrame(int chn, MEDIA_VDEC_FRAME *frame, int timeout_ms);
int MEDIA_VDEC_ReleaseFrame(int chn, MEDIA_VDEC_FRAME frame);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VDEC_H */
