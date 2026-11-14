/*
 * MCF_FUSION 入门教程：彩色 + 单色（红外/夜视）融合
 *
 * MCF 是“多相机融合”模块，把一路彩色和一路单色画面配准后融合。它和其他
 * 模块有个明显区别：**不用端口字符串**，而是用 pipe 常量：
 *   MEDIA_MCF_PIPE_COLOR = 1  →  彩色路，对应 input0
 *   MEDIA_MCF_PIPE_MONO  = 0  →  单色路，对应 input1
 * 送帧用 MEDIA_MCF_SendPipeFrame(grp, pipe, buf, timeout)，取帧用
 * MEDIA_MCF_GetChnFrame(grp, chn, &buf, timeout)，失败必须配对
 * MEDIA_MCF_ReleaseChnFrame，不能调用通用的 ReleaseFrame。
 *
 * 三种处理路径（命令行第 1 个参数 path）：
 *   path=0 FUSION：彩色 + 单色真正融合
 *   path=1 COLOR ：只直通彩色路
 *   path=2 MONO  ：只直通单色路
 * 三条路径输入相同，方便对比“融合”和“直通”的差异。
 *
 * 本程序不读摄像头，自己在内存里造两张 640x480 的 NV12 图：
 *   彩色路（mono=0）：从左到右变亮，UV 偏紫
 *   单色路（mono=1）：从上到下变亮，UV 固定 128（即无色）
 *
 * 命令行：mcf_fusion_tutorial [path] [output] [plugin]
 *
 * 阅读顺序：
 *   [1] 初始化 + 加载插件 -> [2] layout + 三个 Pool -> [3] 组/通道/算法参数
 *   -> [4] CreateGrp / SetChnAttr / EnableChn / StartGrp
 *   -> [5] 两路各送一帧 -> [6] 取通道帧保存 -> [7] 按创建顺序逆序拆解
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_mcf_fusion.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 640
#define H 480
#define POOL_COLOR 27   /* 彩色路输入 Pool */
#define POOL_MONO 28    /* 单色路输入 Pool */
#define POOL_OUT 29     /* 融合结果 Pool */

/*
 * 造一帧测试图并设好 metadata。
 * mono=0 生成水平渐变（彩色路），mono=1 生成垂直渐变（单色路）。
 * 返回 0 表示 buffer 已填好并可交给模块。
 */
static int make_frame(int pool, const MEDIA_VIDEO_LAYOUT *l, size_t bytes, int mono,
                      MEDIA_BUFFER *b)
{
    MEDIA_BUFFER_META m;
    unsigned char *p;

    if (MEDIA_POOL_GetBuffer(pool, b))
        return -1;
    p = MEDIA_POOL_GetVaddr(*b);
    if (!p || MEDIA_POOL_BeginCpuAccess(*b, DMA_BUF_SYNC_WRITE))
        goto fail;
    memset(p, 0, bytes);

    /* Y 平面：单色路纵向渐变，彩色路横向渐变。 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[(size_t)y * l->pitch_bytes[0] + x] =
                (unsigned char)(mono ? 40 + y * 170 / H : 40 + x * 170 / W);
    /* UV 平面：单色路两分量都写 128，表示“无色”；
     * 彩色路给一个偏色，融合后能看出彩色分量的贡献。 */
    for (int y = 0; y < H / 2; y++)
        for (int x = 0; x < W; x += 2) {
            unsigned char *q = p + l->offset_bytes[1] +
                               (size_t)y * l->pitch_bytes[1] + x;

            q[0] = mono ? 128 : 90;
            q[1] = mono ? 128 : 180;
        }
    MEDIA_POOL_EndCpuAccess(*b, DMA_BUF_SYNC_WRITE);

    /* metadata 必须填：模块靠它判断格式、尺寸、pitch。
     * seq 和时间戳用于两路配对，这里两路都给同样的值。 */
    memset(&m, 0, sizeof(m));
    m.valid = 1;
    m.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    m.media_format = MEDIA_FORMAT_NV12;
    m.len = (uint32_t)bytes;
    m.u.video = *l;
    m.time_stamp = 1000000;
    m.seq = 1;
    if (MEDIA_BUFFER_SetMeta(*b, &m) == 0)
        return 0;

fail:
    /* 任何一步失败都要把 buffer 还给 Pool。 */
    MEDIA_POOL_PutBuffer(*b);
    return -1;
}

