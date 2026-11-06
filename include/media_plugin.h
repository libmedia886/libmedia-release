#ifndef MEDIA_PLUGIN_H
#define MEDIA_PLUGIN_H

/*
 * libmedia 插件 SDK（插件作者使用）。
 *
 * 插件是一个独立的动态库（libxxx.so），通过导出 media_plugin_entry() 向
 * libmedia 描述自己提供的一个或多个模块类型（MEDIA_PLUGIN_CLASS）。应用侧使用
 * media_api.h 中的 MEDIA_PLUGIN_Load / MEDIA_PLUGIN_CreateModule 加载并创建实例，
 * 实例创建后与内置模块完全一样参与 MEDIA_SYS_Bind、GetFrame/SendFrame 和统计。
 *
 * 插件只依赖本文件与 media_api.h 中的稳定 C 接口：
 *   - 线程、队列、端口连接、Bind 投递和输入 Buffer 归还由框架负责；
 *   - 插件通过 MEDIA_POOL_* / MEDIA_BUFFER_* 访问帧数据与 metadata；
 *   - CPU 读写 DMA-BUF 前后必须成对调用 MEDIA_POOL_BeginCpuAccess/EndCpuAccess。
 *
 * ABI：所有结构体只在末尾追加字段；不兼容变更会提升 MEDIA_PLUGIN_ABI_VERSION，
 * 框架加载时拒绝 abi_version 不匹配的插件。
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_PLUGIN_ABI_VERSION 2u  /* 2: 0.9.0 删除 MEDIA_PLUGIN_Register，改为 MEDIA_PLUGIN_LoadSelf */

/* 每个插件实例最多的输入/输出端口数（端口名固定为 inputN / outputN）。 */
#define MEDIA_PLUGIN_MAX_PORTS 16

/* 每个实例最多同时持有（RecvFrame / RetainFrame 后未归还）的 buffer 数。 */
#define MEDIA_PLUGIN_MAX_HELD 64

/* 插件模块类型名最大长度（含结尾 0），实例名为 "<type>_<id>"。 */
#define MEDIA_PLUGIN_TYPE_NAME_MAX 32

#define MEDIA_PLUGIN_ENTRY_SYMBOL "media_plugin_entry"

#if defined(__GNUC__)
#define MEDIA_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define MEDIA_PLUGIN_EXPORT
#endif

/* 每个插件首次进入 media_plugin_entry 时输出授权提示；不改变插件 ABI。 */
#define MEDIA_PLUGIN_LICENSE_NOTICE(plugin_name)                                      \
    do {                                                                               \
        static int media_plugin_license_notice_printed;                               \
        if (!media_plugin_license_notice_printed) {                                   \
            media_plugin_license_notice_printed = 1;                                 \
            fprintf(stderr,                                                         \
                    "[libmedia license] %s：本软件为闭源软件，个人非商业使用免费；" \
                    "商业使用请联系 allinjetson@gmail.com。\n"                  \
                    "[libmedia license] %s: This is closed-source software. Free for " \
                    "personal, non-commercial use; for commercial use, please contact " \
                    "allinjetson@gmail.com.\n",                                      \
                    (plugin_name), (plugin_name));                                   \
        }                                                                              \
    } while (0)

/* 插件实例的框架侧句柄，对插件不透明。 */
typedef struct MEDIA_PLUGIN_CTX MEDIA_PLUGIN_CTX;

/*
 * 两种运行模式，任何 kind 都可以选：
 *   托管模式（提供 process）：框架工作线程依次从 input0..N 各取一帧调用 process，
 *     发送 outputs[] 并归还输入。SOURCE 无输入时循环调用 process(NULL, 0, ...)。
 *   自管模式（process 为 NULL）：框架不起线程；插件在 start 中创建自有线程，用
 *     MEDIA_PLUGIN_RecvFrame / ReturnFrame 收输入、MEDIA_PLUGIN_EmitFrame 发输出，
 *     在 stop 中 join（stop 被调用前框架已关闭输入队列，阻塞的 RecvFrame 会立即返回）。
 */
