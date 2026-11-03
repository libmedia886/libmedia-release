#ifndef MEDIA_API_H
#define MEDIA_API_H

/*
 * libmedia 公共应用 API。libmedia 以 libmedia.so 二进制形式发布，
 * 使用条款见随附 LICENSE.TXT。扩展模块通过插件实现，开发接口见 media_plugin.h。
 *
 * 0.8.0 起 core 只保留框架（pipeline / buffer / pool / plugin_host / GPU 服务），
 * 模块全部是外部插件：OSD / VMIX_RGA 等模块的 API 移到各自插件仓库的公共头文件
 * （media_osd.h / media_vmix_rga.h），应用改为链接对应插件 .so。
 */
#include <stdint.h>
#include <stddef.h>
#include <linux/dma-buf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_API_VERSION_MAJOR 0
#define MEDIA_API_VERSION_MINOR 8
#define MEDIA_API_VERSION_PATCH 0
#define MEDIA_API_VERSION_STR "0.8.0"

typedef int MEDIA_DEV;
typedef int MEDIA_CHN;

typedef enum {
    MEDIA_STATUS_OK = 0,
    MEDIA_ERR_INVALID_PARAM = -1,
    MEDIA_ERR_MODULE_NOT_FOUND = -1001,
    MEDIA_ERR_PORT_NOT_FOUND = -1002,
    MEDIA_ERR_PORT_DIRECTION = -1003,
    MEDIA_ERR_PORT_INCOMPATIBLE = -1004,
    MEDIA_ERR_PIPELINE_STATE = -1005,
    MEDIA_ERR_RESOURCE = -1006,
    MEDIA_ERR_ALREADY_EXISTS = -1007,
    MEDIA_ERR_NOT_FOUND = -1008,
    MEDIA_ERR_UNSUPPORTED = -1009,
    MEDIA_ERR_BUSY = -1010,
    MEDIA_ERR_ABI_MISMATCH = -1011,
    MEDIA_ERR_TIMEOUT = -1012,
} MEDIA_STATUS;

typedef struct {
    int pool_id;
    int index;
} MEDIA_BUFFER;

typedef enum {
    MEDIA_BUFFER_META_NONE = 0,
    MEDIA_BUFFER_META_VIDEO_FRAME,
    MEDIA_BUFFER_META_AUDIO_FRAME,
    MEDIA_BUFFER_META_AUDIO_STREAM,
} MEDIA_BUFFER_META_TYPE;

/*
 * 视频压缩只描述存储布局，不替代 MEDIA_FORMAT 的像素语义。
 * 0 始终表示 LINEAR/NONE；非零 AFBC 值由一个 block 值和若干 flag 组合。
 */
typedef uint64_t MEDIA_VIDEO_COMPRESS;

#define MEDIA_VIDEO_COMPRESS_NONE       UINT64_C(0)

/*
 * VPSS sample-domain precision.  Zero initialization intentionally means
 * 8-bit SDR.  There is no AUTO value and this enum does not describe HDR
 * transfer functions, gamut, matrix coefficients, or full/limited range.
 */
typedef enum {
    MEDIA_DYNAMIC_RANGE_SDR8 = 0,
    MEDIA_DYNAMIC_RANGE_SDR10 = 1,
} MEDIA_DYNAMIC_RANGE;

/*
 * Explicit VPSS GLES color encoding contract.  YUV formats use one of the
 * BT.601/BT.709 matrix+range values; packed RGB formats use RGB_FULL or
 * RGB_LIMITED.  There is deliberately no AUTO value: callers must not infer
 * colorimetry from resolution, format, or MEDIA_DYNAMIC_RANGE.  Since API
 * 0.1.35 packed RGB callers must select an RGB value explicitly.
 */
typedef enum {
    MEDIA_COLOR_SPACE_BT601_LIMITED = 0,
    MEDIA_COLOR_SPACE_BT601_FULL = 1,
    MEDIA_COLOR_SPACE_BT709_LIMITED = 2,
    MEDIA_COLOR_SPACE_BT709_FULL = 3,
    MEDIA_COLOR_SPACE_RGB_FULL = 4,
    MEDIA_COLOR_SPACE_RGB_LIMITED = 5,
} MEDIA_COLOR_SPACE;

