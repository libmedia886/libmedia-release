/*
 * EDOF_CL 入门教程：扩展景深（Extended Depth Of Field）
 *
 * 显微/微距场景下景深很浅：同一时刻只有一部分区域在焦内。EDOF 的输入是
 * 同一场景在两个焦面拍到的两张图（近焦面一张、远焦面一张），逐块比较局部
 * 清晰度，清晰的那一路给更大权重，输出尽量让每块都取到清晰的那一张。
 *
 * 它和“平均/融合”区别很大：不是把两张混在一起，而是按清晰度挑选。
 *
 * 输入端口有语义：
 *   input0 = 第一个焦面
 *   input1 = 第二个焦面
 * 两路必须同尺寸同格式，且是同一场景的两个焦面，否则“挑清晰的”没有意义。
 *
 * 本程序不读摄像头，自己在内存里造两张 640x480 的 NV12 图：
 *   input0（source=0）：从左到右变亮，UV 偏紫
 *   input1（source=1）：从上到下变亮，UV 偏蓝
 *
 * 命令行：edof_tutorial [mode] [output] [plugin]
 *   mode=0 focus_radius=4，统计窗口 9x9
 *   mode=1 focus_radius=8，统计窗口 17x17
 * 窗口越大越抗噪，但 GPU 工作量和边缘扩散也更大。
 *
 * 阅读顺序：
 *   [1] 初始化 + 加载插件 -> [2] 两个 layout + 三个 Pool -> [3] 配置属性
 *   -> [4] CreateModule / Start -> [5] 送帧（先 input1 后 input0）-> [6] 取帧保存
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "media_api.h"
#include "media_edof.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 640
#define H 480
#define POOL_A 27       /* input0（近焦面）Pool */
#define POOL_B 28       /* input1（远焦面）Pool */
#define POOL_OUT 29     /* 融合结果 Pool */
#define POOL_MASK 30    /* 预留的遮罩 Pool（本教程未使用，但要在退出时销毁） */
#define FMT MEDIA_FORMAT_NV12

/*
 * 造一帧测试图并设好 metadata。
 * source=0 生成水平渐变（近焦面），source=1 生成垂直渐变（远焦面）。
 * 返回 0 表示 buffer 已填好并可交给模块。
 */
static int make_frame(int pool, const MEDIA_VIDEO_LAYOUT *layout, size_t bytes, int source,
                      MEDIA_BUFFER *out)
{
    MEDIA_BUFFER_META m;
    unsigned char *p;

    if (MEDIA_POOL_GetBuffer(pool, out))
        return -1;
    p = MEDIA_POOL_GetVaddr(*out);
    if (!p || MEDIA_POOL_BeginCpuAccess(*out, DMA_BUF_SYNC_WRITE))
        goto fail;
    memset(p, 0, bytes);

#if FMT == MEDIA_FORMAT_RGB888
    /* 未启用的分支：保留它可以让你把 FMT 改成 RGB888 直接试。
     * RGB888 每像素 3 字节，所以内层步长是 3*x。 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned char *q = p + (size_t)y * layout->pitch_bytes[0] + 3 * x;

            q[0] = (unsigned char)(source ? 20 : x * 255 / W);
            q[1] = (unsigned char)(source ? y * 255 / H : 30);
            q[2] = (unsigned char)(source ? 230 : 20);
        }
#else
    /* Y 平面：渐变，亮度范围 40..190。 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[(size_t)y * layout->pitch_bytes[0] + x] =
                (unsigned char)(source ? 40 + y * 150 / H : 40 + x * 150 / W);
    /* UV 平面：交错 U,V，所以按 x += 2 走。两路颜色不同，
     * 融合后偏色能帮助判断输出取了哪一路。 */
    for (int y = 0; y < H / 2; y++)
        for (int x = 0; x < W; x += 2) {
            unsigned char *q = p + layout->offset_bytes[1] +
                               (size_t)y * layout->pitch_bytes[1] + x;

            q[0] = source ? 90 : 160;
            q[1] = source ? 200 : 90;
        }
#endif

    MEDIA_POOL_EndCpuAccess(*out, DMA_BUF_SYNC_WRITE);

    /* 只填像素是不够的：模块靠 metadata 判断这块 DMA-BUF 是什么格式、
     * 多大、怎么排。不设 metadata 就送帧，模块无法正确解释内存。 */
    memset(&m, 0, sizeof(m));
    m.valid = 1;
    m.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    m.media_format = FMT;
    m.len = (uint32_t)bytes;
    m.u.video = *layout;
    m.seq = 1;
    m.time_stamp = 1000000;
    if (MEDIA_BUFFER_SetMeta(*out, &m) == 0)
        return 0;

fail:
    /* 任何一步失败都要把 buffer 还给 Pool，否则 Pool 会慢慢被占满。 */
    MEDIA_POOL_PutBuffer(*out);
    return -1;
}

/* 造一帧并发到指定输入端口。端口名必须完全匹配（"input0" / "input1"）。 */
static int send_frame(int pool, const MEDIA_VIDEO_LAYOUT *layout, size_t bytes, int source,
                      const char *port)
{
    MEDIA_BUFFER b;
    int r = make_frame(pool, layout, bytes, source, &b);

    if (r)
        return r;
    r = MEDIA_SYS_SendFrame("EDOF_CL", 0, port, b, 1000);
    /* 送帧成功后所有权归模块，不能再 PutBuffer；失败时 buffer 还在我们手里。 */
    if (r)
        MEDIA_POOL_PutBuffer(b);
    return r;
}

