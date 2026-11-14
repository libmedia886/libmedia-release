/*
 * PANO 入门教程：两路 NV12 输入拼成一张全景图
 *
 * 本程序不读摄像头，自己在内存里造两张 640x480 的 NV12：
 *   PA 路：从左到右变亮（模拟相机 A）
 *   PB 路：从上到下变亮（模拟相机 B）
 * 两路送进 PANO 模块，取一帧拼接结果写成裸文件。
 *
 * 阅读顺序：
 *   [1] 加载插件 -> [2] 建 layout + 三个 Pool -> [3] CreateModule
 *   -> [4] 造两帧并 Send -> [5] GetFrame / 保存 / Release -> [6] 逆序拆解
 */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_panorama.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 640       /* 输入宽 */
#define H 480       /* 输入高 */
#define PA 27       /* A 路 Pool 编号 */
#define PB 28       /* B 路 Pool 编号 */
#define PO 29       /* 输出 Pool 编号 */

/*
 * 造一帧 NV12 并发给调用者。
 * pool：从哪个 Pool 取 buffer；l/n：layout 和字节数；
 * k=0 生成水平渐变，k=1 生成垂直渐变；b：取到的 buffer 写回这里。
 * 返回 0 表示 buffer 已经填好并且 metadata 设置成功。
 */