#define MEDIA_AFBC_BLOCK_MASK            UINT64_C(0x0f)
#define MEDIA_AFBC_BLOCK_16X16           UINT64_C(0x01)
#define MEDIA_AFBC_BLOCK_32X8            UINT64_C(0x02)
#define MEDIA_AFBC_BLOCK_64X4            UINT64_C(0x03)
#define MEDIA_AFBC_BLOCK_32X8_64X4       UINT64_C(0x04)
#define MEDIA_AFBC_YTR                   (UINT64_C(1) << 4)
#define MEDIA_AFBC_SPLIT                 (UINT64_C(1) << 5)
#define MEDIA_AFBC_SPARSE                (UINT64_C(1) << 6)
#define MEDIA_AFBC_CBR                   (UINT64_C(1) << 7)
#define MEDIA_AFBC_TILED                 (UINT64_C(1) << 8)
#define MEDIA_AFBC_SC                    (UINT64_C(1) << 9)
#define MEDIA_AFBC_DB                    (UINT64_C(1) << 10)
#define MEDIA_AFBC_BCH                   (UINT64_C(1) << 11)
#define MEDIA_AFBC_USM                   (UINT64_C(1) << 12)

typedef struct {
    uint32_t visible_width;
    uint32_t visible_height;
    uint32_t storage_width;
    uint32_t storage_height;
    uint32_t valid_x;
    uint32_t valid_y;
    uint32_t drm_fourcc;
    uint64_t drm_modifier;
    uint32_t plane_count;
    uint32_t pitch_bytes[4];
    uint32_t offset_bytes[4];
} MEDIA_VIDEO_LAYOUT;

/*
 * VPSS video-frame compatibility view.  This groups the existing Buffer and
 * VIDEO_FRAME metadata fields for convenient API use; it is not ABI- or
 * source-compatible with a third-party SDK.  Runtime frame facts remain in
 * the Buffer metadata after the API boundary is crossed.
 */
typedef struct {
    MEDIA_BUFFER buffer;
    int media_format;
    uint32_t len;
    uint64_t time_stamp;
    uint32_t seq;
    MEDIA_VIDEO_LAYOUT layout;
} MEDIA_VIDEO_FRAME;

typedef struct {
    int valid;
    int type;
    int media_format;
    uint32_t len;
    uint64_t time_stamp;
    uint32_t seq;

    union {
        MEDIA_VIDEO_LAYOUT video;

        struct {
            int sample_rate;
            int bit_width;
            int snd_mode;
            int point_num_per_frame;
            uint32_t plane_count;
            uint32_t plane_offsets[2];
            uint32_t plane_lengths[2];
        } audio_frame;

        struct {
            int codec;
            int transport_type;
            uint32_t plane_count;
            uint32_t plane_offsets[1];
            uint32_t plane_lengths[1];
        } audio_stream;
    } u;
} MEDIA_BUFFER_META;

typedef enum {
    MEDIA_POOL_ALLOC_MODE_NATIVE = 1,
    MEDIA_POOL_ALLOC_MODE_COMPAT = 2,
    MEDIA_POOL_ALLOC_MODE_HOST = 3,
} MEDIA_POOL_ALLOC_MODE;

typedef struct {
    void *data;
    size_t len;
    void *handle;
} MEDIA_PACKET;

