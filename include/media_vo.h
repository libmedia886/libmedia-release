#ifndef MEDIA_VO_H
#define MEDIA_VO_H

/*
 * VO（DRM/KMS 显示输出）外部模块 API，由 libmedia_vo.so 提供。
 * 与原内置 VO 源码兼容：包含本头文件并链接 libmedia_vo.so，函数名、参数和结构体布局不变。
 * 模块 type 名 "VO"，实例名 "VO_<output_id>"，输入端口 input<chn>，
 * 可直接 MEDIA_SYS_Bind("<src>", id, "outputN", "VO", output_id, "input<chn>")。
 * 本头文件的声明与迁出前 media_api.h 中 "// VO" 段逐字相同（只追加字段规则见 README.md）。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// VO
typedef enum {
    MEDIA_VO_INTF_HDMI = 0,
    MEDIA_VO_INTF_DP = 1,
    MEDIA_VO_INTF_EDP = 2,
    MEDIA_VO_INTF_MIPI = 3,
} MEDIA_VO_INTF;

typedef enum {
    MEDIA_VO_PLANE_TYPE_AUTO = 0,     // 自动选择 (overlay 优先，无 overlay 则用 primary)
    MEDIA_VO_PLANE_TYPE_OVERLAY = 1,  // 必须是 overlay plane
    MEDIA_VO_PLANE_TYPE_PRIMARY = 2,  // 必须是 primary plane
} MEDIA_VO_PLANE_TYPE;

/* VO 时序策略：默认 Negotiate（扩展字段为 0 时即此值） */
typedef enum {
    MEDIA_VO_MODE_NEGOTIATE = 0, // sink 支持配置 mode 才可用；不自动改分辨率
    MEDIA_VO_MODE_FORCE = 1,     // 强制配置 mode；未插线也允许输出路径
} MEDIA_VO_MODE_POLICY;

/* 显示所有权：同一 output 同时只允许一个 master */
typedef enum {
    MEDIA_VO_DISPLAY_OWNER_VO = 0,       // VO 自己 modeset / HPD 恢复（默认）
    MEDIA_VO_DISPLAY_OWNER_EXTERNAL = 1, // 外部（X11 等）管分辨率；VO 不 modeset，但仍可 DRM plane commit 上屏
} MEDIA_VO_DISPLAY_OWNER;

/* HDMI SDR 链路位深；AUTO 不强制改写 DRM color_depth。 */
typedef enum {
    MEDIA_VO_COLOR_DEPTH_AUTO = 0,
    MEDIA_VO_COLOR_DEPTH_8BPC = 8,
    MEDIA_VO_COLOR_DEPTH_10BPC = 10,
} MEDIA_VO_COLOR_DEPTH;

/* HDMI SDR 输出编码；AUTO 不强制改写 DRM color_format。 */
typedef enum {
    MEDIA_VO_COLOR_FORMAT_AUTO = 0,
    MEDIA_VO_COLOR_FORMAT_RGB = 1,
    MEDIA_VO_COLOR_FORMAT_YCBCR444 = 2,
    MEDIA_VO_COLOR_FORMAT_YCBCR422 = 3,
    MEDIA_VO_COLOR_FORMAT_YCBCR420 = 4,
} MEDIA_VO_COLOR_FORMAT;

typedef struct {
    MEDIA_VO_COLOR_DEPTH depth;
    MEDIA_VO_COLOR_FORMAT format;
} MEDIA_VO_COLOR_ATTR;

typedef struct {
    MEDIA_VO_COLOR_DEPTH requested_depth;
    MEDIA_VO_COLOR_FORMAT requested_format;
    MEDIA_VO_COLOR_DEPTH actual_depth;
    MEDIA_VO_COLOR_FORMAT actual_format;
    int actual_valid; // 1=actual_* 来自当前 DRM connector 属性回读
} MEDIA_VO_COLOR_INFO;

typedef enum {
    MEDIA_VO_EVENT_HOTPLUG = 1,
    MEDIA_VO_EVENT_NO_PLUG = 2,
    MEDIA_VO_EVENT_EDID_FAIL = 3,
    MEDIA_VO_EVENT_MODE_UNSUPPORTED = 4,
    MEDIA_VO_EVENT_MODE_CHANGED = 5,
    MEDIA_VO_EVENT_MODESET_FAIL = 6,
} MEDIA_VO_EVENT;

typedef enum {
    MEDIA_VO_EVENT_SOURCE_VO = 0,
    MEDIA_VO_EVENT_SOURCE_EXTERNAL = 1,
} MEDIA_VO_EVENT_SOURCE;

typedef struct {
    int width;
    int height;
    int fps; // <=0 表示按默认 60 解释（配置侧）；caps 列表中为实际值
} MEDIA_VO_MODE;

typedef struct {
    int output_id;
    MEDIA_VO_INTF intf;
    MEDIA_VO_EVENT event;
    MEDIA_VO_EVENT_SOURCE source; // 事件来源：VO 自身或外部 master
    int requested_w;
    int requested_h;
    int requested_fps;
    int actual_w;
    int actual_h;
    int actual_fps;
    int actual_valid; // 1=actual_* 有效
    int error_code;
} MEDIA_VO_EVENT_INFO;