typedef enum {
    MEDIA_PLUGIN_KIND_FILTER = 0, /* 有输入，可有输出 */
    MEDIA_PLUGIN_KIND_SOURCE = 1, /* 无输入，只有输出 */
    MEDIA_PLUGIN_KIND_SINK = 2,   /* 只有输入 */
    /*
     * 0.7.0：端口形态由 create 实际声明的端口决定，不要求固定 kind。
     * 适用于输入端口可选（例如同一 type 既有 bind 模式又有手动送包模式）的插件；
     * 框架按实际端口推导模块类型：无输入有输出 = SOURCE，有输入无输出 = SINK，
     * 两者都有 = FILTER/PROCESS。create 必须至少声明一个端口。
     */
    MEDIA_PLUGIN_KIND_AUTO = 3,
} MEDIA_PLUGIN_KIND;

typedef struct {
    int format;                    /* MEDIA_FORMAT_* */
    MEDIA_VIDEO_COMPRESS compress; /* 0 = LINEAR；非零时 stride 必须为 0 */
    int width;                     /* 输出端口必须 > 0；输入端口与 height 同为 0 表示接受任意尺寸 */
    int height;
    int stride;                    /* plane0 byte pitch，0 = 由格式推导 */
    int fps;                       /* 0 = 不限 */
    int queue_depth;               /* 仅输入端口：Bind 队列深度，0 取默认 4 */
} MEDIA_PLUGIN_PORT_ATTR;

typedef struct {
    /* 模块类型名，只允许 [A-Z0-9_]，用于 MEDIA_SYS_Bind 的 mod 参数，如 "MY_FILTER"。 */
    const char *type;
    const char *description;
    MEDIA_PLUGIN_KIND kind;

    /*
     * 创建实例：解析 config、调用 MEDIA_PLUGIN_AddInputPort/AddOutputPort 声明端口、
     * 用 MEDIA_PLUGIN_SetUserData 保存私有状态。返回 0 成功；失败时框架会调用
     * destroy（若已设置 user data）并释放实例。
     */
    int (*create)(MEDIA_PLUGIN_CTX *ctx, const void *config, size_t config_size);
    /* 销毁实例，释放 user data。框架保证调用前已 stop 且工作线程已退出。 */
    void (*destroy)(MEDIA_PLUGIN_CTX *ctx);

    /* 可选（自管模式必需 start）。托管模式下在框架工作线程启动前 / 退出后调用。 */
    int (*start)(MEDIA_PLUGIN_CTX *ctx);
    int (*stop)(MEDIA_PLUGIN_CTX *ctx);

    /*
     * 处理一帧。inputs[i] 对应 inputN，调用结束后由框架归还，插件不得保存或 Put。
     * outputs[j] 进入时为 {-1,-1}；插件通过 MEDIA_POOL_GetBuffer 申请输出 Buffer、
     * 写入数据与 metadata（MEDIA_BUFFER_SetMeta）后填入 outputs[j]。
     * 返回 0 时框架发送全部有效输出；返回非 0 时框架把已填入的输出归还 Pool。
     */
    int (*process)(MEDIA_PLUGIN_CTX *ctx,
                   const MEDIA_BUFFER *inputs, int input_count,
                   MEDIA_BUFFER *outputs, int output_count);

    /* 可选。运行期参数，由 MEDIA_PLUGIN_SetParam/GetParam 透传。 */
    int (*set_param)(MEDIA_PLUGIN_CTX *ctx, const char *key,
                     const void *value, size_t size);
    int (*get_param)(MEDIA_PLUGIN_CTX *ctx, const char *key,
                     void *value, size_t size);
} MEDIA_PLUGIN_CLASS;

typedef struct {
    uint32_t abi_version;          /* 必须填 MEDIA_PLUGIN_ABI_VERSION */
    const char *name;              /* 插件名 */
    const char *version;           /* 插件自身版本 */
    int class_count;
    const MEDIA_PLUGIN_CLASS *classes;
} MEDIA_PLUGIN_DESC;

