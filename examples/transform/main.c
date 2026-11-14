/*
 * TRANSFORM 入门教程：用查找表（LUT）把 640x480 重映射成 320x240
 *
 * TRANSFORM 不做固定算法，它按你给的表决定“输出第 (x,y) 个像素去输入哪里取样”。
 * 本程序的表是等比例的（输出 4 个像素对应输入 2 个像素），所以效果等于缩放；
 * 想理解 LUT 的形状，可以把它改成只搬左上角四分之一，看输出变化。
 *
 * 链路：TPG（渐变图）-> TRANSFORM -> 取帧存文件。
 *
 * 阅读顺序：
 *   [1] 两个 layout + 两个 Pool -> [2] 生成 LUT -> [3] CreateGrp
 *   -> [4] 创建 TPG -> [5] Bind -> [6] Start/Enable -> [7] GetFrame
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_transform.h"
#include "media_tpg.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define IN_W 640
#define IN_H 480
#define OUT_W 320
#define OUT_H 240
#define SRC_POOL 0   /* TPG 写源图用 */
#define OUT_POOL 1   /* TRANSFORM 写结果用 */

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
    const char *out = argc > 1 ? argv[1] : "transform_out.nv12";
    MEDIA_VIDEO_LAYOUT li, lo;
    size_t si, so;
    float *lut;
    MEDIA_TRANSFORM_ATTR a;
    MEDIA_TPG_ATTR t;
    MEDIA_BUFFER b;
    int tpg = 0, tr = 0, run = 0, rc = 1;

    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [1] 输入输出尺寸不同，各算一个 layout；两个 Pool 都要 NATIVE。 */
    if (MEDIA_VIDEO_BuildLinearLayout(IN_W, IN_H, MEDIA_FORMAT_NV12, &li, &si) != 0 ||
        MEDIA_VIDEO_BuildLinearLayout(OUT_W, OUT_H, MEDIA_FORMAT_NV12, &lo, &so) != 0 ||
        MEDIA_POOL_CreateWithMode(SRC_POOL, si, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, so, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        goto done;
    }

    /* [2] 生成 LUT：每个输出像素占两个 float（源 x、源 y）。
     *     这里用等比例映射，所以等价于缩放。表放在堆上，用完要 free。 */
    lut = calloc((size_t)OUT_W * OUT_H * 2, sizeof(*lut));
    if (!lut)
        goto cleanup;
    for (int y = 0; y < OUT_H; y++) {
        for (int x = 0; x < OUT_W; x++) {
            lut[2 * ((size_t)y * OUT_W + x)] = (float)x * IN_W / OUT_W;
            lut[2 * ((size_t)y * OUT_W + x) + 1] = (float)y * IN_H / OUT_H;
        }
    }

    /* [3] 创建 transform 组。lut_size 是字节数，必须是元素个数 x sizeof(float)。 */
    memset(&a, 0, sizeof(a));
    a.in_width = IN_W;
    a.in_height = IN_H;
    a.input_format = MEDIA_FORMAT_NV12;
    a.in_stride = li.pitch_bytes[0];
    a.out_width = OUT_W;
    a.out_height = OUT_H;
    a.output_format = MEDIA_FORMAT_NV12;
    a.out_stride = lo.pitch_bytes[0];
    a.input_depth = 4;
    a.pool_id = OUT_POOL;
    a.lut_width = OUT_W;
    a.lut_height = OUT_H;
    a.lut = lut;
    a.lut_size = (size_t)OUT_W * OUT_H * 2 * sizeof(float);
    /* 表的所有权始终在本程序手里：模块只读取，退出前由下面 cleanup 里的 free(lut) 释放。 */
    if (MEDIA_TRANSFORM_CreateGrp(0, &a) != 0)
        goto cleanup;
    tr = 1;

    /* [4] 渐变图最容易看出重映射是否正确（位置错了渐变方向会变）。 */
    memset(&t, 0, sizeof(t));
    t.width = IN_W;
    t.height = IN_H;
    t.format = MEDIA_FORMAT_NV12;
    t.fps = 30;
    t.pool_id = SRC_POOL;
    t.pattern = MEDIA_TPG_PATTERN_GRADIENT;
    if (MEDIA_TPG_CreateChn(0, &t) != 0)
        goto cleanup;

    /* [5] 连线：TPG.output0 -> TRANSFORM.input0。 */
    if (MEDIA_SYS_Bind("TPG", 0, "output0", "TRANSFORM", 0, "input0") != 0)
        goto cleanup;

    /* [6] 先启下游 TRANSFORM，再启上游 TPG。 */
    if (MEDIA_TRANSFORM_Start(0) != 0 || MEDIA_TPG_Enable(0) != 0)
        goto cleanup;
    tpg = 1;
    run = 1;

    /* [7] 取一帧并保存。 */
    if (MEDIA_TRANSFORM_GetFrame(0, &b, 2000) != 0 || dump(out, b) != 0) {
        fprintf(stderr, "TRANSFORM output unavailable\n");
        goto cleanup;
    }
    MEDIA_TRANSFORM_ReleaseFrame(0, b);

    printf("TRANSFORM identity LUT: %dx%d -> %dx%d -> %s\n", IN_W, IN_H, OUT_W, OUT_H, out);
    rc = 0;

cleanup:
    if (run) {
        MEDIA_TPG_Disable(0);
        MEDIA_TRANSFORM_Stop(0);
        MEDIA_SYS_UnBind("TPG", 0, "output0", "TRANSFORM", 0, "input0");
    }
    if (tpg)
        MEDIA_TPG_DestroyChn(0);
    if (tr)
        MEDIA_TRANSFORM_DestroyGrp(0);
    free(lut);
    MEDIA_POOL_Destroy(OUT_POOL);
    MEDIA_POOL_Destroy(SRC_POOL);

done:
    MEDIA_SYS_Exit();
    return rc;
}
