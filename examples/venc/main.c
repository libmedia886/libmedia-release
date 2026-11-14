/*
 * VENC 入门教程：把 4 帧 NV12 编码成 H.264 裸流
 *
 * 本程序不读摄像头，自己生成 4 帧逐步变亮的纯色图（Y 平面 16/24/32/40，
 * UV 固定 128 即“无色”），编码后把码流按收到的顺序直接追加进一个裸文件。
 *
 * 阅读顺序：
 *   [1] BuildLinearLayout -> [2] 建输入/输出 Pool -> [3] CreateChn -> Start
 *   -> [4] 逐帧填 NV12 并 Send -> [5] GetPacket / 写文件 / ReleasePacket
 *   -> [6] 逆序拆解
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_venc.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 320        /* 可见宽度 */
#define H 240        /* 可见高度 */
#define STRIDE 320   /* 每行字节数；这里等于宽度，没有额外对齐填充 */
#define IN_POOL 0    /* 输入图像 Pool */
#define OUT_POOL 1   /* 输出码流 Pool（HOST 内存即可，不需要给 GPU 用） */
#define FRAMES 4     /* 一共编几帧 */

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "venc_out.h264";
    MEDIA_VIDEO_LAYOUT l;
    size_t sz;
    MEDIA_BUFFER_META m;
    MEDIA_PACKET pkt;
    FILE *fout = NULL;
    int made = 0, run = 0, rc = 1, packets = 0;
    size_t total = 0;

    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [1][2] 先用 layout 算出 NV12 一帧多少字节，再建两个 Pool。
     *        输入图像必须是 NATIVE（硬件编码器要直接读 DMA-BUF）；
     *        输出码流走 CPU，用 HOST 分配就够，8 个 slot 防止码流堆积时丢包。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_NV12, &l, &sz) != 0 ||
        MEDIA_POOL_CreateWithMode(IN_POOL, sz, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, 256 * 1024, 8, MEDIA_POOL_ALLOC_MODE_HOST) != 0) {
        goto done;
    }

    /* [3] CreateChn 的参数依次是：通道号、宽、高、stride、帧率、
     *     输入/输出队列深度、码流 Pool、码率(bps)、GOP、编码格式、压缩模式。
     *     这里 4 Mbps 码率、GOP 30。 */
    if (MEDIA_VENC_CreateChn(0, W, H, STRIDE, 30, 4, 4, OUT_POOL, 4000000, 30,
                             MEDIA_FORMAT_H264, MEDIA_VIDEO_COMPRESS_NONE) != 0) {
        fprintf(stderr, "VENC create failed (MPP/device unavailable?)\n");
        goto cleanup;
    }
    made = 1;

    if (MEDIA_VENC_Start(0) != 0)
        goto cleanup;
    run = 1;

    fout = fopen(out, "wb");
    if (!fout)
        goto cleanup;

    for (int frame = 0; frame < FRAMES; frame++) {
        MEDIA_BUFFER in;

        /* [4] 取一个输入 buffer，填 NV12：
         *     Y 平面 W*H 字节逐帧变亮，UV 平面紧跟其后，长度是 Y 的一半。 */
        if (MEDIA_POOL_GetBuffer(IN_POOL, &in) != 0)
            goto cleanup;
        {
            uint8_t *p = MEDIA_POOL_GetVaddr(in);

            if (!p || MEDIA_POOL_BeginCpuAccess(in, DMA_BUF_SYNC_WRITE) != 0) {
                MEDIA_POOL_PutBuffer(in);
                goto cleanup;
            }
            memset(p, 16 + frame * 8, STRIDE * H);
            memset(p + STRIDE * H, 128, STRIDE * H / 2);
            MEDIA_POOL_EndCpuAccess(in, DMA_BUF_SYNC_WRITE);
        }

        memset(&m, 0, sizeof(m));
        m.valid = 1;
        m.type = MEDIA_BUFFER_META_VIDEO_FRAME;
        m.media_format = MEDIA_FORMAT_NV12;
        m.len = sz;
        m.u.video = l;

        /* 送帧成功后所有权归模块，不能再 PutBuffer。 */
        if (MEDIA_BUFFER_SetMeta(in, &m) != 0 ||
            MEDIA_SYS_SendFrame("VENC", 0, "input0", in, 1000) != 0) {
            MEDIA_POOL_PutBuffer(in);
            goto cleanup;
        }

        /* [5] 取码流。编码器可能一帧输出多个 packet，也可能暂时不给；
         *     这里每帧等一个，超时就继续下一帧，最后用 total 判断有没有产出。 */
        if (MEDIA_VENC_GetPacket(0, &pkt, 3000) == 0) {
            if (pkt.len && fwrite(pkt.data, 1, pkt.len, fout) == pkt.len) {
                total += pkt.len;
                packets++;
            }
            MEDIA_VENC_ReleasePacket(0, &pkt);
        }
    }

    fclose(fout);
    fout = NULL;

    if (!total) {
        fprintf(stderr, "VENC produced no non-empty packet\n");
        goto cleanup;
    }
    printf("VENC H.264 NV12 %dx%d packets=%d bytes=%zu -> %s\n", W, H, packets, total, out);
    rc = 0;

cleanup:
    /* [6] 逆序拆解：文件 -> 通道 -> Pool */
    if (fout)
        fclose(fout);
    if (run)
        MEDIA_VENC_Stop(0);
    if (made)
        MEDIA_VENC_DestroyChn(0);
    MEDIA_POOL_Destroy(OUT_POOL);
    MEDIA_POOL_Destroy(IN_POOL);

done:
    MEDIA_SYS_Exit();
    return rc;
}
