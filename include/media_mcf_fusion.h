/*
 * MCF_FUSION_CL 外部模块公开头文件。
 *
 * 这里保留 master:include/media_api.h 中 MCF 与 MCF_FUSION_CL 的全部类型、
 * 结构体字段顺序和函数原型，供应用源码直接迁移；插件本体只依赖公开 SDK。
 * 旧声明保持原样；插件新增的配置类型只在文件末尾追加，新增宏和辅助
 * 函数可以放在文件开头，便于应用包含头文件后直接使用。
 */
#ifndef MEDIA_MCF_FUSION_H
#define MEDIA_MCF_FUSION_H

#include <string.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 插件 type 名（MEDIA_PLUGIN_CreateModule / MEDIA_SYS_Bind 使用）。 */
#define MEDIA_MCF_FUSION_TYPE "MCF_FUSION_CL"

#define MEDIA_MCF_MAX_PIPE_NUM 2
#define MEDIA_MCF_MAX_CHN_NUM 1

/* 队列深度缺省值与上下限（旧模块语义）。 */
#define MEDIA_MCF_DEFAULT_DEPTH 4
#define MEDIA_MCF_MAX_DEPTH 64
/* 时间戳配对容差缺省值与显式范围（微秒）。 */
#define MEDIA_MCF_DEFAULT_TOLERANCE_US 2000
#define MEDIA_MCF_MIN_TOLERANCE_US 100
#define MEDIA_MCF_MAX_TOLERANCE_US 16667

typedef enum {
    MEDIA_MCF_PIPE_MONO = 0,
    MEDIA_MCF_PIPE_COLOR = 1,
} MEDIA_MCF_PIPE;

typedef enum {
    MEDIA_MCF_PATH_FUSION = 0,
    MEDIA_MCF_PATH_COLOR = 1,
    MEDIA_MCF_PATH_MONO = 2,
} MEDIA_MCF_PATH;

typedef enum {
    MEDIA_MCF_NORMALIZE_NONE = 0,
    MEDIA_MCF_NORMALIZE_MEAN_STD = 1,
} MEDIA_MCF_NORMALIZE_MODE;

typedef enum {
    MEDIA_MCF_ALG_LOCAL_REALTIME = 0,
    MEDIA_MCF_ALG_MULTIBAND = 1,
    MEDIA_MCF_ALG_LEGACY_SINGLE_SCALE = 2,
} MEDIA_MCF_ALG_MODE;

/* 单路输入属性。 */
typedef struct {
    int pipe_id;    /* MEDIA_MCF_PIPE_MONO / MEDIA_MCF_PIPE_COLOR，必须与端口对应 */
    int width;      /* 有效宽度（像素），必须为偶数 */
    int height;     /* 有效高度（像素），必须为偶数 */
    int format;     /* color 只能是 NV12；mono 支持 GRAY8 或 NV12（只读 Y 平面） */
    int stride;     /* 行字节数，必须 ≥ width 且 64 字节对齐 */
    int depth;      /* 输入队列深度；0 表示缺省 4，显式范围 1..64 */
} MEDIA_MCF_PIPE_ATTR;

/* 分组属性。 */
typedef struct {
    int sync_pipe_en;                    /* 0 = 先到先配；1 = 按时间戳容差配对 */
    MEDIA_MCF_PIPE_ATTR mono_pipe_attr;   /* 单色路（input1）属性 */
    MEDIA_MCF_PIPE_ATTR color_pipe_attr;  /* 彩色路（input0）属性 */
    int depth;                            /* 输入队列深度；0 表示缺省 4 */
    int path;                             /* MEDIA_MCF_PATH，选择直通或融合 */
    int timestamp_tolerance_us;           /* 配对容差；0 表示 2000us，显式范围 100..16667 */
    int reuse_last_frame;                 /* 1 = 缺一路时复用该路上一帧，0 = 丢弃等下一对 */
} MEDIA_MCF_GRP_ATTR;

/* 输出通道属性（第一版只支持 chn=0）。 */
typedef struct {
    int width;            /* 输出宽度，必须等于 color pipe 宽度 */
    int height;           /* 输出高度，必须等于 color pipe 高度 */
    int format;           /* 第一版只支持 MEDIA_FORMAT_NV12 */
    int stride;           /* 输出行字节数，64 字节对齐且 ≥ width */
    int depth;            /* 输出队列深度；0 表示缺省 4 */
    int output_pool_id;   /* 输出 Pool；Start 前必须已创建足量同尺寸 buffer */
} MEDIA_MCF_CHN_ATTR;