/*
 * 插件入口。host_abi_version 为框架的 MEDIA_PLUGIN_ABI_VERSION；插件不支持时可
 * 返回 NULL 拒绝加载。返回的描述及其 classes 必须在插件卸载前保持有效。
 */
typedef const MEDIA_PLUGIN_DESC *(*MEDIA_PLUGIN_ENTRY_FN)(uint32_t host_abi_version);
MEDIA_PLUGIN_EXPORT const MEDIA_PLUGIN_DESC *media_plugin_entry(uint32_t host_abi_version);

/* ===== 框架服务（由 libmedia 导出，插件直接调用） ===== */

/*
 * 直接链接进应用的模块库（例如 libmedia_tpg.so 自带 MEDIA_TPG_* API）在自身 API 的 init 路径
 * 调用它，把 desc 所在的已签名映像加载进 libmedia（0.9.0 起取代已删除的 MEDIA_PLUGIN_Register）。
 *
 * desc 必须是插件映像内的静态描述（不能在堆或栈上构造）。libmedia 从 /proc/self/maps 找到
 * desc 所在的文件，按与 MEDIA_PLUGIN_Load 相同的规则校验 <file>.sig，并要求 desc、classes 及
 * 全部回调指针都位于这个已验签文件里；任一项不满足返回 MEDIA_ERR_PLUGIN_SIGNATURE。
 *
 * 重复调用同一 desc 返回同一 plugin_id（已加载时不再读文件）；同一库被 MEDIA_PLUGIN_Load /
 * MEDIA_PLUGIN_PATH 加载过时也返回该 id。返回 plugin_id 或负错误码。
 * MEDIA_SYS_Exit 会注销全部插件，模块库应在下次调用时再次调用本函数。
 */
int MEDIA_PLUGIN_LoadSelf(const MEDIA_PLUGIN_DESC *desc);

/* 声明端口，返回端口序号（inputN/outputN 的 N），失败返回负错误码。只能在 create 中调用。 */
int MEDIA_PLUGIN_AddInputPort(MEDIA_PLUGIN_CTX *ctx, const MEDIA_PLUGIN_PORT_ATTR *attr);
int MEDIA_PLUGIN_AddOutputPort(MEDIA_PLUGIN_CTX *ctx, const MEDIA_PLUGIN_PORT_ATTR *attr);

/*
 * 设置端口的不透明能力描述（size <= 256，内容由插件自己定义，core 不解释）。
 * Bind 时仅当两端都设置了描述，才要求长度与内容完全一致，否则返回 MEDIA_ERR_PORT_INCOMPATIBLE。
 * 仅允许在 create 中，或实例已 Stop 且该端口未 Bind 时调用；其它状态返回 MEDIA_ERR_PIPELINE_STATE。
 * size 为 0 表示清除描述。
 */
int MEDIA_PLUGIN_SetPortCaps(MEDIA_PLUGIN_CTX *ctx, MEDIA_PORT_DIR direction, int index,
                             const void *blob, size_t size);
/* 端口当前是否已 Bind：1 已连接，0 未连接，负值为错误（索引无效）。线程安全。 */
int MEDIA_PLUGIN_IsPortConnected(MEDIA_PLUGIN_CTX *ctx, MEDIA_PORT_DIR direction, int index);

void MEDIA_PLUGIN_SetUserData(MEDIA_PLUGIN_CTX *ctx, void *user_data);
void *MEDIA_PLUGIN_GetUserData(MEDIA_PLUGIN_CTX *ctx);

/* 实例名（如 "MY_FILTER_0"）与实例 id。 */
const char *MEDIA_PLUGIN_GetInstanceName(MEDIA_PLUGIN_CTX *ctx);
int MEDIA_PLUGIN_GetInstanceId(MEDIA_PLUGIN_CTX *ctx);

