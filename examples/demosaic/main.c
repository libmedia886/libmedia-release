/*
 * DEMOSAIC 入门教程：Bayer RAW16 -> 交错 RGB_U16
 *
 * 程序不需要相机：先在内存里生成一张 64x48 的 RAW16 棋盘图（每 8x8 一块交替
 * 亮/暗），送进 DEMOSAIC 模块，再把结果写成裸文件。
 *
 * 阅读顺序对应 README 的“3. 逐步理解程序”：
 *   [1] BuildLinearLayout  -> [2] 建 Pool -> [3] Create -> [4] 填输入帧并 Send
 *   -> [5] SendFrame -> [6] GetFrame / 保存 / Release
 * 每一步都有 [n] 标记，可以按编号和 README 对照。
 */
#include <linux/dma-buf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_demosaic.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define W 64            /* 可见宽度（像素） */
#define H 48            /* 可见高度（像素） */
#define IN_STRIDE 128   /* 输入每行实际字节数：64 像素 x 2 字节 = 128，正好对齐 */
#define OUT_STRIDE 384  /* 输出每行实际字节数：64 像素 x 3 通道 x 2 字节 = 384 */
#define IN_POOL 0       /* 输入 Pool 的编号，由本程序自己定，两个模块间别重复 */
#define OUT_POOL 1      /* 输出 Pool 的编号 */

/*
 * 把一块 Buffer 的内容原样写成裸文件（没有文件头）。
 * 返回 0 表示写成功。CPU 读取硬件缓冲区前后必须包在
 * BeginCpuAccess/EndCpuAccess 之间，否则可能读到 cache 里的旧数据。
 */
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
    const char *out = argc > 1 ? argv[1] : "demosaic_out.rgb_u16";
    MEDIA_BUFFER in, b;
    MEDIA_BUFFER_META m;
    MEDIA_VIDEO_LAYOUT li;
    MEDIA_DEMOSAIC_ATTR a;
    size_t si = 0, so = (size_t)OUT_STRIDE * H;
    int made = 0, started = 0, rc = 1;

    /* [1] 计算 RAW16 的 canonical layout，同时得到 Pool 需要的字节数 si。
     *     手工先填好可见尺寸/存储尺寸/单平面/行 pitch，函数会校验并补全偏移量。 */
    memset(&li, 0, sizeof(li));
    li.visible_width = W;
    li.visible_height = H;
    li.storage_width = W;
    li.storage_height = H;
    li.plane_count = 1;
    li.pitch_bytes[0] = IN_STRIDE;
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_BAYER_RAW16_LE, &li, &si) != 0) {
        fprintf(stderr, "DEMOSAIC RAW layout setup failed\n");
        return 1;
    }

    /* 使用任何 media API 之前必须先初始化系统。 */
    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [2] 创建两个 NATIVE Pool：NATIVE 才是可被 GLES/硬件导入的 DMA-BUF。
     *     输入 2 个 slot 足够“一帧在填、一帧在处理”。 */
    if (MEDIA_POOL_CreateWithMode(IN_POOL, si, 2, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, so, 2, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        fprintf(stderr, "DEMOSAIC pool setup failed\n");
        goto done;
    }

    /* [3] 创建模块实例 0。input_depth 是输入队列深度，不是位深。
     *     output_pool_id 指向上面建好的输出 Pool，模块从里面取空 buffer 写结果。 */
    memset(&a, 0, sizeof(a));
    a.width = W;
    a.height = H;
    a.input_depth = 2;
    a.output_pool_id = OUT_POOL;
    a.input_stride = IN_STRIDE;
    a.output_stride = OUT_STRIDE;
    a.bayer_pattern = MEDIA_BAYER_PATTERN_RGGB;
    if (MEDIA_DEMOSAIC_Create(0, &a) != 0) {
        fprintf(stderr, "DEMOSAIC create failed\n");
        goto cleanup;
    }
    made = 1;

    if (MEDIA_DEMOSAIC_Start(0) != 0) {
        fprintf(stderr, "DEMOSAIC start failed\n");
        goto cleanup;
    }
    started = 1;

    /* [4] 取一个输入 buffer，用 CPU 填棋盘图：8x8 块交替 50000 / 10000。
     *     这两个值只用来产生明显反差，方便肉眼确认去马赛克是否生效。 */
    if (MEDIA_POOL_GetBuffer(IN_POOL, &in) != 0)
        goto cleanup;
    {
        uint8_t *p = MEDIA_POOL_GetVaddr(in);
        if (!p || MEDIA_POOL_BeginCpuAccess(in, DMA_BUF_SYNC_WRITE) != 0) {
            MEDIA_POOL_PutBuffer(in);
            goto cleanup;
        }
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                *(uint16_t *)(p + y * IN_STRIDE + x * 2) =
                    (uint16_t)(((x / 8 + y / 8) & 1) ? 50000 : 10000);
            }
        }
        MEDIA_POOL_EndCpuAccess(in, DMA_BUF_SYNC_WRITE);
    }

    /* 只填了像素还不够：库靠 metadata 判断这块内存是什么格式、多大、怎么排。
     * 不设置 metadata 就送帧，模块无法正确解释 DMA-BUF。 */
    memset(&m, 0, sizeof(m));
    m.valid = 1;
    m.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    m.media_format = MEDIA_FORMAT_BAYER_RAW16_LE;
    m.len = IN_STRIDE * H;
    m.u.video = li;

    /* [5] 送帧。成功后 buffer 所有权交给模块，不能再 PutBuffer，
     *     否则会出现同一块内存被两边同时使用的问题。 */
    if (MEDIA_BUFFER_SetMeta(in, &m) != 0 ||
        MEDIA_SYS_SendFrame("DEMOSAIC", 0, "input0", in, 1000) != 0) {
        MEDIA_POOL_PutBuffer(in);
        goto cleanup;
    }

    /* [6] 取输出并保存。GetFrame 拿到的帧必须 ReleaseFrame 归还，否则 Pool 很快耗尽。 */
    if (MEDIA_DEMOSAIC_GetFrame(0, &b, 2000) != 0 || dump(out, b) != 0) {
        fprintf(stderr, "DEMOSAIC output unavailable\n");
        goto cleanup;
    }
    MEDIA_DEMOSAIC_ReleaseFrame(0, b);

    printf("DEMOSAIC RGGB RAW16 %dx%d -> RGB_U16 %s\n", W, H, out);
    rc = 0;

cleanup:
    /* 逆序拆解：先停再销毁模块，最后销毁 Pool。 */
    if (started)
        MEDIA_DEMOSAIC_Stop(0);
    if (made)
        MEDIA_DEMOSAIC_Destroy(0);
    MEDIA_POOL_Destroy(OUT_POOL);
    MEDIA_POOL_Destroy(IN_POOL);

done:
    MEDIA_SYS_Exit();
    return rc;
}
