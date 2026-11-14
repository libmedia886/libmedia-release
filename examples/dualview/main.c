/*
 * DUALVIEW 入门教程：两路画面排版（并排 / 隔行）
 *
 * DUALVIEW 只做“排版”，不做颜色融合。它把两路画面拼到一张输出图上，
 * 常用于双目预览、左右相机同步检查，以及 3D 显示前的打包。
 *
 * 两种排版方式：
 *   mode=0 side-by-side：输出 1280x480，左 640 列来自 input0，右 640 列来自 input1
 *   mode=1 line-by-line：输出 640x480，偶数行来自 input0，奇数行来自 input1
 *
 * 注意本模块只接受 RGB888 LINEAR（不是 NV12）。所以查看输出要用
 * ffplay -pixel_format rgb24，而且两种 mode 的输出尺寸不同。
 *
 * 本程序不读摄像头，自己在内存里造两张 640x480 的 RGB888 图：
 *   input0（主路）：蓝色分量从左到右增强
 *   input1（次路）：红色分量从上到下增强
 *
 * 命令行：dualview_tutorial [mode] [output] [plugin]
 *
 * 阅读顺序：
 *   [1] 初始化 + 加载插件 -> [2] 输出尺寸随 mode 变化，两个 layout 分开算
 *   -> [3] 配置属性 -> [4] CreateModule / Start -> [5] 送帧 -> [6] 取帧保存
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "media_api.h"
#include "media_dualview.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 640
#define H 480
#define POOL_A 27       /* 主路 input0 的 Pool */
#define POOL_B 28       /* 次路 input1 的 Pool */
#define POOL_OUT 29     /* 排版结果 Pool */
#define POOL_MASK 30    /* 预留的遮罩 Pool（本教程未使用，但要在退出时销毁） */
#define FMT MEDIA_FORMAT_RGB888   /* 本模块只吃 RGB888，不能改成 NV12 */

/*
 * 造一帧测试图并设好 metadata。
 * source=0 生成“蓝→亮”的水平渐变（主路），
 * source=1 生成“红→亮”的垂直渐变（次路）。
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
    /* RGB888 每像素 3 字节，所以内层步长是 3*x。
     * q[0]=R q[1]=G q[2]=B，按这个顺序写。 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned char *q = p + (size_t)y * layout->pitch_bytes[0] + 3 * x;

            q[0] = (unsigned char)(source ? 20 : x * 255 / W);
            q[1] = (unsigned char)(source ? y * 255 / H : 30);
            q[2] = (unsigned char)(source ? 230 : 20);
        }
#else
    /* 未启用的分支：如果将来 DUALVIEW 支持 NV12，可以把 FMT 改过来复用。
     * 当前模块不接受 NV12，保留这段只是为了让你看到两种格式的写法差异。 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[(size_t)y * layout->pitch_bytes[0] + x] =
                (unsigned char)(source ? 40 + y * 150 / H : 40 + x * 150 / W);
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
    r = MEDIA_SYS_SendFrame("DUALVIEW", 0, port, b, 1000);
    /* 送帧成功后所有权归模块，不能再 PutBuffer；失败时 buffer 还在我们手里。 */
    if (r)
        MEDIA_POOL_PutBuffer(b);
    return r;
}