typedef enum {
    MEDIA_FORMAT_UNSET = 0,  // 未设置格式
    MEDIA_FORMAT_NV12 = 1,
    /*
     * Packed RGB names below describe bytes in increasing memory-address
     * order. MEDIA_VIDEO_LAYOUT.drm_fourcc can therefore have the reverse
     * RGB/BGR spelling required by little-endian DRM scanout.
     */
    MEDIA_FORMAT_RGB888 = 2,
    MEDIA_FORMAT_RGBA8888 = 3,
    /* 数值 4 保留为空洞；AFBC 通过逻辑格式 + MEDIA_VIDEO_COMPRESS 表达。 */
    MEDIA_FORMAT_GRAY8 = 5,  // 8 位灰度格式
    MEDIA_FORMAT_BGR888 = 6,
    MEDIA_FORMAT_BGRA8888 = 7,
    MEDIA_FORMAT_YUV422SP = 8,
    MEDIA_FORMAT_YUV422P = 9,
    MEDIA_FORMAT_YUV420SP = 10,
    MEDIA_FORMAT_YUV420P = 11,
    MEDIA_FORMAT_YCrCb_420SP = 12,
    MEDIA_FORMAT_YCrCb_422SP = 13,
    MEDIA_FORMAT_RGB565 = 14,
    MEDIA_FORMAT_RGBA5551 = 15,
    MEDIA_FORMAT_RGBA4444 = 16,
    MEDIA_FORMAT_ARGB8888 = 17,
    MEDIA_FORMAT_ABGR8888 = 18,
    MEDIA_FORMAT_DEPTH_F32 = 19, // float32 depth map
    MEDIA_FORMAT_TENSOR_NHWC_F16 = 20, // packed NHWC RGB tensor, fp16 per channel
    MEDIA_FORMAT_NV24 = 21, // Y/CbCr 4:4:4 semi-planar, HDMI RX commonly reports this
    MEDIA_FORMAT_YUV420SP_10BIT = 22,
    MEDIA_FORMAT_YUV422SP_10BIT = 23,
    MEDIA_FORMAT_YUV422_YUYV = 24,
    MEDIA_FORMAT_YUV422_YVYU = 25,
    MEDIA_FORMAT_YUV422_UYVY = 26,
    MEDIA_FORMAT_YUV422_VYUY = 27,
    MEDIA_FORMAT_YUV440SP = 28,
    MEDIA_FORMAT_YUV411SP = 29,
    MEDIA_FORMAT_AUDIO_PCM_S16LE = 30, // planar PCM S16LE audio frame
    MEDIA_FORMAT_YUV444P = 31,
    MEDIA_FORMAT_YUV444SP_10BIT = 32,
    MEDIA_FORMAT_BGR565 = 33,
    MEDIA_FORMAT_RGB555 = 34,
    MEDIA_FORMAT_BGR555 = 35,
    MEDIA_FORMAT_RGB444 = 36,
    MEDIA_FORMAT_BGR444 = 37,
    MEDIA_FORMAT_RGB101010 = 38,
    MEDIA_FORMAT_BGR101010 = 39,        // DRM XB30；当前 VPSS 仅输出 opaque AFBC 0x041，LINEAR 不支持
    MEDIA_FORMAT_BAYER_RAW16_LE = 40, // unpacked little-endian uint16 Bayer sample
    MEDIA_FORMAT_RGB_U16 = 41,        // interleaved little-endian uint16 R/G/B
    MEDIA_FORMAT_P010 = 42,           // 10bit 4:2:0, MSB-aligned in uint16 containers
    MEDIA_FORMAT_P210 = 43,           // 10bit 4:2:2, MSB-aligned in uint16 containers
    MEDIA_FORMAT_Y210 = 44,           // packed 10bit 4:2:2, 8 bytes per two pixels
    MEDIA_FORMAT_Y410 = 45,           // packed 10bit 4:4:4 plus 2bit alpha
    MEDIA_FORMAT_Y0L2 = 46,           // packed 10bit 4:2:0, 2x2 tiles
    MEDIA_FORMAT_ABGR2101010 = 47,    // DRM AB30: 10bit RGB plus 2bit alpha
    // 编码格式（用于模块间传递编码包）
    MEDIA_FORMAT_H264 = 100,  // H.264 编码包
    MEDIA_FORMAT_H265 = 101,  // H.265 编码包
    MEDIA_FORMAT_MJPEG = 102, // MJPEG 编码包
    MEDIA_FORMAT_JPEG = 103,  // JPEG 编码包
    MEDIA_FORMAT_MPEG2 = 104,
    MEDIA_FORMAT_H263 = 105,
    MEDIA_FORMAT_MPEG4 = 106,
    MEDIA_FORMAT_VP8 = 107,
    MEDIA_FORMAT_VP9 = 108,
    MEDIA_FORMAT_AVSPLUS = 109,
    MEDIA_FORMAT_AVS = 110,
    MEDIA_FORMAT_AVS2 = 111,
    MEDIA_FORMAT_AV1 = 112,
    MEDIA_FORMAT_AUDIO_G711A = 120, // G.711 A-law audio stream
    MEDIA_FORMAT_AUDIO_G711U = 121, // G.711 mu-law audio stream
    MEDIA_FORMAT_AUDIO_AAC = 122,   // AAC-LC audio stream
} MEDIA_FORMAT;