/* 单个频段的处理配置。 */
typedef struct {
    float filter_sigma;   /* 该频段高斯滤波 sigma（第一版只接受固定值） */
    float detail_gain;    /* 该频段细节注入增益 */
    float base_alpha;     /* 该频段低频混合权重 */
} MEDIA_MCF_EACH_FREQ_PROC_CFG;

/* 算法参数。 */
typedef struct {
    int mode;                        /* MEDIA_MCF_ALG_MODE，选择 LOCAL_REALTIME / MULTIBAND / LEGACY */
    int normalize_mode;              /* MEDIA_MCF_NORMALIZE_MODE，mono 是否做均值方差归一化 */
    int local_window;                /* 局部窗口大小；第一版必须是 5 */
    float local_map_sigma;           /* 局部融合权重图的高斯 sigma；固定 0.8 */
    float local_detail_sigma;        /* 局部细节底图的高斯 sigma；固定 3.0 */
    float local_detail_gain;         /* LOCAL_REALTIME 细节注入增益，缺省 0.90 */
    MEDIA_MCF_EACH_FREQ_PROC_CFG hf_cfg; /* 高频段配置；filter_sigma 固定 1.0 */
    MEDIA_MCF_EACH_FREQ_PROC_CFG mf_cfg; /* 中频段配置；filter_sigma 固定 3.0 */
    MEDIA_MCF_EACH_FREQ_PROC_CFG lf_cfg; /* 低频段配置；filter_sigma 固定 3.0 */
    float flatness_threshold;        /* 平坦区域判定阈值，缺省 0.0007 */
    float gain_min;                  /* 归一化增益下限，缺省 0.5 */
    float gain_max;                  /* 归一化增益上限，缺省 2.0 */
    float epsilon;                   /* 数值保护项，缺省 1e-6 */
    float registration_confidence;   /* 配准置信度 0..1，运行中允许更新 */
    float fallback_threshold;        /* 置信度不高于该值时退化为 COLOR 直通 */
} MEDIA_MCF_ALG_PARAM;

/* 运行统计（诊断用）。 */
typedef struct {
    double stats_kernel_ms;          /* 归一化统计 pass 耗时，未执行时为 -1 */
    double filter_kernel_ms;         /* 滤波类 pass 累计耗时，未执行为 -1 */
    double fusion_kernel_ms;         /* 最后一个融合 pass 耗时，未执行为 -1 */
    double gpu_total_ms;             /* 本帧 GPU 总耗时（含 Finish 等待），未执行为 -1 */
    double process_ms;               /* 本帧 CPU 侧处理总耗时 */
    double pass_kernel_ms[6];        /* 前 6 个算法 pass 的耗时，未使用为 -1 */
    int pass_count;                  /* 本帧执行的算法 pass 数（不含 Y/UV 拷贝） */
    uint64_t processed_frames;       /* 成功输出帧数 */
    uint64_t matched_pairs;          /* 两路都是新帧的配对数 */
    uint64_t color_drop_count;       /* 时间戳不匹配丢弃的彩色帧数 */
    uint64_t mono_drop_count;        /* 时间戳不匹配丢弃的单色帧数 */
    uint64_t color_reuse_count;      /* 复用彩色历史帧的次数 */
    uint64_t mono_reuse_count;       /* 复用单色历史帧的次数 */
    uint64_t missing_timestamp_count;/* 开启配对但某路时间戳为 0 的次数 */
    uint64_t low_confidence_fallback_count; /* 置信度过低退化为 COLOR 的次数 */
    uint64_t import_cache_hits;      /* EGLImage 导入缓存命中次数 */
    uint64_t import_cache_misses;    /* 实际执行的 EGLImage 导入次数 */
    int import_cache_entries;        /* 当前缓存的有效导入项数量 */
    int cpu_fallback_used;           /* 是否使用过 CPU 回退（本插件固定 0） */
    uint64_t full_frame_copy_count;  /* 为复用历史帧而整帧拷贝的次数（本插件固定 0） */
    uint64_t cpu_map_count;          /* CPU 映射次数（本插件固定 0） */
    uint64_t gpu_readback_count;     /* GPU 回读次数（本插件固定 0） */
    uint64_t strong_sync_count;      /* 异常强同步次数（本插件固定 0） */
    uint64_t output_completion_wait_count; /* 等待输出完成（Finish）的次数 */
} MEDIA_MCF_PERF;

