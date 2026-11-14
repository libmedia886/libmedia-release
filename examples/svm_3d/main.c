/*
 * SVM3D 入门教程：两路 NV12 做双目视差（深度）计算
 *
 * 本程序不接双目相机，自己在内存里造两张“近似同一场景、水平错位一点”的图，
 * 送进 SVM3D，输出一张 ABGR8888 的视差图。
 *
 * 链路：内存造两帧 -> SVM3D（2 输入 1 输出）-> GetFrame -> 裸文件。
 *
 * 阅读顺序：
 *   [1] 加载插件 -> [2] 两个 layout + 三个 Pool -> [3] 造两帧
 *   -> [4] CreateModule -> [5] SendFrame x2 -> [6] GetFrame / 保存
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_svm_3d.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 640           /* 输入宽 */
#define H 480           /* 输入高 */
#define OUT_W 1280      /* 输出宽（本模块输出尺寸独立于输入） */
#define OUT_H 720       /* 输出高 */
#define POOL_A 27       /* A 路输入 Pool */
#define POOL_B 28       /* B 路输入 Pool */
#define POOL_OUT 29     /* 输出 Pool */

/*
 * 造一帧 NV12 并设好 metadata。
 * k=0：亮度从左到右变化；k=1：从上到下变化。
 * 两者相当于“同一场景的左右目视图”的简化版，足够让模块跑通流程。
 */
static int frame(int pool, const MEDIA_VIDEO_LAYOUT *l, size_t n, int k, MEDIA_BUFFER *b)
{
    MEDIA_BUFFER_META m;
    unsigned char *p;

    if (MEDIA_POOL_GetBuffer(pool, b))
        return -1;
    p = MEDIA_POOL_GetVaddr(*b);
    if (!p || MEDIA_POOL_BeginCpuAccess(*b, DMA_BUF_SYNC_WRITE))
        goto bad;

    memset(p, 0, n);
    /* Y 平面 */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            p[(size_t)y * l->pitch_bytes[0] + x] =
                (unsigned char)(k ? 50 + y * 180 / H : 50 + x * 180 / W);
        }
    }
    /* UV 平面：交错 U,V，按 x += 2 走 */
    for (int y = 0; y < H / 2; y++) {
        for (int x = 0; x < W; x += 2) {
            unsigned char *q = p + l->offset_bytes[1] + (size_t)y * l->pitch_bytes[1] + x;

            q[0] = 90;
            q[1] = 180;
        }
    }
    MEDIA_POOL_EndCpuAccess(*b, DMA_BUF_SYNC_WRITE);

    memset(&m, 0, sizeof(m));
    m.valid = 1;
    m.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    m.media_format = MEDIA_FORMAT_NV12;
    m.len = n;
    m.u.video = *l;
    m.time_stamp = 1000000;
    m.seq = 1;
    if (MEDIA_BUFFER_SetMeta(*b, &m) == 0)
        return 0;

bad:
    MEDIA_POOL_PutBuffer(*b);
    return -1;
}