typedef enum {
    MEDIA_BAYER_PATTERN_INVALID = 0,
    MEDIA_BAYER_PATTERN_BGGR = 1,
    MEDIA_BAYER_PATTERN_GBRG = 2,
    MEDIA_BAYER_PATTERN_GRBG = 3,
    MEDIA_BAYER_PATTERN_RGGB = 4,
} MEDIA_BAYER_PATTERN;

typedef enum {
    MEDIA_VIDEO_H264 = 0,
    MEDIA_VIDEO_H265 = 1,
    MEDIA_VIDEO_MJPEG = 2,
    MEDIA_VIDEO_JPEG = 3,
    MEDIA_VIDEO_MPEG2 = 4,
    MEDIA_VIDEO_H263 = 5,
    MEDIA_VIDEO_MPEG4 = 6,
    MEDIA_VIDEO_VP8 = 7,
    MEDIA_VIDEO_VP9 = 8,
    MEDIA_VIDEO_AVSPLUS = 9,
    MEDIA_VIDEO_AVS = 10,
    MEDIA_VIDEO_AVS2 = 11,
    MEDIA_VIDEO_AV1 = 12,
} MEDIA_VIDEO_TYPE;

typedef enum {
    MEDIA_AUDIO_CODEC_G711A = 0,
    MEDIA_AUDIO_CODEC_G711U = 1,
    MEDIA_AUDIO_CODEC_AAC = 2,
} MEDIA_AUDIO_CODEC;

typedef enum {
    MEDIA_AUDIO_SAMPLE_RATE_8000 = 8000,
    MEDIA_AUDIO_SAMPLE_RATE_16000 = 16000,
    MEDIA_AUDIO_SAMPLE_RATE_44100 = 44100,
    MEDIA_AUDIO_SAMPLE_RATE_48000 = 48000,
} MEDIA_AUDIO_SAMPLE_RATE;

typedef enum {
    MEDIA_AUDIO_BIT_WIDTH_16 = 16,
} MEDIA_AUDIO_BIT_WIDTH;

typedef enum {
    MEDIA_AUDIO_SOUND_MODE_MONO = 1,
    MEDIA_AUDIO_SOUND_MODE_STEREO = 2,
} MEDIA_AUDIO_SOUND_MODE;

typedef enum {
    MEDIA_AUDIO_AAC_TRANSPORT_ADTS = 0,
} MEDIA_AUDIO_AAC_TRANSPORT;

typedef struct {
    void *virt_addr[2];          // planar PCM, one plane per channel
    uint32_t len;                // bytes per channel plane
    uint64_t time_stamp;         // monotonic timestamp in us
    uint32_t seq;
    int sample_rate;             // MEDIA_AUDIO_SAMPLE_RATE_*
    int bit_width;               // MEDIA_AUDIO_BIT_WIDTH_*
    int snd_mode;                // MEDIA_AUDIO_SOUND_MODE_*
    int point_num_per_frame;     // samples per channel
    void *handle;                // reserved for module-owned payload
} MEDIA_AUDIO_FRAME;