/* 裁剪区域。 */
typedef struct {
    int enable;   /* 0 = 关闭裁剪，1 = 启用 */
    int x;        /* 裁剪起点 X */
    int y;        /* 裁剪起点 Y */
    int width;    /* 裁剪宽度 */
    int height;   /* 裁剪高度 */
} MEDIA_MCF_CROP_INFO;

/* 扩展通道属性。 */
typedef struct {
    int bind_chn;                  /* 绑定的主通道号 */
    MEDIA_MCF_CHN_ATTR chn_attr;   /* 扩展通道输出属性 */
} MEDIA_MCF_EXT_CHN_ATTR;

/* VI 绑定属性。 */
typedef struct {
    int enable;      /* 0 = 关闭 VI 直连，1 = 启用 */
    int mono_pipe;   /* 单色路 pipe 号 */
    int color_pipe;  /* 彩色路 pipe 号 */
} MEDIA_MCF_VI_ATTR;

/* 通用矩形（区域亮度统计用）。 */
typedef struct {
    int x;        /* 起点 X */
    int y;        /* 起点 Y */
    int width;    /* 宽度 */
    int height;   /* 高度 */
} MEDIA_MCF_RECT;

int MEDIA_MCF_CreateGrp(int grp, const MEDIA_MCF_GRP_ATTR *attr);
int MEDIA_MCF_DestroyGrp(int grp);
int MEDIA_MCF_ResetGrp(int grp);
int MEDIA_MCF_StartGrp(int grp);
int MEDIA_MCF_StopGrp(int grp);
int MEDIA_MCF_SetGrpAttr(int grp, const MEDIA_MCF_GRP_ATTR *attr);
int MEDIA_MCF_GetGrpAttr(int grp, MEDIA_MCF_GRP_ATTR *attr);
int MEDIA_MCF_SetAlgParam(int grp, const MEDIA_MCF_ALG_PARAM *param);
int MEDIA_MCF_GetAlgParam(int grp, MEDIA_MCF_ALG_PARAM *param);
int MEDIA_MCF_SendPipeFrame(int grp, int pipe, MEDIA_BUFFER frame, int timeout_ms);
int MEDIA_MCF_SetChnAttr(int grp, int chn, const MEDIA_MCF_CHN_ATTR *attr);
int MEDIA_MCF_GetChnAttr(int grp, int chn, MEDIA_MCF_CHN_ATTR *attr);
int MEDIA_MCF_EnableChn(int grp, int chn);
int MEDIA_MCF_DisableChn(int grp, int chn);
int MEDIA_MCF_GetChnFrame(int grp, int chn, MEDIA_BUFFER *frame, int timeout_ms);
int MEDIA_MCF_ReleaseChnFrame(int grp, int chn, MEDIA_BUFFER frame);
int MEDIA_MCF_GetLastPerf(int grp, MEDIA_MCF_PERF *perf);

