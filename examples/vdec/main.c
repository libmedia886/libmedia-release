/*
 * VDEC 入门教程：把 H.264 裸流解码成 NV12 图像
 *
 * 输入是本仓库 examples/venc 生成的 venc_out.h264。先跑 venc 再跑本程序，
 * 否则会提示找不到文件。
 *
 * 与编码相反，解码的输出 Pool 大小不能自己拍脑袋：要先问
 * MEDIA_VDEC_QueryOutputPoolRequirements 要 slot_size / slot_count，
 * 按模块给的值建 Pool，否则解码器可能写不下。
 *
 * 阅读顺序：
 *   [1] 读整个码流文件 -> [2] 查询输出 Pool 需求并建 Pool -> [3] CreateChn
 *   -> [4] SendPacket -> [5] GetFrame / 保存 / Release
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_vdec.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 320
#define H 240
#define OUT_POOL 0          /* 解码结果 Pool */
#define MAX_FILE (16 * 1024 * 1024)  /* 码流文件上限，防止误读大文件 */

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
    const char *inpath = argc > 1 ? argv[1] : "venc_out.h264";
    const char *out = argc > 2 ? argv[2] : "vdec_out.nv12";
    FILE *f = fopen(inpath, "rb");
    long n;
    unsigned char *data;
    MEDIA_VDEC_ATTR a;
    MEDIA_VDEC_OUTPUT_POOL_REQUIREMENTS req;
    MEDIA_VDEC_FRAME fr;
    int made = 0, run = 0, rc = 1;

    /* [1] 一次性把整个码流读进内存。解码器接收的是一整段 access unit，
     *     所以小文件这样最简单；真实播放器会边读边送。 */
    if (!f) {
        fprintf(stderr, "open %s failed; run examples/venc first\n", inpath);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > MAX_FILE || (data = malloc((size_t)n)) == NULL ||
        fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(data);
        return 1;
    }
    fclose(f);

    if (MEDIA_SYS_Init() != 0)
        goto done;

    memset(&a, 0, sizeof(a));
    a.width = W;
    a.height = H;
    a.stride = W;
    a.buf_cnt = 8;
    a.video_type = MEDIA_VIDEO_H264;
    a.output_format = 0;        /* 0 = 用模块默认输出格式（NV12） */
    a.output_compress = MEDIA_VIDEO_COMPRESS_NONE;
    a.has_input_port = 0;       /* 本例子直接 SendPacket，不走输入端口 */

    /* [2] 关键一步：先问模块要多大、要几个 slot，再照建。
     *     自己拍一个“够大”的数在这里是错的常见做法。 */
    if (MEDIA_VDEC_QueryOutputPoolRequirements(&a, &req) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, req.slot_size, req.slot_count,
                                  MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        fprintf(stderr, "VDEC pool requirement/setup failed\n");
        goto cleanup;
    }
    a.pool_id = OUT_POOL;

    /* [3] 创建解码通道并启动。 */
    if (MEDIA_VDEC_CreateChn(0, &a) != 0) {
        fprintf(stderr, "VDEC create failed (MPP/device unavailable?)\n");
        goto cleanup;
    }
    made = 1;

    if (MEDIA_VDEC_Start(0) != 0)
        goto cleanup;
    run = 1;

    /* [4] 送整段码流。最后一个参数 0 表示没有额外时间戳。 */
    if (MEDIA_VDEC_SendPacket(0, data, (size_t)n, 0) != 0) {
        fprintf(stderr, "VDEC send failed\n");
        goto cleanup;
    }

    /* [5] 取解码结果。第一帧可能是 SPS/PPS 之后的首帧，也可能需要
     *     重试一次才有输出，所以这里给两次机会、每次等 5 秒。 */
    for (int attempt = 0; attempt < 2; attempt++) {
        if (MEDIA_VDEC_GetFrame(0, &fr, 5000) == 0) {
            if (dump(out, fr.buffer) != 0) {
                MEDIA_VDEC_ReleaseFrame(0, fr);
                goto cleanup;
            }
            MEDIA_VDEC_ReleaseFrame(0, fr);
            printf("VDEC H.264 packet=%ld -> NV12 %dx%d %s\n", n, W, H, out);
            rc = 0;
            break;
        }
    }
    if (rc != 0)
        fprintf(stderr, "VDEC decode timeout; input may need a complete access unit\n");

cleanup:
    if (run)
        MEDIA_VDEC_Stop(0);
    if (made)
        MEDIA_VDEC_DestroyChn(0);
    MEDIA_POOL_Destroy(OUT_POOL);

done:
    MEDIA_SYS_Exit();
    free(data);
    return rc;
}