typedef struct {
    void *stream;
    uint32_t len;
    uint64_t time_stamp;         // monotonic timestamp in us
    uint32_t seq;
    int codec;                   // MEDIA_AUDIO_CODEC_*
    void *handle;                // reserved for module-owned payload
} MEDIA_AUDIO_STREAM;

// 一个输出端口最多可 Bind 的下游数（一出多入，buffer 以引用计数共享）
#define MEDIA_PORT_MAX_FANOUT 8
#define MEDIA_SYS_MAX_STATS_MODULES 32
#define MEDIA_SYS_MAX_STATS_PORTS 32
#define MEDIA_SYS_MAX_STATS_POOLS 32  /* 与 Pool 上限一致 */
#define MEDIA_SYS_STATS_NAME_LEN 64
#define MEDIA_SYS_STATS_DETAIL_LEN 128

typedef enum {
    MEDIA_PORT_DIR_INPUT = 0,
    MEDIA_PORT_DIR_OUTPUT = 1,
} MEDIA_PORT_DIR;

typedef struct {
    int has_gpu;
    double gpu_kernel_ms;
    double gpu_queue_ms;
    char detail[MEDIA_SYS_STATS_DETAIL_LEN];
} MEDIA_MODULE_EXTRA_STATS;

typedef struct {
    char name[MEDIA_SYS_STATS_NAME_LEN];
    int direction;              /* MEDIA_PORT_DIR_* */
    int connected;
    int width;
    int height;
    int stride;
    int format;
    int declared_queue_depth;

    int queue_current;
    int queue_capacity;
    int queue_high_watermark;
    uint64_t send_count;
    uint64_t enqueue_count;
    uint64_t dequeue_count;
    uint64_t drop_count;
    uint64_t manual_overwrite_count;
    double queue_latency_last_ms;
    double queue_latency_avg_ms;
    double queue_latency_max_ms;

    int last_layout_valid;
    int last_layout_format;
    int last_layout_width;
    int last_layout_height;
    uint32_t last_layout_strides[4];
} MEDIA_PORT_STATS;

typedef struct {
    char name[MEDIA_SYS_STATS_NAME_LEN];
    int type;
    int state;
    uint64_t frame_count;        /* total frame count, not reset by MEDIA_SYS_ResetStats */
    uint64_t stats_frame_count;  /* frame count since last MEDIA_SYS_ResetStats */
    uint64_t process_count;
    uint64_t process_fail_count;
    double fps;                 /* average since last MEDIA_SYS_ResetStats */
    double process_last_ms;
    double process_avg_ms;
    double process_max_ms;

    int has_gpu;
    double gpu_kernel_ms;
    double gpu_queue_ms;
    char gpu_detail[MEDIA_SYS_STATS_DETAIL_LEN]; /* legacy name; may carry non-GPU hardware module detail when has_gpu=0 */

    int input_count;
    int output_count;
    MEDIA_PORT_STATS inputs[MEDIA_SYS_MAX_STATS_PORTS];
    MEDIA_PORT_STATS outputs[MEDIA_SYS_MAX_STATS_PORTS];
} MEDIA_MODULE_STATS;

typedef struct {
    int pool_id;
    size_t buffer_size;
    int total_count;
    int available_count;
    int used_count;
    int peak_used_count;
    uint64_t get_count;
    uint64_t put_count;
    uint64_t get_fail_count;
} MEDIA_POOL_STATS;

typedef struct {
    char name[MEDIA_SYS_STATS_NAME_LEN];
    int state;
    int module_count;
    int connection_count;
    int pool_count;
    MEDIA_MODULE_STATS modules[MEDIA_SYS_MAX_STATS_MODULES];
    MEDIA_POOL_STATS pools[MEDIA_SYS_MAX_STATS_POOLS];
} MEDIA_PIPELINE_STATS;

// 系统控制。MEDIA_SYS_Exit 是进程级 teardown；调用前应用必须停止并 join 其它并发 MEDIA_* API。
int MEDIA_SYS_Init(void);
int MEDIA_SYS_Exit(void);
const char *MEDIA_SYS_GetVersion(void);
int MEDIA_SYS_GetModuleFrameCount(const char *mod, int id, uint64_t *frame_count);
int MEDIA_SYS_GetPipelineStats(MEDIA_PIPELINE_STATS *stats);
int MEDIA_SYS_ResetStats(void);