static int mk(int pool, const MEDIA_VIDEO_LAYOUT *l, size_t n, int k, MEDIA_BUFFER *b)
{
    MEDIA_BUFFER_META m;
    unsigned char *p;

    if (MEDIA_POOL_GetBuffer(pool, b))
        return -1;
    p = MEDIA_POOL_GetVaddr(*b);
    if (!p || MEDIA_POOL_BeginCpuAccess(*b, DMA_BUF_SYNC_WRITE))
        goto bad;

    /* Y 平面：渐变，亮度范围 60..220 */
    memset(p, 0, n);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            p[(size_t)y * l->pitch_bytes[0] + x] =
                (unsigned char)(k ? 60 + y * 160 / H : 60 + x * 160 / W);
        }
    }
    /* UV 平面：固定淡蓝色。NV12 的 UV 是交错的 U,V,U,V…，
     * 所以按 x += 2 走，q[0]=U、q[1]=V。 */
    for (int y = 0; y < H / 2; y++) {
        for (int x = 0; x < W; x += 2) {
            unsigned char *q = p + l->offset_bytes[1] + (size_t)y * l->pitch_bytes[1] + x;

            q[0] = 100;
            q[1] = 160;
        }
    }
    MEDIA_POOL_EndCpuAccess(*b, DMA_BUF_SYNC_WRITE);

    /* 关掉 CPU 访问之后再交出 buffer：库只认 metadata，不猜测内容。 */
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
    /* 参数：输出路径、插件路径、PTO 标定文件路径（可省略） */
    const char *outp = argc > 1 ? argv[1] : "panorama_frame.nv12";
    const char *plugin = argc > 2 ? argv[2] : "../../plugins/libmedia_panorama.so";
    const char *pto = argc > 3 ? argv[3] : NULL;
    int r = 1, cr = 0, st = 0;
    MEDIA_VIDEO_LAYOUT l;
    size_t n = 0;
    MEDIA_BUFFER a, b, o;
    MEDIA_BUFFER_META m;
    FILE *f;

    /* [1] 初始化系统并加载插件。插件和它的 .sig 必须成对存在，
     *     所以这里显式给路径而不是依赖 LD_LIBRARY_PATH。 */
    if (MEDIA_SYS_Init() != 0) {
        fprintf(stderr, "MEDIA_SYS_Init failed\n");
        return 1;
    }
    r = MEDIA_PLUGIN_Load(plugin);
    if (r < 0) {
        fprintf(stderr, "load %s failed: %d\n", plugin, r);
        goto done;
    }
    /* 加载成功时返回的是插件槽位号（>=0），不能直接当退出码用。
     * 后面每一步都会重新写 r，只有全部走通才会被置为 0。 */
    r = 1;

    /* [2] 三路同尺寸，所以只需要一个 layout。 */
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_NV12, &l, &n) != 0) {
        fprintf(stderr, "PANO layout setup failed\n");
        goto done;
    }
    if (MEDIA_POOL_CreateWithMode(PA, n, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) ||
        MEDIA_POOL_CreateWithMode(PB, n, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) ||
        MEDIA_POOL_CreateWithMode(PO, n, 4, MEDIA_POOL_ALLOC_MODE_NATIVE)) {
        fprintf(stderr, "PANO pool setup failed\n");
        goto done;
    }

    /* [3] 创建 PANO 实例。注意这里是通用的 MEDIA_PLUGIN_CreateModule，
     *     但送帧/取帧走模块专用 API（MEDIA_PANO_SendFrame/GetFrame）。
     *     sync_wait_inputs=1 表示要等两路都到齐才合成。 */
    {
        MEDIA_PANO_ATTR x;

        memset(&x, 0, sizeof(x));
        x.pto_path = pto;
        x.input_count = 2;
        x.in_width = W;
        x.in_height = H;
        x.out_width = W;
        x.out_height = H;
        x.in_stride = l.pitch_bytes[0];
        x.out_stride = l.pitch_bytes[0];
        x.output_pool_id = PO;
        x.input_depth = 4;
        x.sync_timeout_ms = 100;
        x.sync_wait_inputs = 1;
        x.output_compress = MEDIA_VIDEO_COMPRESS_NONE;
        r = MEDIA_PLUGIN_CreateModule("PANO", 0, &x, sizeof(x));
        if (r) {
            /* 没有 PTO 标定文件时这里会失败，这是设计使然：
             * 该模块需要真实标定才能决定每个像素来自哪路相机。 */
            fprintf(stderr,
                    "PANO create failed: %d (pto_path=%s; a calibrated .pto file is "
                    "required)\n",
                    r, pto ? pto : "(null)");
            goto done;
        }
        cr = 1;
    }

    r = MEDIA_PLUGIN_Start("PANO", 0);
    if (r) {
        fprintf(stderr, "PANO start failed: %d\n", r);
        goto done;
    }
    st = 1;

    /* [4] 造两帧，分别送到输入 0 和输入 1。 */
    if (mk(PA, &l, n, 0, &a) || mk(PB, &l, n, 1, &b)) {
        fprintf(stderr, "PANO failed to build input frames\n");
        goto done;
    }
    if (MEDIA_PANO_SendFrame(0, 0, a, 1000) || MEDIA_PANO_SendFrame(0, 1, b, 1000)) {
        fprintf(stderr, "PANO send frame failed\n");
        goto done;
    }

    /* [5] 等合成结果（5 秒）。两路不同时到达会超时，这是最常见的失败原因。 */
    r = MEDIA_PANO_GetFrame(0, &o, 5000);
    if (r) {
        fprintf(stderr, "PANO get frame failed: %d\n", r);
        goto done;
    }

    if (MEDIA_BUFFER_GetMeta(o, &m) || MEDIA_POOL_BeginCpuAccess(o, DMA_BUF_SYNC_READ)) {
        MEDIA_PANO_ReleaseFrame(0, o);
        goto done;
    }
    {
        void *p = MEDIA_POOL_GetVaddr(o);
        /* m.len 是有效字节数，可能比 Pool 容量小（有对齐填充）。 */
        size_t z = m.len < MEDIA_POOL_GetSize(o) ? m.len : MEDIA_POOL_GetSize(o);

        f = fopen(outp, "wb");
        r = (f && p && fwrite(p, 1, z, f) == z) ? 0 : 1;
        if (f)
            fclose(f);
        MEDIA_POOL_EndCpuAccess(o, DMA_BUF_SYNC_READ);
        MEDIA_PANO_ReleaseFrame(0, o);
        if (!r)
            printf("PANO %ux%u bytes=%zu -> %s\n", m.u.video.visible_width,
                   m.u.video.visible_height, z, outp);
    }

done:
    /* [6] 逆序拆解。注意退出路径上也要释放已经 Get 到的帧，
     *     这里所有 Get 都在 done 之前配对了 Release。 */
    if (st)
        MEDIA_PLUGIN_Stop("PANO", 0);
    if (cr)
        MEDIA_PLUGIN_DestroyModule("PANO", 0);
    MEDIA_POOL_Destroy(PO);
    MEDIA_POOL_Destroy(PB);
    MEDIA_POOL_Destroy(PA);
    MEDIA_SYS_Exit();
    return r ? 1 : 0;
}