/* 实例是否处于运行状态，供插件自有线程判断退出。 */
int MEDIA_PLUGIN_IsRunning(MEDIA_PLUGIN_CTX *ctx);

/*
 * 主动输出一帧到 outputN（任何模式、任何线程）。成功后该引用转移给框架（多个下游时
 * 框架自动加引用分发）；失败时仍归插件，需自行 MEDIA_POOL_PutBuffer。
 */
int MEDIA_PLUGIN_EmitFrame(MEDIA_PLUGIN_CTX *ctx, int output_index, MEDIA_BUFFER buf);

/*
 * 自管模式：从自己的 inputN 取一帧。timeout_ms：-1 一直等，0 不等。
 * 成功后插件持有该 buffer，用完必须 MEDIA_PLUGIN_ReturnFrame。
 * 超时返回 MEDIA_ERR_TIMEOUT；实例停止时返回 MEDIA_ERR_PIPELINE_STATE（线程据此退出）。
 */
int MEDIA_PLUGIN_RecvFrame(MEDIA_PLUGIN_CTX *ctx, int input_index, MEDIA_BUFFER *buf,
                           int timeout_ms);

/*
 * 扣留一个 buffer（增加一个引用），用于时域算法保留历史帧：例如在 process 中扣留当前
 * 输入作为下一帧的参考帧。扣留的帧只能读，用完必须 MEDIA_PLUGIN_ReturnFrame。
 */
int MEDIA_PLUGIN_RetainFrame(MEDIA_PLUGIN_CTX *ctx, MEDIA_BUFFER buf);

/*
 * 归还 RecvFrame / RetainFrame 得到的 buffer。框架按实例记账：归还未持有的 buffer 返回
 * MEDIA_ERR_NOT_FOUND；实例销毁时仍未归还的 buffer 会被自动回收并告警
 * （MEDIA_SYS_debug 中显示 held/leaked）。同一实例最多同时持有 MEDIA_PLUGIN_MAX_HELD 个。
 */
int MEDIA_PLUGIN_ReturnFrame(MEDIA_PLUGIN_CTX *ctx, MEDIA_BUFFER buf);

/*
 * 自管模式记录一次处理（耗时、是否成功），进入 MEDIA_SYS_debug 的 process_ms / frames / fail。
 * 一旦调用过，EmitFrame 不再单独计帧。托管模式由框架自动统计，不需要调用。
 */
int MEDIA_PLUGIN_RecordProcess(MEDIA_PLUGIN_CTX *ctx, uint64_t elapsed_ns, int ok);

/* 上报模块额外统计（GPU 耗时、自定义 detail），MEDIA_SYS_debug / GetPipelineStats 中显示。 */
int MEDIA_PLUGIN_ReportStats(MEDIA_PLUGIN_CTX *ctx, const MEDIA_MODULE_EXTRA_STATS *stats);

/*
 * 修改已声明端口的属性（例如 Stop 后改输出尺寸/格式）。实例运行中只允许改 stride/fps；
 * 端口已 Bind 时 format/width/height/compress 必须保持不变，否则返回 MEDIA_ERR_PIPELINE_STATE。
 */
int MEDIA_PLUGIN_SetPortAttr(MEDIA_PLUGIN_CTX *ctx, MEDIA_PORT_DIR direction, int index,
                             const MEDIA_PLUGIN_PORT_ATTR *attr);

/* ===== GPU（OpenGL ES）上下文：给需要 GPU 的插件使用 ===== */

/*
 * 每个 MEDIA_GPU_CTX 是一个独立的 GLES 3 无 surface 上下文，共享 libmedia 进程级 GBM
 * EGLDisplay（与内置 GLES 模块同一个）。典型用法：在 start（或自有线程）中 CreateContext +
 * MakeCurrent，之后同一线程内 ImportBuffer / 绘制 / Finish；stop 中 DestroyContext。
 * 上下文同一时刻只能在一个线程 current。shader/program 由插件自己创建。
 */