/* 第一版显式不支持的高级接口，全部返回 MEDIA_ERR_UNSUPPORTED。 */
int MEDIA_MCF_SetGrpCrop(int grp, const MEDIA_MCF_CROP_INFO *crop);
int MEDIA_MCF_GetGrpCrop(int grp, MEDIA_MCF_CROP_INFO *crop);
int MEDIA_MCF_SetExtChnAttr(int grp, int chn, const MEDIA_MCF_EXT_CHN_ATTR *attr);
int MEDIA_MCF_GetExtChnAttr(int grp, int chn, MEDIA_MCF_EXT_CHN_ATTR *attr);
int MEDIA_MCF_SetChnCrop(int grp, int chn, const MEDIA_MCF_CROP_INFO *crop);
int MEDIA_MCF_GetChnCrop(int grp, int chn, MEDIA_MCF_CROP_INFO *crop);
int MEDIA_MCF_SetChnMirror(int grp, int chn, int enable);
int MEDIA_MCF_GetChnMirror(int grp, int chn, int *enable);
int MEDIA_MCF_SetChnFlip(int grp, int chn, int enable);
int MEDIA_MCF_GetChnFlip(int grp, int chn, int *enable);
int MEDIA_MCF_SetChnRotation(int grp, int chn, int rotation);
int MEDIA_MCF_GetChnRotation(int grp, int chn, int *rotation);
int MEDIA_MCF_ChnAttachVBPool(int grp, int chn, int pool_id);
int MEDIA_MCF_ChnDetachVBPool(int grp, int chn);
int MEDIA_MCF_SetChnStrideAlign(int grp, int chn, unsigned int align);
int MEDIA_MCF_GetChnStrideAlign(int grp, int chn, unsigned int *align);
int MEDIA_MCF_GetChnRgnLuma(int grp, int chn, unsigned int num,
                            const MEDIA_MCF_RECT *regions, uint64_t *luma,
                            int timeout_ms);
int MEDIA_MCF_GetChnFd(int grp, int chn);
int MEDIA_MCF_GetGrpFrame(int grp, MEDIA_BUFFER *color, MEDIA_BUFFER *mono,
                          int timeout_ms);
int MEDIA_MCF_ReleaseGrpFrame(int grp, MEDIA_BUFFER color, MEDIA_BUFFER mono);
int MEDIA_MCF_SetVIAttr(int grp, const MEDIA_MCF_VI_ATTR *attr);
int MEDIA_MCF_GetVIAttr(int grp, MEDIA_MCF_VI_ATTR *attr);

/* MCF_FUSION_CL 旧版简化接口（单组、单通道、固定 LEGACY 算法）。 */
typedef enum {
    MEDIA_MCF_FUSION_CL_PATH_FUSION = 0,
    MEDIA_MCF_FUSION_CL_PATH_COLOR = 1,
    MEDIA_MCF_FUSION_CL_PATH_MONO = 2,
} MEDIA_MCF_FUSION_CL_PATH;

typedef enum {
    MEDIA_MCF_FUSION_CL_NORMALIZE_NONE = 0,
    MEDIA_MCF_FUSION_CL_NORMALIZE_MEAN_STD = 1,
} MEDIA_MCF_FUSION_CL_NORMALIZE_MODE;

typedef struct {
    int width;              /* 输入/输出宽度，必须为偶数 */
    int height;             /* 输入/输出高度，必须为偶数 */
    int format;             /* 输入与输出格式，第一版只支持 MEDIA_FORMAT_NV12 */
    int input_depth;        /* 输入队列深度；0 表示缺省 4 */
    int output_pool_id;     /* 输出 Pool；Start 前必须已创建足量 buffer */
    int input_stride;       /* 输入行字节数；0 表示按 width 向上对齐到 64 */
    int output_stride;      /* 输出行字节数；0 表示按 width 向上对齐到 64 */
    int path;               /* 0 = 融合，1 = 彩色直通，2 = 单色直通 */
    int normalize_mode;     /* mono 归一化模式 */
    int blur_radius;        /* 旧版单尺度底图半径；>2 按 2 处理 */
    float base_alpha;       /* 低频混合权重，缺省 0.25 */
    float detail_gain;      /* 细节注入增益，缺省 0.80 */
    float alpha_min;        /* 细节权重下限，缺省 0.15 */
    float alpha_max;        /* 细节权重上限，缺省 0.85 */
    float gain_min;         /* 归一化增益下限，缺省 0.5 */
    float gain_max;         /* 归一化增益上限，缺省 2.0 */
    float epsilon;          /* 数值保护项，缺省 1e-6 */
} MEDIA_MCF_FUSION_CL_ATTR;

typedef struct {
    double stats_kernel_ms;    /* 归一化统计耗时 */
    double fusion_kernel_ms;   /* 融合 pass 耗时 */
    double gpu_total_ms;       /* GPU 总耗时 */
} MEDIA_MCF_FUSION_CL_PERF;