int main(int argc, char **argv)
{
    int path = argc > 1 ? atoi(argv[1]) : 0,
        r = 1, created = 0, started = 0, chn = 0;
    const char *out_path = argc > 2 ? argv[2] : "mcf_fusion_frame.nv12";
    const char *plugin = argc > 3 ? argv[3] : "../../plugins/libmedia_mcf_fusion.so";
    MEDIA_VIDEO_LAYOUT l;
    size_t bytes = 0;
    MEDIA_MCF_GRP_ATTR ga;
    MEDIA_MCF_CHN_ATTR ca;
    MEDIA_MCF_ALG_PARAM ap;
    MEDIA_BUFFER c, m, out;
    MEDIA_BUFFER_META meta;
    FILE *fp;

    /* [1] 初始化并加载插件。插件和它的 .sig 必须成对存在。 */
    if (MEDIA_SYS_Init() != 0 || MEDIA_PLUGIN_Load(plugin) < 0)
        goto done;

    /* [2] 两路输入和输出同尺寸同格式，所以一个 layout 够用；
     *     但三个 Pool 必须分开建：模块要知道哪块内存是哪一路的。
     *     MCF 走 GPU，必须用 NATIVE 分配（真正的 dma-buf）。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_NV12, &l, &bytes) != 0)
        goto done;
    if (MEDIA_POOL_CreateWithMode(POOL_COLOR, bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_MONO, bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_OUT, bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0)
        goto done;

    /* [3] 三组参数，MCF 比其他模块多一层“组”的概念：
     *     GRP_ATTR 描述整个组；PIPE_ATTR 描述每路输入；
     *     CHN_ATTR 描述输出通道（本例只用 chn 0）。 */
    memset(&ga, 0, sizeof(ga));
    ga.sync_pipe_en = 0;                 /* 不强制两路同步到达 */
    ga.path = path;                      /* 0=FUSION，1=COLOR，2=MONO */
    ga.reuse_last_frame = 1;             /* 某一路迟到时复用上一帧 */
    ga.timestamp_tolerance_us = 2000;    /* 两路时间戳允许的偏差 */
    ga.depth = 4;                        /* 每路排队深度 */
    ga.color_pipe_attr = (MEDIA_MCF_PIPE_ATTR){MEDIA_MCF_PIPE_COLOR, W, H, MEDIA_FORMAT_NV12,
                                              (int)l.pitch_bytes[0], 4};
    ga.mono_pipe_attr = (MEDIA_MCF_PIPE_ATTR){MEDIA_MCF_PIPE_MONO, W, H, MEDIA_FORMAT_NV12,
                                             (int)l.pitch_bytes[0], 4};

    r = MEDIA_MCF_CreateGrp(0, &ga);
    if (r)
        goto done;
    created = 1;

    memset(&ca, 0, sizeof(ca));
    ca.width = W;
    ca.height = H;
    ca.format = MEDIA_FORMAT_NV12;
    ca.stride = l.pitch_bytes[0];
    ca.depth = 4;
    ca.output_pool_id = POOL_OUT;        /* 输出 buffer 从这里取 */

    r = MEDIA_MCF_SetChnAttr(0, 0, &ca);
    if (r)
        goto done;

    r = MEDIA_MCF_EnableChn(0, 0);
    if (r)
        goto done;
    chn = 1;

    /* 算法参数：本地实时模式，不做归一化，局部细节增强 0.9 倍。 */
    memset(&ap, 0, sizeof(ap));
    ap.mode = MEDIA_MCF_ALG_LOCAL_REALTIME;
    ap.normalize_mode = MEDIA_MCF_NORMALIZE_NONE;
    ap.local_window = 5;                 /* 局部统计窗口 */
    ap.local_map_sigma = 0.8f;           /* 融合权重图的平滑 */
    ap.local_detail_sigma = 3.0f;        /* 细节层的高斯尺度 */
    ap.local_detail_gain = 0.9f;         /* 细节注入强度 */
    ap.registration_confidence = 1.0f;   /* 配准置信度（合成图无需配准） */
    ap.fallback_threshold = 0.0f;
    ap.epsilon = 1e-6f;                  /* 防止除零 */
    MEDIA_MCF_SetAlgParam(0, &ap);

    /* [4] 先 StartGrp 才能送帧。 */
    r = MEDIA_MCF_StartGrp(0);
    if (r)
        goto done;
    started = 1;

    /* [5] 两路各造一帧送进去。送帧成功后所有权归模块，失败时自己 PutBuffer。 */
    if (make_frame(POOL_COLOR, &l, bytes, 0, &c) || make_frame(POOL_MONO, &l, bytes, 1, &m))
        goto done;
    if (MEDIA_MCF_SendPipeFrame(0, MEDIA_MCF_PIPE_COLOR, c, 1000) != 0) {
        MEDIA_POOL_PutBuffer(c);
        goto done;
    }
    if (MEDIA_MCF_SendPipeFrame(0, MEDIA_MCF_PIPE_MONO, m, 1000) != 0) {
        MEDIA_POOL_PutBuffer(m);
        goto done;
    }

    /* [6] 从输出通道取一帧。MCF 用 GetChnFrame，不是 MEDIA_SYS_GetFrame。 */
    r = MEDIA_MCF_GetChnFrame(0, 0, &out, 5000);
    if (r)
        goto done;
    if (MEDIA_BUFFER_GetMeta(out, &meta) != 0 ||
        MEDIA_POOL_BeginCpuAccess(out, DMA_BUF_SYNC_READ) != 0) {
        MEDIA_MCF_ReleaseChnFrame(0, 0, out);
        goto done;
    }

    fp = fopen(out_path, "wb");
    void *p = MEDIA_POOL_GetVaddr(out);
    size_t n = meta.len < MEDIA_POOL_GetSize(out) ? meta.len : MEDIA_POOL_GetSize(out);

    r = (fp && p && fwrite(p, 1, n, fp) == n) ? 0 : 1;
    if (fp)
        fclose(fp);
    MEDIA_POOL_EndCpuAccess(out, DMA_BUF_SYNC_READ);
    MEDIA_MCF_ReleaseChnFrame(0, 0, out);
    if (!r)
        printf("MCF path=%d %ux%u bytes=%zu -> %s\n", path, meta.u.video.visible_width,
               meta.u.video.visible_height, n, out_path);

done:
    /* [7] 逆序拆解：StopGrp -> DisableChn -> DestroyGrp -> 销毁 Pool -> Exit。
     * 即使中间某一步失败，这里的标志位保证只释放真正创建过的对象。 */
    if (started)
        MEDIA_MCF_StopGrp(0);
    if (chn)
        MEDIA_MCF_DisableChn(0, 0);
    if (created)
        MEDIA_MCF_DestroyGrp(0);
    MEDIA_POOL_Destroy(POOL_OUT);
    MEDIA_POOL_Destroy(POOL_MONO);
    MEDIA_POOL_Destroy(POOL_COLOR);
    MEDIA_SYS_Exit();
    return r ? 1 : 0;
}