int main(int argc, char **argv)
{
    /* 参数：输出路径、插件路径、标定资产路径（可省略） */
    const char *outp = argc > 1 ? argv[1] : "svm_3d_frame.rgba";
    const char *plugin = argc > 2 ? argv[2] : "../../plugins/libmedia_svm_3d.so";
    const char *asset = argc > 3 ? argv[3] : NULL;
    int r = 1, created = 0, started = 0;
    MEDIA_VIDEO_LAYOUT in, outl;
    size_t ni = 0, no = 0;
    MEDIA_BUFFER a, b, o;
    MEDIA_BUFFER_META m;
    FILE *f;

    /* [1] 初始化并加载插件。SVM3D 的插件名和模块名不同：插件文件名是
     *     libmedia_svm_3d.so，模块类型串是 "SVM3D"。 */
    if (MEDIA_SYS_Init() != 0) {
        fprintf(stderr, "MEDIA_SYS_Init failed\n");
        return 1;
    }
    {
        int loaded = MEDIA_PLUGIN_Load(plugin);

        if (loaded < 0) {
            fprintf(stderr, "load %s failed: %d\n", plugin, loaded);
            r = loaded;
            goto done;
        }
    }

    /* [2] 输入 NV12 640x480，输出 ABGR8888 1280x720 —— 两个尺寸不同，
     *     所以建三个 Pool 时分别用各自的字节数。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_NV12, &in, &ni) != 0 ||
        MEDIA_VIDEO_BuildLinearLayout(OUT_W, OUT_H, MEDIA_FORMAT_ABGR8888, &outl, &no) != 0) {
        fprintf(stderr, "SVM3D layout setup failed\n");
        goto done;
    }
    if (MEDIA_POOL_CreateWithMode(POOL_A, ni, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) ||
        MEDIA_POOL_CreateWithMode(POOL_B, ni, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) ||
        MEDIA_POOL_CreateWithMode(POOL_OUT, no, 4, MEDIA_POOL_ALLOC_MODE_NATIVE)) {
        fprintf(stderr, "SVM3D pool setup failed\n");
        goto done;
    }

    /* [3] 造两帧“左右目”图。 */
    if (frame(POOL_A, &in, ni, 0, &a) || frame(POOL_B, &in, ni, 1, &b)) {
        fprintf(stderr, "SVM3D failed to build input frames\n");
        goto done;
    }

    /* [4] 创建模块。input_count=2 是双目的前提；
     *     sync_wait_inputs=1 表示必须两路都到齐才开始计算，
     *     sync_timeout_ms=100 是等待另一路的宽限时间。 */
    {
        MEDIA_SVM3D_ATTR x;

        memset(&x, 0, sizeof(x));
        x.asset_path = asset;
        x.input_count = 2;
        x.input_format = MEDIA_FORMAT_NV12;
        x.in_width = W;
        x.in_height = H;
        x.in_stride = in.pitch_bytes[0];
        x.output_format = MEDIA_FORMAT_ABGR8888;
        x.out_width = OUT_W;
        x.out_height = OUT_H;
        x.out_stride = outl.pitch_bytes[0];
        x.output_pool_id = POOL_OUT;
        x.input_depth = 4;
        x.sync_timeout_ms = 100;
        x.sync_wait_inputs = 1;
        r = MEDIA_PLUGIN_CreateModule("SVM3D", 0, &x, sizeof(x));
        if (r) {
            /* 没有 mesh 标定资产时这里会失败，这是设计使然：
             * 错误的 mesh 会把画面投到错误位置，比直接报错更危险。 */
            fprintf(stderr,
                    "SVM3D create failed: %d (asset_path=%s; a calibrated mesh asset is "
                    "required)\n",
                    r, asset ? asset : "(null)");
            goto done;
        }
        created = 1;
    }

    r = MEDIA_PLUGIN_Start("SVM3D", 0);
    if (r) {
        fprintf(stderr, "SVM3D start failed: %d\n", r);
        goto done;
    }
    started = 1;

    /* [5] 两路分别送到输入 0 和输入 1。注意顺序：
     *     0 号通常是左目，1 号是右目，接反了视差方向会相反。 */
    if (MEDIA_SVM3D_SendFrame(0, 0, a, 1000) || MEDIA_SVM3D_SendFrame(0, 1, b, 1000)) {
        fprintf(stderr, "SVM3D send frame failed\n");
        goto done;
    }

    /* [6] 取视差图。这里用模块专用 GetFrame，等待时间给足 5 秒。 */
    r = MEDIA_SVM3D_GetFrame(0, &o, 5000);
    if (r) {
        fprintf(stderr, "SVM3D get frame failed: %d\n", r);
        goto done;
    }

    if (MEDIA_BUFFER_GetMeta(o, &m) != 0 ||
        MEDIA_POOL_BeginCpuAccess(o, DMA_BUF_SYNC_READ) != 0) {
        MEDIA_SVM3D_ReleaseFrame(0, o);
        goto done;
    }
    {
        void *p = MEDIA_POOL_GetVaddr(o);
        size_t n = m.len < MEDIA_POOL_GetSize(o) ? m.len : MEDIA_POOL_GetSize(o);

        f = fopen(outp, "wb");
        r = (f && p && fwrite(p, 1, n, f) == n) ? 0 : 1;
        if (f)
            fclose(f);
        MEDIA_POOL_EndCpuAccess(o, DMA_BUF_SYNC_READ);
        MEDIA_SVM3D_ReleaseFrame(0, o);
        if (!r)
            printf("SVM3D %ux%u ABGR bytes=%zu -> %s\n", m.u.video.visible_width,
                   m.u.video.visible_height, n, outp);
    }

done:
    if (started)
        MEDIA_PLUGIN_Stop("SVM3D", 0);
    if (created)
        MEDIA_PLUGIN_DestroyModule("SVM3D", 0);
    MEDIA_POOL_Destroy(POOL_OUT);
    MEDIA_POOL_Destroy(POOL_B);
    MEDIA_POOL_Destroy(POOL_A);
    MEDIA_SYS_Exit();
    return r ? 1 : 0;
}
