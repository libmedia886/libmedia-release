#ifndef MEDIA_VI_H
#define MEDIA_VI_H

/*
 * VI（V4L2 / HDMI-RX 视频采集）外部模块 API，由 libmedia_vi.so 提供。
 *
 * 与原内置 VI 源码兼容：包含本头文件并链接 libmedia_vi.so 即可。实例名 "VI_<dev>"，
 * 输出端口 output0，可直接 MEDIA_SYS_Bind("VI", dev, "output0", ...)。
 * 也可以 MEDIA_PLUGIN_Load("libmedia_vi.so") 后 MEDIA_PLUGIN_CreateModule("VI", dev, &attr, sizeof(attr))。
 *
 * Buffer：VI 从 pool_id 取 buf_cnt 个 Buffer 以 V4L2_MEMORY_DMABUF 方式入队（零拷贝），每出一帧再
 * 从 Pool 补一个；Pool 的 count 至少为 buf_cnt + 下游在途数量，单个 Buffer 至少为驱动 sizeimage。
 *
 * Pool 必须是 CMA/连续内存：HDMI-RX 驱动导入 DMABUF 时只接受 CMA 的 dma-buf，默认的
 * MEDIA_POOL_Create（MEDIA_POOL_ALLOC_MODE_COMPAT，system-dma32）会在 QBUF 被拒（EFAULT）。
 * 请用 MEDIA_POOL_CreateWithMode(pool_id, size, count, MEDIA_POOL_ALLOC_MODE_NATIVE)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *device;
    int width;
    int height;
    int stride;     // plane0 bytesperline (0=driver default)
    int fps;
    int buf_cnt;
    int pool_id;
    int format;     // 像素格式 (MEDIA_FORMAT_*)
    MEDIA_VIDEO_COMPRESS output_compress; // output0 精确存储契约；当前仅支持 NONE/LINEAR
} MEDIA_VI_ATTR;

typedef enum {
    MEDIA_VI_SOURCE_EVENT_NONE = 0,
    MEDIA_VI_SOURCE_EVENT_CONNECTED = 1,
    MEDIA_VI_SOURCE_EVENT_DISCONNECTED = 2,
    MEDIA_VI_SOURCE_EVENT_FORMAT_CHANGED = 3,
    MEDIA_VI_SOURCE_EVENT_SOURCE_CHANGED = 4,
    MEDIA_VI_SOURCE_EVENT_TIMEOUT = 5,
} MEDIA_VI_SOURCE_EVENT_TYPE;

typedef struct {
    char device[64];
    char driver[32];
    char card[64];
    char bus_info[64];
    int is_hdmi_rx;
    int connected;          // HDMI RX power_present or detected DV timing
    int audio_present;
    int audio_sampling_rate;
    int width;
    int height;
    int fps_milli;          // 60000 表示 60.000fps；0 表示未知
    int format;             // MEDIA_FORMAT_*
    uint32_t fourcc;        // V4L2 fourcc
    char fourcc_name[5];    // printable fourcc, NUL terminated
    int plane_count;
    int stride;             // plane0 bytesperline
    int size_image;         // plane0 sizeimage
} MEDIA_VI_SOURCE_INFO;

typedef struct {
    MEDIA_VI_SOURCE_EVENT_TYPE type;
    MEDIA_VI_SOURCE_INFO before;
    MEDIA_VI_SOURCE_INFO after;
} MEDIA_VI_SOURCE_EVENT;

int MEDIA_VI_SetAttr(MEDIA_DEV dev, const MEDIA_VI_ATTR *attr);   // 创建 VI_<dev>（打开设备，未开始采集）
int MEDIA_VI_Enable(MEDIA_DEV dev);                               // S_FMT + 入队 + STREAMON
int MEDIA_VI_Disable(MEDIA_DEV dev);                              // STREAMOFF，归还驱动中的 Buffer
int MEDIA_VI_Destroy(MEDIA_DEV dev);                              // 关闭设备并销毁实例（需先解绑）
int MEDIA_VI_GetFrame(MEDIA_DEV dev, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VI_ReleaseFrame(MEDIA_DEV dev, MEDIA_BUFFER buf);
// 查询/等待视频源状态（HDMI-RX 插拔、分辨率变化），不依赖 VI 实例。
// WaitSourceChange：有变化返回 1，超时返回 0（event.type=TIMEOUT），失败返回 -1。
int MEDIA_VI_QuerySource(const char *device, MEDIA_VI_SOURCE_INFO *info);
int MEDIA_VI_WaitSourceChange(const char *device, int timeout_ms, MEDIA_VI_SOURCE_EVENT *event);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VI_H */
