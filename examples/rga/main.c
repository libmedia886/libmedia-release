/*
 * RGA 入门教程：用 RGA 硬件把 640x480 缩放到 320x240
 *
 * 本程序不读摄像头：TPG 生成棋盘图 -> Bind 到 RGA -> RGA 缩放 -> 取帧存文件。
 * 这条链路是“两个模块用 Bind 连起来”的最小完整例子，学会后可以把 TPG
 * 换成 VI、把 RGA 换成别的 FILTER 模块。
 *
 * 阅读顺序：
 *   [1] 建 layout + Pool -> [2] 描述 RGA 的输入/输出端口 -> [3] CreateGrp
 *   -> [4] TPG 送源 -> [5] Bind 连线 -> [6] Start/Enable -> [7] GetFrame
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_rga.h"
#include "media_tpg.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define IW 640          /* 输入宽 */
#define IH 480          /* 输入高 */
#define OW 320          /* 输出宽（正好一半） */
#define OH 240          /* 输出高 */
#define SRC_POOL 0      /* TPG 写源图用 */
#define OUT_POOL 1      /* RGA 写结果用 */

/* 把结果 Buffer 原样写成裸文件。返回 0 表示成功。 */
static int dump(const char *path, MEDIA_BUFFER b)
{
    FILE *f = fopen(path, "wb");
    void *p = MEDIA_POOL_GetVaddr(b);
    size_t n = MEDIA_POOL_GetSize(b);
    int r = -1;

    if (f && p && MEDIA_POOL_BeginCpuAccess(b, DMA_BUF_SYNC_READ) == 0) {
        r = fwrite(p, 1, n, f) == n ? 0 : -1;
        MEDIA_POOL_EndCpuAccess(b, DMA_BUF_SYNC_READ);
    }
    if (f)
        fclose(f);
    return r;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "rga_out.nv12";
    MEDIA_VIDEO_LAYOUT li, lo;
    size_t si, so;
    MEDIA_RGA_GRP_ATTR a;
    MEDIA_RGA_PORT_ATTR in, outp;
    MEDIA_TPG_ATTR t;
    MEDIA_BUFFER b;
    int tpg = 0, rga = 0, run = 0, rc = 1;

    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [1] 输入输出各算一次 layout（两个尺寸不同），再各自建 NATIVE Pool。
     *     RGA 直接读写 DMA-BUF，所以两边都必须是 NATIVE。 */
    if (MEDIA_VIDEO_BuildLinearLayout(IW, IH, MEDIA_FORMAT_NV12, &li, &si) != 0 ||
        MEDIA_VIDEO_BuildLinearLayout(OW, OH, MEDIA_FORMAT_NV12, &lo, &so) != 0 ||
        MEDIA_POOL_CreateWithMode(SRC_POOL, si, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, so, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        goto done;
    }

    /* [2] 端口描述：每个端口要说清自己的尺寸、stride、格式和队列深度。
     *     输入端口不给 pool_id（数据来自上游 TPG）；输出端口必须给，
     *     因为没人给它送帧，它要自己从 OUT_POOL 拿空 buffer 写。 */
    memset(&in, 0, sizeof(in));
    in.port_id = 0;
    in.width = IW;
    in.height = IH;
    in.stride = li.pitch_bytes[0];
    in.format = MEDIA_FORMAT_NV12;
    in.queue_depth = 4;

    memset(&outp, 0, sizeof(outp));
    outp.port_id = 0;
    outp.width = OW;
    outp.height = OH;
    outp.stride = lo.pitch_bytes[0];
    outp.format = MEDIA_FORMAT_NV12;
    outp.pool_id = OUT_POOL;
    outp.queue_depth = 4;

    /* [3] 用“组”的形式创建：一个组可以带多输入多输出，
     *     algo 决定这一组做什么。这里是 RESIZE。 */
    memset(&a, 0, sizeof(a));
    a.algo = MEDIA_RGA_ALG_RESIZE;
    a.input_count = 1;
    a.output_count = 1;
    a.input_depth = 4;
    a.output_depth = 4;
    a.inputs[0] = in;
    a.outputs[0] = outp;
    if (MEDIA_RGA_CreateGrp(0, &a) != 0) {
        fprintf(stderr, "RGA create failed\n");
        goto cleanup;
    }
    rga = 1;

    /* [4] 创建 TPG（测试图案发生器）作为源：棋盘图最容易看出缩放是否正确。 */
    memset(&t, 0, sizeof(t));
    t.width = IW;
    t.height = IH;
    t.format = MEDIA_FORMAT_NV12;
    t.fps = 30;
    t.pool_id = SRC_POOL;
    t.pattern = MEDIA_TPG_PATTERN_CHECKERBOARD;
    if (MEDIA_TPG_CreateChn(0, &t) != 0)
        goto cleanup;

    /* [5] Bind 把 TPG 的 output0 接到 RGA 的 input0。
     *     端口名写错会直接返回错误——不要凭感觉猜名字。 */
    if (MEDIA_SYS_Bind("TPG", 0, "output0", "RGA", 0, "input0") != 0)
        goto cleanup;

    /* [6] 先启动下游再启动上游：数据是从生产端往消费端流的。 */
    if (MEDIA_RGA_Start(0) != 0)
        goto cleanup;
    if (MEDIA_TPG_Enable(0) != 0)
        goto cleanup;
    tpg = 1;
    run = 1;

    /* [7] 取一帧结果并保存。拿到就必须 Release，否则输出 Pool 会被占满。 */
    if (MEDIA_RGA_GetFrame(0, &b, 2000) != 0 || dump(out, b) != 0) {
        fprintf(stderr, "RGA output unavailable\n");
        goto cleanup;
    }
    MEDIA_RGA_ReleaseFrame(0, b);

    printf("RGA RESIZE NV12: %dx%d -> %dx%d -> %s\n", IW, IH, OW, OH, out);
    rc = 0;

cleanup:
    /* 逆序拆解：先断流（禁上游、停下游、解绑），再销毁，最后销毁 Pool。 */
    if (run) {
        MEDIA_TPG_Disable(0);
        MEDIA_RGA_Stop(0);
        MEDIA_SYS_UnBind("TPG", 0, "output0", "RGA", 0, "input0");
    }
    if (tpg)
        MEDIA_TPG_DestroyChn(0);
    if (rga)
        MEDIA_RGA_DestroyChn(0);
    MEDIA_POOL_Destroy(OUT_POOL);
    MEDIA_POOL_Destroy(SRC_POOL);

done:
    MEDIA_SYS_Exit();
    return rc;
}
