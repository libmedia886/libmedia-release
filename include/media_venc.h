#ifndef MEDIA_VENC_H
#define MEDIA_VENC_H

/*
 * VENC 外部模块（libmedia_venc.so）公共头文件。
 *
 * 本模块由原 libmedia 内置 VENC 迁出，type 名仍为 "VENC"，MEDIA_VENC_* 接口与
 * 原内置版本源码兼容。使用前先 MEDIA_SYS_Init()；应用链接 libmedia_venc.so 并
 * 包含本头文件即可，模块首次调用时会自行向 libmedia 注册。
 *
 * 两种用法：
 *   1) 直接调用 MEDIA_VENC_CreateChn(chn, ..., output_pool_id, ...)；
 *   2) 作为插件 MEDIA_PLUGIN_CreateModule("VENC", chn, &attr, sizeof(attr))，其中
 *      attr 是下面的 MEDIA_VENC_ATTR；该结构没有 output_pool_id，创建后用
 *      MEDIA_PLUGIN_SetParam("VENC", chn, "output_pool_id", &pool_id, sizeof(int))
 *      设置，再 MEDIA_PLUGIN_Start("VENC", chn)。
 * 实例名为 "VENC_<chn>"，可用 MEDIA_SYS_Bind(..., "VENC", chn, "input0") 接收上游
 * 视频帧，或用 MEDIA_SYS_SendFrame("VENC", chn, "input0", ...) 手动送帧；
 * 未 Bind 输出时用 MEDIA_VENC_GetPacket/ReleasePacket 或 MEDIA_SYS_GetFrame 取码流。
 */

#include <stdint.h>
#include <stddef.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEDIA_VENC_RC_CBR = 0,        // 固定码率 - 保持恒定码率，适合网络流媒体
    MEDIA_VENC_RC_VBR = 1,        // 可变码率 - 根据画面复杂度调整，画质优先
    MEDIA_VENC_RC_FIXQP = 2,      // 固定 QP - 固定量化参数，画质稳定但码率波动大
    MEDIA_VENC_RC_AVBR = 3,       // 平均可变码率 - 长期平均码率可控，短期可波动
    MEDIA_VENC_RC_CBR_SHARE = 4,  // 共享 CBR - 多通道共享带宽的 CBR 模式
    MEDIA_VENC_RC_VBR_SHARE = 5,  // 共享 VBR - 多通道共享带宽的 VBR 模式
} MEDIA_VENC_RC_MODE;

typedef struct {
    int width;
    int height;
    int stride;     // bytesperline；0 表示默认=width（AFBC 压缩输入必须为 0）
    int fps;
    int buf_cnt;
    int input_depth;  // 输入端口队列深度（0 表示默认=4）
    int bitrate;
    int gop;
    int video_format;  // 编码格式 (MEDIA_FORMAT_H264/H265/MJPEG/JPEG/VP8)
    int rc_mode;
    int input_format;  // 当前 RK3588 MPP VENC 仅支持已验证的 8bit frame format；compact/RGB 10bit 会拒绝
    /* input0 exact storage contract: H.264/H.265 accept NV12 NONE,
     * AFBC16x16 0x001, or AFBC16x16+SPARSE 0x041; stride must be 0 for
     * either compressed tuple.  Other modifiers, 10bit and codecs remain
     * unsupported. */
    MEDIA_VIDEO_COMPRESS input_compress;
} MEDIA_VENC_ATTR;

/* SetAttr 可在线修改的字段：fps、bitrate、gop、rc_mode、input_format、
 * input_compress、stride、video_format。width/height/buf_cnt/input_depth
 * 只能在 CreateChn 时确定，SetAttr 传入与当前不同的值会返回
 * MEDIA_ERR_UNSUPPORTED（合法但当前不支持）。已 Bind 输入时进一步禁止
 * 修改 input_format/input_compress/stride。 */
int MEDIA_VENC_CreateChn(int chn, int width, int height, int stride, int fps,
                         int buf_cnt, int input_depth, int output_pool_id,
                         int bitrate, int gop, int video_format,
                         MEDIA_VIDEO_COMPRESS input_compress);
int MEDIA_VENC_SetAttr(int chn, const MEDIA_VENC_ATTR *attr);
int MEDIA_VENC_DestroyChn(int chn);
int MEDIA_VENC_Enable(int chn);
int MEDIA_VENC_Disable(int chn);
int MEDIA_VENC_Start(int chn);
int MEDIA_VENC_Stop(int chn);
int MEDIA_VENC_GetFrame(int chn, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VENC_ReleaseFrame(int chn, MEDIA_BUFFER buf);
int MEDIA_VENC_GetMpp(int chn, void **mpp_ctx, void **mpp_mpi);
/* GetPacket 成功后 pkt->handle 持有输出 buffer 的槽位，必须 ReleasePacket
 * 才能归还。ReleasePacket 失败（返回 MEDIA_ERR_RESOURCE）时 handle 保持有效，
 * 可再次调用重试。 */
int MEDIA_VENC_GetPacket(int chn, MEDIA_PACKET *pkt, int timeout_ms);
int MEDIA_VENC_ReleasePacket(int chn, MEDIA_PACKET *pkt);

/* VENC 扩展参数设置接口。
 * QP / BPS 参数为三态：-1 = 不修改，0 是合法取值（QP 范围 0~51）。 */
int MEDIA_VENC_SetRcMode(int chn, int rc_mode, int bitrate);
int MEDIA_VENC_SetFormat(int chn, int format);
int MEDIA_VENC_SetVideoType(int chn, int video_type);
int MEDIA_VENC_SetQpConfig(int chn, int qp_init, int qp_max, int qp_min);
int MEDIA_VENC_SetBpsConfig(int chn, int bps_max, int bps_min);

// H265 专属参数设置（布尔参数：0=关闭，1=打开）
int MEDIA_VENC_SetH265Profile(int chn, int profile);
int MEDIA_VENC_SetH265Level(int chn, int level);
int MEDIA_VENC_SetH265Tier(int chn, int tier);
int MEDIA_VENC_SetH265CuSize(int chn, int cu_size);
int MEDIA_VENC_SetH265SaoEn(int chn, int sao_en);
int MEDIA_VENC_SetH265DblkEn(int chn, int dblk_en);

// H264 专属参数设置（布尔参数：0=关闭，1=打开）
int MEDIA_VENC_SetH264Profile(int chn, int profile);
int MEDIA_VENC_SetH264Level(int chn, int level);
int MEDIA_VENC_SetH264CabacEn(int chn, int cabac_en);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VENC_H */