typedef void (*MEDIA_VO_EVENT_CB)(const MEDIA_VO_EVENT_INFO *info, void *user);

typedef struct {
    MEDIA_VO_INTF intf;
    const char *connector;         // 必填 DRM connector 名，如 "HDMI-A-1" / "HDMI-A-2" / "DSI-1"
    int width;
    int height;
    int plane_count;
    int fps;                       // <=0 默认 60
    MEDIA_VO_MODE_POLICY policy;   // 默认 NEGOTIATE
} MEDIA_VO_ATTR;

typedef struct {
    MEDIA_VO_INTF intf;
    char connector[32];            // 已绑定 connector 名
    int connector_id;              // DRM connector id
    int crtc_id;
    int width;
    int height;
    int fps;
    int plane_count;
    MEDIA_VO_MODE_POLICY policy;
    int sink_connected;
    MEDIA_VO_DISPLAY_OWNER display_owner;
} MEDIA_VO_OUTPUT_INFO;

typedef struct {
    char name[32];                 // DRM 标准名，如 HDMI-A-1
    MEDIA_VO_INTF intf;
    int connector_id;
    int connected;                 // 1=connected
    int width;                     // preferred/first mode，无则 0
    int height;
    int fps;
} MEDIA_VO_CONNECTOR_INFO;

/*
 * 当前 output 上可直接 scanout 的 channel 候选。
 * 结果是查询时刻的快照；CreateChn/首帧仍会重新校验 DRM 资源与真实 layout。
 */
typedef struct {
    int format;                    // 逻辑 MEDIA_FORMAT_*
    MEDIA_VIDEO_COMPRESS compress; // 精确 LINEAR/AFBC 契约
    int drm_plane_id;
    MEDIA_VO_PLANE_TYPE plane_type;
    uint32_t drm_fourcc;
    uint64_t drm_modifier;
} MEDIA_VO_CHN_CAP;

int MEDIA_VO_SetAttr(int output_id, const MEDIA_VO_ATTR *attr);
int MEDIA_VO_SetDisplayOwner(int output_id, MEDIA_VO_DISPLAY_OWNER owner);
int MEDIA_VO_GetDisplayOwner(int output_id, MEDIA_VO_DISPLAY_OWNER *owner);
/* 仅 VO-owned HDMI 可设置；首期为 SDR，不生成 HDR metadata/PQ/BT.2020。 */
int MEDIA_VO_SetColorAttr(int output_id, const MEDIA_VO_COLOR_ATTR *attr);
/* VO/EXTERNAL owner 均可查询；actual 值来自 DRM connector，不复制 requested。 */
int MEDIA_VO_GetColorInfo(int output_id, MEDIA_VO_COLOR_INFO *info);
/* EXTERNAL owner：外部（如 X11）完成分辨率切换后通知库跟随；不执行 DRM modeset，不影响 plane commit 上屏 */
int MEDIA_VO_NotifyExternalMode(int output_id, int width, int height, int fps);
int MEDIA_VO_GetOutput(int output_id, MEDIA_VO_INTF *intf, int *width, int *height, int *plane_count);
int MEDIA_VO_GetOutputInfo(int output_id, MEDIA_VO_OUTPUT_INFO *info);
int MEDIA_VO_ProbeConnectors(MEDIA_VO_CONNECTOR_INFO *list, int max_n, int *out_count);
int MEDIA_VO_GetActualMode(int output_id, MEDIA_VO_MODE *mode);
int MEDIA_VO_GetSinkCaps(int output_id, MEDIA_VO_MODE *modes, int max_n, int *out_count);
int MEDIA_VO_RegisterEventCb(int output_id, MEDIA_VO_EVENT_CB cb, void *user);
/*
 * caps=NULL,max_caps=0 查询所需数量；列表按 plane 逐项返回。
 * plane_type/force_plane 与 CreateChn 含义一致，force_plane=0 表示不强制。
 */
int MEDIA_VO_QueryChnCaps(int output_id, MEDIA_VO_PLANE_TYPE plane_type,
                          int force_plane, MEDIA_VO_CHN_CAP *caps,
                          int max_caps, int *out_count);
int MEDIA_VO_CreateChn(int output_id, int chn, int x, int y, int width, int height, int stride, int depth,
                       MEDIA_VO_PLANE_TYPE plane_type, int format,
                       MEDIA_VIDEO_COMPRESS compress, int force_plane);
int MEDIA_VO_DestroyChn(int output_id, int chn);
int MEDIA_VO_Start(int output_id, int chn);
int MEDIA_VO_Stop(int output_id, int chn);
/* 从 VO input 端口取帧（少用；正常显示走 DRM plane commit，无需 GetFrame）。
 * EXTERNAL 与 VO owner 均走同一取帧语义；ReleaseFrame 必须配对。 */
int MEDIA_VO_GetFrame(int output_id, int chn, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VO_ReleaseFrame(int output_id, int chn, MEDIA_BUFFER buf);
int MEDIA_VO_FreezePlane(int output_id, int plane_id, int freeze);
int MEDIA_VO_HidePlane(int output_id, int plane_id, int hide);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VO_H */