// 绑定/解绑。模块统一使用 <TYPE>_<id> 实例名。
// src_port/dst_port 必须非空显式指定（如 "output0"/"input0"/"output1"），禁止 NULL/"" 隐式绑第一个端口；端口名一律从 0 编号。
// API 0.1.36 起，Bind 自动投递在下游队列满载时保留时间较新的 buffer；时间戳不可用时按 FIFO 淘汰队头。
// 该规则适用于所有 Port 类型；手动 MEDIA_SYS_SendFrame 的等待、超时和失败 ownership 语义不变。
int MEDIA_SYS_Bind(const char *src_mod, int src_id, const char *src_port,
                   const char *dst_mod, int dst_id, const char *dst_port);
int MEDIA_SYS_UnBind(const char *src_mod, int src_id, const char *src_port,
                     const char *dst_mod, int dst_id, const char *dst_port);

// 插件（动态库 .so）。插件开发接口见 media_plugin.h。
// MEDIA_SYS_Init 会自动加载环境变量 MEDIA_PLUGIN_PATH（冒号分隔的目录列表）下的全部 *.so；
// MEDIA_SYS_Exit 在销毁全部模块后卸载所有插件。
// 插件实例名为 "<type>_<id>"，创建后直接用 MEDIA_SYS_Bind(type, id, "outputN", ...) 连接。
#define MEDIA_PLUGIN_MAX_LOADED 32
typedef struct {
    char name[64];
    char version[32];
    char path[256];
    int class_count;
    int instance_count;
} MEDIA_PLUGIN_INFO;