typedef struct MEDIA_GPU_CTX MEDIA_GPU_CTX;

typedef struct {
    void *egl_image;       /* EGLImageKHR */
    unsigned int texture;  /* GL texture，已绑定该 EGLImage */
    unsigned int target;   /* GL_TEXTURE_EXTERNAL_OES（采样/YUV 渲染）或 GL_TEXTURE_2D（RGB 渲染） */
    unsigned int fbo;      /* render_target=1 时的 framebuffer，否则 0 */
    uint32_t width;        /* visible 尺寸 */
    uint32_t height;
} MEDIA_GPU_IMAGE;

int MEDIA_GPU_CreateContext(MEDIA_GPU_CTX **ctx);
void MEDIA_GPU_DestroyContext(MEDIA_GPU_CTX *ctx);   /* 释放缓存的 EGLImage/纹理/FBO */
int MEDIA_GPU_MakeCurrent(MEDIA_GPU_CTX *ctx);       /* 绑定到调用线程 */
int MEDIA_GPU_ReleaseCurrent(MEDIA_GPU_CTX *ctx);
void *MEDIA_GPU_GetEGLDisplay(MEDIA_GPU_CTX *ctx);   /* EGLDisplay */
void *MEDIA_GPU_GetEGLContext(MEDIA_GPU_CTX *ctx);   /* EGLContext */

/*
 * 把 buffer 导入为 EGLImage + 纹理（render_target=1 时另建 FBO 作为渲染目标）。
 * 依据 buffer 的 VIDEO_FRAME metadata（真实 layout / modifier，支持 AFBC），因此输出 buffer
 * 要先 MEDIA_BUFFER_SetMeta 再导入。结果按 (pool, index, pool 代次, layout) 缓存，稳态零导入；
 * 返回的对象归上下文所有，不要自行删除。需要当前线程已 MakeCurrent。
 */
int MEDIA_GPU_ImportBuffer(MEDIA_GPU_CTX *ctx, MEDIA_BUFFER buf, int render_target,
                           MEDIA_GPU_IMAGE *image);
void MEDIA_GPU_PurgeCache(MEDIA_GPU_CTX *ctx);       /* Pool 重建后可主动清缓存 */
uint64_t MEDIA_GPU_GetImportCount(MEDIA_GPU_CTX *ctx); /* 累计真实导入次数（缓存命中不计） */

/*
 * 等待本上下文已提交的 GPU 工作完成（EGL fence）。process 返回、ReturnFrame 或 EmitFrame
 * 之前必须调用，否则下游或 Pool 可能在 GPU 仍读写时复用该 buffer。
 */
int MEDIA_GPU_Finish(MEDIA_GPU_CTX *ctx);

/* GLES DMA-BUF 能力事实源（与内置 GLES 模块同一张表）：逻辑格式 / 精确 compress 能否导入或渲染。 */
int MEDIA_GPU_FormatSupported(int media_format, MEDIA_PORT_DIR direction);
int MEDIA_GPU_CompressSupported(int media_format, MEDIA_VIDEO_COMPRESS compress,
                                MEDIA_PORT_DIR direction);

/*
 * 加载构建期生成的 program binary（SDK 工具 media_gles_compiler 按插件自己的 spec 与 shader
 * 生成 embedded_*_program / _size / _format 数组和 embedded_gles_build_* 指纹字符串）。
 * 运行时 GL 指纹与 build_env 不一致时返回 0（驱动升级后需重新构建插件）。需要当前线程已
 * MakeCurrent。返回 GL program id。
 */
typedef struct {
    const char *vendor;
    const char *renderer;
    const char *version;
    const char *glsl_version;
} MEDIA_GPU_PROGRAM_ENV;

unsigned int MEDIA_GPU_LoadProgramBinary(const void *binary, size_t binary_size,
                                         uint32_t binary_format,
                                         const MEDIA_GPU_PROGRAM_ENV *build_env,
                                         const char *label);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_PLUGIN_H */