int MEDIA_MCF_FUSION_CL_CreateGrp(int grp, const MEDIA_MCF_FUSION_CL_ATTR *attr);
int MEDIA_MCF_FUSION_CL_DestroyGrp(int grp);
int MEDIA_MCF_FUSION_CL_Start(int grp);
int MEDIA_MCF_FUSION_CL_Stop(int grp);
int MEDIA_MCF_FUSION_CL_Enable(int grp);
int MEDIA_MCF_FUSION_CL_Disable(int grp);
int MEDIA_MCF_FUSION_CL_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_MCF_FUSION_CL_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_MCF_FUSION_CL_GetLastPerf(int grp, MEDIA_MCF_FUSION_CL_PERF *perf);

/*
 * ===== 以下为插件新增内容（旧源码没有）=====
 *
 * 旧模块的 legacy 参数是内部结构；移植后把它公开，便于应用在创建分组后
 * 通过 "mcf_legacy_param" 参数读写。字段顺序固定，只在末尾追加。
 */
typedef struct {
    int blur_radius;   /* 单尺度底图半径，1 或 2；缺省 2 */
    float base_alpha;  /* 低频混合权重，缺省 0.25 */
    float detail_gain; /* 细节注入增益，缺省 0.80 */
    float alpha_min;   /* 细节权重下限，缺省 0.15 */
    float alpha_max;   /* 细节权重上限，缺省 0.85 */
} MEDIA_MCF_FUSION_LEGACY_PARAM;

/*
 * 插件实例配置：MEDIA_MCF_CreateGrp / MEDIA_MCF_FUSION_CL_CreateGrp 组装后
 * 通过 MEDIA_PLUGIN_CreateModule 传给插件。应用通常不必直接构造它。
 */
typedef struct {
    MEDIA_MCF_GRP_ATTR grp_attr;                    /* 分组属性 */
    MEDIA_MCF_CHN_ATTR chn_attr;                    /* 输出通道属性 */
    MEDIA_MCF_ALG_PARAM alg_param;                  /* 算法参数 */
    MEDIA_MCF_FUSION_LEGACY_PARAM legacy_param;     /* 旧版单尺度参数 */
    int chn_configured;                             /* 1 = chn_attr 已显式配置 */
    int chn_enabled;                                /* 1 = 通道使能 */
    int legacy_compat;                              /* 1 = 由 MEDIA_MCF_FUSION_CL_CreateGrp 创建 */
} MEDIA_MCF_FUSION_CONFIG;

/* 写入 MEDIA_MCF_ALG_PARAM 的旧版缺省值。 */
static inline void MEDIA_MCF_GetDefaultAlgParam(MEDIA_MCF_ALG_PARAM *param)
{
    if (param == NULL) {
        return;
    }
    memset(param, 0, sizeof(*param));
    param->mode = MEDIA_MCF_ALG_LOCAL_REALTIME;
    param->normalize_mode = MEDIA_MCF_NORMALIZE_MEAN_STD;
    param->local_window = 5;
    param->local_map_sigma = 0.8f;
    param->local_detail_sigma = 3.0f;
    param->local_detail_gain = 0.90f;
    param->hf_cfg.filter_sigma = 1.0f;
    param->hf_cfg.detail_gain = 1.15f;
    param->hf_cfg.base_alpha = 0.0f;
    param->mf_cfg.filter_sigma = 3.0f;
    param->mf_cfg.detail_gain = 0.60f;
    param->mf_cfg.base_alpha = 0.0f;
    param->lf_cfg.filter_sigma = 3.0f;
    param->lf_cfg.detail_gain = 0.0f;
    param->lf_cfg.base_alpha = 0.08f;
    param->flatness_threshold = 0.0007f;
    param->gain_min = 0.5f;
    param->gain_max = 2.0f;
    param->epsilon = 1.0e-6f;
    param->registration_confidence = 1.0f;
    param->fallback_threshold = 0.5f;
}

/* 写入 MEDIA_MCF_FUSION_LEGACY_PARAM 的旧版缺省值。 */
static inline void MEDIA_MCF_GetDefaultLegacyParam(MEDIA_MCF_FUSION_LEGACY_PARAM *param)
{
    if (param == NULL) {
        return;
    }
    memset(param, 0, sizeof(*param));
    param->blur_radius = 2;
    param->base_alpha = 0.25f;
    param->detail_gain = 0.80f;
    param->alpha_min = 0.15f;
    param->alpha_max = 0.85f;
}

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_MCF_FUSION_H */