int main(int argc, char **argv)
{
    int mode = argc > 1 ? atoi(argv[1]) : 0,
        r = 1, created = 0, started = 0;
    const char *output = argc > 2 ? argv[2] : "dualview_frame.rgb";
    const char *plugin = argc > 3 ? argv[3] : "../../plugins/libmedia_dualview.so";
    MEDIA_VIDEO_LAYOUT L, OL;
    size_t in_bytes = 0, out_bytes = 0;
    MEDIA_DUALVIEW_ATTR a;
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

    /* [2] 输入固定 W x H，输出宽度随 mode 变：
     *     mode 0 并排 → 2*W x H
     *     mode 1 隔行 → W x H（两路交错进同一个尺寸）
     *     Pool 大小必须跟着输出 layout 走，否则模块写不下。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, FMT, &L, &in_bytes) != 0 ||
        MEDIA_VIDEO_BuildLinearLayout((mode == 0 ? 2 * W : W), H, FMT, &OL, &out_bytes) != 0)
        goto done;
    if (MEDIA_POOL_CreateWithMode(POOL_A, in_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_B, in_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_OUT, out_bytes, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0)
        goto done;

    /* [3] 关键配置：输入尺寸/stride 来自 layout；输出宽度同样随 mode 变。
     *     inputs[1].x = W 只在 mode 0 有意义（把次路放到右半边），
     *     mode 1 隔行时会忽略这个坐标。 */
    memset(&a, 0, sizeof(a));
    a.input_width = W;
    a.input_height = H;
    a.input_stride = L.pitch_bytes[0];             /* 输入每行实际字节数 */
    a.output_width = (mode == 0 ? 2 * W : W);      /* 随排版方式变化 */
    a.output_height = H;
    a.output_stride = OL.pitch_bytes[0];           /* 输出每行实际字节数 */
    a.format = FMT;
    a.mode = mode;                                 /* 0=并排，1=隔行 */
    a.output_pool_id = POOL_OUT;
    a.inputs[0].enabled = 1;
    a.inputs[1].enabled = 1;
    a.inputs[1].x = W;                             /* 并排时次路放右半边 */

    /* [4] 创建并启动。 */
    r = MEDIA_PLUGIN_CreateModule("DUALVIEW", 0, &a, sizeof(a));
    if (r) {
        fprintf(stderr, "create failed: %d\n", r);
        goto done;
    }
    created = 1;

    r = MEDIA_PLUGIN_Start("DUALVIEW", 0);
    if (r) {
        fprintf(stderr, "start failed: %d\n", r);
        goto done;
    }
    started = 1;

    /* [5] 先送次路再送主路：主路到达时可能立刻触发一次排版，
     *     如果次路还没到，那一帧就是缺一路的结果。 */
    if ((r = send_frame(POOL_B, &L, in_bytes, 1, "input1")) != 0 ||
        (r = send_frame(POOL_A, &L, in_bytes, 0, "input0")) != 0) {
        fprintf(stderr, "send failed: %d\n", r);
        goto done;
    }

    /* [6] 取排版结果。 */
    r = MEDIA_SYS_GetFrame("DUALVIEW", 0, "output0", &out, 5000);
    if (r) {
        fprintf(stderr, "get failed: %d\n", r);
        goto done;
    }

    if (MEDIA_BUFFER_GetMeta(out, &meta) != 0 || !meta.valid ||
        MEDIA_POOL_BeginCpuAccess(out, DMA_BUF_SYNC_READ) != 0) {
        fprintf(stderr, "output metadata/access failed\n");
        MEDIA_SYS_ReleaseFrame("DUALVIEW", 0, "output0", out);
        r = 1;
        goto done;
    }

    /* meta.len 是有效字节数，可能比 Pool 容量小（有对齐填充）。
     * 输出宽度写进打印里：mode 0 是 1280，mode 1 是 640，
     * 拿去看 ffplay 的 -video_size 时就以这个值为准。 */
    fp = fopen(output, "wb");
    void *p = MEDIA_POOL_GetVaddr(out);
    size_t n = meta.len <= MEDIA_POOL_GetSize(out) ? meta.len : MEDIA_POOL_GetSize(out);

    r = (fp && p && fwrite(p, 1, n, fp) == n) ? 0 : 1;
    if (fp)
        fclose(fp);
    MEDIA_POOL_EndCpuAccess(out, DMA_BUF_SYNC_READ);
    MEDIA_SYS_ReleaseFrame("DUALVIEW", 0, "output0", out);
    if (r)
        fprintf(stderr, "write failed: %s\n", output);
    else
        printf("DUALVIEW mode=%d %ux%u bytes=%zu -> %s\n", mode,
               meta.u.video.visible_width, meta.u.video.visible_height, n, output);

done:
    /* 逆序拆解：停模块 -> 销毁模块 -> 销毁 Pool -> 退出系统。
     * POOL_MASK 从未创建过，但 Destroy 一个不存在的 Pool 是安全的空操作。 */
    if (started)
        MEDIA_PLUGIN_Stop("DUALVIEW", 0);
    if (created)
        MEDIA_PLUGIN_DestroyModule("DUALVIEW", 0);
    MEDIA_POOL_Destroy(POOL_MASK);
    MEDIA_POOL_Destroy(POOL_OUT);
    MEDIA_POOL_Destroy(POOL_B);
    MEDIA_POOL_Destroy(POOL_A);
    MEDIA_SYS_Exit();
    return r ? 1 : 0;
}