int MEDIA_PLUGIN_Load(const char *path);          // 返回 plugin_id (>=0) 或负错误码
int MEDIA_PLUGIN_LoadDir(const char *dir);        // 返回成功加载的插件数量或负错误码
int MEDIA_PLUGIN_Unload(int plugin_id);           // 仍有实例时返回 MEDIA_ERR_BUSY
int MEDIA_PLUGIN_GetInfo(int plugin_id, MEDIA_PLUGIN_INFO *info);
int MEDIA_PLUGIN_FindType(const char *type);      // 返回提供该类型的 plugin_id 或 MEDIA_ERR_NOT_FOUND
int MEDIA_PLUGIN_CreateModule(const char *type, int id, const void *config, size_t config_size);
int MEDIA_PLUGIN_DestroyModule(const char *type, int id);
int MEDIA_PLUGIN_Start(const char *type, int id);
int MEDIA_PLUGIN_Stop(const char *type, int id);
int MEDIA_PLUGIN_SetParam(const char *type, int id, const char *key, const void *value, size_t size);
int MEDIA_PLUGIN_GetParam(const char *type, int id, const char *key, void *value, size_t size);
int MEDIA_PLUGIN_SendFrame(const char *type, int id, int input_index, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_PLUGIN_GetFrame(const char *type, int id, int output_index, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_PLUGIN_ReleaseFrame(const char *type, int id, int output_index, MEDIA_BUFFER buf);

// Pool
int MEDIA_POOL_CreateWithMode(int pool_id, size_t size, int count, MEDIA_POOL_ALLOC_MODE alloc_mode);
#ifndef ALLDEMO_MEDIA_POOL_ALLOC_MODE
#define ALLDEMO_MEDIA_POOL_ALLOC_MODE MEDIA_POOL_ALLOC_MODE_COMPAT
#endif
#ifndef MEDIA_POOL_DEFAULT_ALLOC_MODE
#define MEDIA_POOL_DEFAULT_ALLOC_MODE ALLDEMO_MEDIA_POOL_ALLOC_MODE
#endif
#ifndef MEDIA_POOL_CREATE_NO_DEFAULT_MACRO
#define MEDIA_POOL_Create(pool_id, size, count) \
    MEDIA_POOL_CreateWithMode((pool_id), (size), (count), MEDIA_POOL_DEFAULT_ALLOC_MODE)
#endif
int MEDIA_POOL_Destroy(int pool_id);
// Pool 信息：slot 数、单个 buffer 大小、代次（Pool 每次创建都不同，可用作缓存失效依据）。
int MEDIA_POOL_GetInfo(int pool_id, int *count, size_t *buffer_size, uint64_t *generation);
int MEDIA_POOL_Get(int pool_id, MEDIA_BUFFER *buf);
int MEDIA_POOL_Put(MEDIA_BUFFER buf);

// 用户手动申请/归还 Buffer
int MEDIA_POOL_GetBuffer(int pool_id, MEDIA_BUFFER *buf);
int MEDIA_POOL_PutBuffer(MEDIA_BUFFER buf);
int MEDIA_POOL_GetFd(MEDIA_BUFFER buf, int *dmabuf_fd, size_t *size);
size_t MEDIA_POOL_GetSize(MEDIA_BUFFER buf);
void *MEDIA_POOL_GetVaddr(MEDIA_BUFFER buf);
int MEDIA_POOL_BeginCpuAccess(MEDIA_BUFFER buf, uint64_t flags);
int MEDIA_POOL_EndCpuAccess(MEDIA_BUFFER buf, uint64_t flags);
// 引用计数：MEDIA_POOL_GetBuffer 后计数为 1；Ref 增加一个持有者，每个持有者各调用一次
// Unref（等同 MEDIA_POOL_PutBuffer），最后一次才真正归还 Pool。Bind 一出多入时框架自动加引用，
// 下游拿到的是同一块内存：多个持有者同时存在时只能读，不得改写像素或 metadata。
int MEDIA_BUFFER_Ref(MEDIA_BUFFER buf);
int MEDIA_BUFFER_Unref(MEDIA_BUFFER buf);
/* 1=HOST 内存，0=其它后端，负值=无效 Buffer；不申请 slot。 */
int MEDIA_BUFFER_IsHostMemory(MEDIA_BUFFER buf);
int MEDIA_BUFFER_GetRefCount(MEDIA_BUFFER buf);  // FREE 为 0
int MEDIA_BUFFER_SetMeta(MEDIA_BUFFER buf, const MEDIA_BUFFER_META *meta);
int MEDIA_BUFFER_GetMeta(MEDIA_BUFFER buf, MEDIA_BUFFER_META *meta);

/*
 * Pool slot ownership（0.7.0，通用服务）：供把应用 Pool 的 slot 直接导入硬件引擎的 producer
 * 插件使用。注册归还回调后，该 Pool 的 MEDIA_POOL_Get/GetBuffer 会失败（slot 只由持有者经
 * MEDIA_POOL_ClaimSlot 按 index 取用）；应用 ReleaseFrame/PutBuffer 使 slot 归还时回调被调用，
 * 回调返回 0 后 slot 才回到 FREE，返回非 0 保持 RETURNING 以便同一调用重试。
 */
typedef int (*MEDIA_POOL_RETURN_CB)(int pool_id, int index, void *user);
int MEDIA_POOL_SetReturnCallback(int pool_id, MEDIA_POOL_RETURN_CB cb, void *user);
/* 注销回调；仍有回调在执行或仍有 slot 未归还时返回 MEDIA_ERR_BUSY。 */
int MEDIA_POOL_ClearReturnCallback(int pool_id);
/* 把指定 FREE slot 置为 IN_USE（引用计数 1）；timeout_ms=-1 一直等，0 不等待。 */
int MEDIA_POOL_ClaimSlot(int pool_id, int index, int timeout_ms);
/* 回滚一次尚未交付的 MEDIA_POOL_ClaimSlot（slot 必须 IN_USE 且引用计数为 1）。 */
int MEDIA_POOL_UnclaimSlot(int pool_id, int index);
int MEDIA_VIDEO_QueryLayout(uint32_t width, uint32_t height, int media_format,
                            MEDIA_VIDEO_COMPRESS compress,
                            MEDIA_VIDEO_LAYOUT *layout, size_t *buffer_size);
// 只回答“分配器能否描述并分配这个 tuple”，与 GPU 能否导入无关（GPU 能力用
// MEDIA_GPU_FormatSupported 单独判断）。LINEAR 由格式事实源逐格式声明：例如 HDMI-RX 的 NV24
// 可分配（CPU/DRM 扫描），但 Mali 无法导入，所以 GPU 插件仍拿不到它。
// 逻辑格式 + 精确 compress 对应的公开 DRM FourCC（与 libmedia 发布 VIDEO_FRAME metadata 同一事实源）。
// 未知组合返回 MEDIA_ERR_UNSUPPORTED。外部模块填写 MEDIA_VIDEO_LAYOUT.drm_fourcc 时必须使用它。
int MEDIA_VIDEO_GetDrmFourcc(int media_format, MEDIA_VIDEO_COMPRESS compress, uint32_t *drm_fourcc);
// 按格式事实源校验一份真实布局（plane/pitch/offset/modifier 与 payload_len 是否自洽）。
// 成功返回 MEDIA_STATUS_OK，并从 modifier 反解出 compress（可为 NULL）。外部模块发布逐帧
// metadata 前应调用，避免下游在 Bind 时拒收。
int MEDIA_VIDEO_ValidateLayout(const MEDIA_VIDEO_LAYOUT *layout, int media_format,
                               size_t payload_len, MEDIA_VIDEO_COMPRESS *compress);
// canonical LINEAR 布局（逐 plane 行字节 64 对齐），不需要 MEDIA_SYS_Init。
int MEDIA_VIDEO_BuildLinearLayout(uint32_t width, uint32_t height, int media_format,
                                  MEDIA_VIDEO_LAYOUT *layout, size_t *buffer_size);
// compress 语法（block/flags 组合）是否合法；不代表任何模块支持它。
int MEDIA_VIDEO_CompressSyntaxValid(MEDIA_VIDEO_COMPRESS compress);
// 由 DRM modifier 反解完整 compress（LINEAR 为 0）。
int MEDIA_VIDEO_CompressFromModifier(uint64_t drm_modifier, MEDIA_VIDEO_COMPRESS *compress);
// GPU 导入/渲染该 tuple 时使用的 storage FourCC。通常等于公开 FourCC；不同时是格式事实源中
// 经过验证的存储视图（例如 public XB30 + AFBC 以 AB30 视图导入），不得写回逐帧 metadata。
int MEDIA_VIDEO_GetStorageFourcc(int media_format, MEDIA_VIDEO_COMPRESS compress,
                                 uint32_t *drm_fourcc);
//int MEDIA_POOL_ImportFd(int pool_id, int index, int dmabuf_fd, size_t size);

// 手动发送
int MEDIA_SYS_SendFrame(const char *dst_mod, int dst_id, const char *dst_port,
                        MEDIA_BUFFER buf, int timeout_ms);
// 从任意模块（内置或插件）的输出端口手动取帧，与 SendFrame 对称；ReleaseFrame 必须配对。
// 有人等待 GetFrame 时该端口的帧优先交给手动取帧（与各模块 XXX_GetFrame 语义一致）。
// 超时返回 MEDIA_ERR_TIMEOUT，模块未运行返回 MEDIA_ERR_PIPELINE_STATE。
int MEDIA_SYS_GetFrame(const char *src_mod, int src_id, const char *src_port,
                       MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_SYS_ReleaseFrame(const char *src_mod, int src_id, const char *src_port,
                           MEDIA_BUFFER buf);

// All Debug API
// debug_buf由调用者提供；返回0表示完整写入，1表示被截断，-1表示参数或状态错误。
int MEDIA_SYS_GetDebugString(char *debug_buf, size_t debug_buf_size);
int MEDIA_SYS_debug(void);

#ifdef __cplusplus
}
#endif

#endif // MEDIA_API_H