int main(int argc, char **argv)
{
    int mode = argc > 1 ? atoi(argv[1]) : 0,
        r = 1, created = 0, started = 0;
    const char *output = argc > 2 ? argv[2] : "edof_frame.nv12";
    const char *plugin = argc > 3 ? argv[3] : "../../plugins/libmedia_edof.so";
    MEDIA_VIDEO_LAYOUT L, OL;
    size_t in_bytes = 0, out_bytes = 0;
    MEDIA_EDOF_CL_ATTR a;
    MEDIA_BUFFER out;
    MEDIA_BUFFER_META meta;
    FILE *fp;

    /* [1] 初始化并加载插件。插件和它的 .sig 必须成对存在。 */
    if (MEDIA_SYS_Init() != 0)
        return 1;
    if (MEDIA_PLUGIN_Load(plugin) < 0) {
        fprintf(stderr, "load %s failed\n", plugin);
        goto done;
    }

    /* [2] 输入和输出都是 W x H、同格式，所以两个 layout 的尺寸一样，
     *     但仍然分开算：将来把输出换成别的格式时不用改结构。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, FMT, &L, &in_bytes) != 0 ||
        MEDIA_VIDEO_BuildLinearLayout(W, H, FMT, &OL, &out_bytes) != 0)
        goto done;
    if (MEDIA_POOL_CreateWithMode(POOL_A, in_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_B, in_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_OUT, out_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0)
        goto done;

    /* [3] 关键配置。stride 必须来自 layout，不能手写 width：
     *     硬件对齐后每行通常比 W 长，写错会让模块按错误的行宽读内存。
     *
     *     focus_radius 决定“局部清晰度”在多大窗口里统计：
     *     窗口边长 = 2*r+1，所以 r=4 是 9x9，r=8 是 17x17。
     *     score_eps 是两路清晰度都很低时的分母保护，避免除零放大噪声。 */
    memset(&a, 0, sizeof(a));
    a.width = W;
    a.height = H;
    a.format = FMT;
    a.input_stride = L.pitch_bytes[0];           /* 输入每行实际字节数 */
    a.output_stride = OL.pitch_bytes[0];         /* 输出每行实际字节数 */
    a.output_pool_id = POOL_OUT;
    a.focus_radius = mode ? 8 : 4;               /* 清晰度统计窗口半径 */
    a.score_eps = 1e-3f;                         /* 权重分母保护值 */

    /* [4] 创建并启动。 */
    r = MEDIA_PLUGIN_CreateModule("EDOF_CL", 0, &a, sizeof(a));
    if (r) {
        fprintf(stderr, "create failed: %d\n", r);
        goto done;
    }
    created = 1;

    r = MEDIA_PLUGIN_Start("EDOF_CL", 0);
    if (r) {
        fprintf(stderr, "start failed: %d\n", r);
        goto done;
    }
    started = 1;

    /* [5] 先送次路再送主路：主路到达时可能立刻触发一次处理，
     *     如果次路还没到，那一帧就是缺一路的结果。 */
    if ((r = send_frame(POOL_B, &L, in_bytes, 1, "input1")) != 0 ||
        (r = send_frame(POOL_A, &L, in_bytes, 0, "input0")) != 0) {
        fprintf(stderr, "send failed: %d\n", r);
        goto done;
    }

    /* [6] 取融合结果。 */
    r = MEDIA_SYS_GetFrame("EDOF_CL", 0, "output0", &out, 5000);
    if (r) {
        fprintf(stderr, "get failed: %d\n", r);
        goto done;
    }

    if (MEDIA_BUFFER_GetMeta(out, &meta) != 0 || !meta.valid ||
        MEDIA_POOL_BeginCpuAccess(out, DMA_BUF_SYNC_READ) != 0) {
        fprintf(stderr, "output metadata/access failed\n");
        MEDIA_SYS_ReleaseFrame("EDOF_CL", 0, "output0", out);
        r = 1;
        goto done;
    }

    /* meta.len 是有效字节数，可能比 Pool 容量小（有对齐填充）。 */
    fp = fopen(output, "wb");
    void *p = MEDIA_POOL_GetVaddr(out);
    size_t n = meta.len <= MEDIA_POOL_GetSize(out) ? meta.len : MEDIA_POOL_GetSize(out);

    r = (fp && p && fwrite(p, 1, n, fp) == n) ? 0 : 1;
    if (fp)
        fclose(fp);
    MEDIA_POOL_EndCpuAccess(out, DMA_BUF_SYNC_READ);
    MEDIA_SYS_ReleaseFrame("EDOF_CL", 0, "output0", out);
    if (r)
        fprintf(stderr, "write failed: %s\n", output);
    else
        printf("EDOF_CL mode=%d %ux%u bytes=%zu -> %s\n", mode,
               meta.u.video.visible_width, meta.u.video.visible_height, n, output);

done:
    /* 逆序拆解：停模块 -> 销毁模块 -> 销毁 Pool -> 退出系统。
     * POOL_MASK 从未创建过，但 Destroy 一个不存在的 Pool 是安全的空操作。 */
    if (started)
        MEDIA_PLUGIN_Stop("EDOF_CL", 0);
    if (created)
        MEDIA_PLUGIN_DestroyModule("EDOF_CL", 0);
    MEDIA_POOL_Destroy(POOL_MASK);
    MEDIA_POOL_Destroy(POOL_OUT);
    MEDIA_POOL_Destroy(POOL_B);
    MEDIA_POOL_Destroy(POOL_A);
    MEDIA_SYS_Exit();
    return r ? 1 : 0;
}
