/*
 * vpss_16out —— 一路输入，16 路独立变换输出（RK3588 真机）
 *
 *   source --VPSS--> output0..output15
 *
 * 每一路单独配置旋转（0/90/180/270）、水平翻转、垂直翻转和保宽高比留边。
 * 每路用 MEDIA_SYS_GetFrame 抓到 CPU，转成 RGB24 拼成一张 4x4 大图。
 *
 * 全程不碰 DRM/VO，所以不需要独占显示，桌面上也能跑。
 *
 * 用法:
 *   ./vpss_16out <输出目录> [帧数] [间隔毫秒] [源]
 *
 *   源（省略则为 tpg）:
 *     tpg                  内置测试图案
 *     img:<path.rgb>       原始 RGB24 文件，需为 1920x1080，无文件头
 *
 * 输出:
 *   <输出目录>/frame_000.rgb ...    RGB24，画布尺寸写在 <输出目录>/info.txt
 *
 * 示例（用自己合成的图片做输入）:
 *   python3 -c "from PIL import Image; \
 *     open('in.rgb','wb').write(Image.open('in.png').convert('RGB').tobytes())"
 *   ./vpss_16out frames 1 0 img:in.rgb
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/dma-buf.h>
#include <time.h>
#include <unistd.h>

#include "media_api.h"
#include "media_plugin.h"
#include "media_tpg.h"
#include "media_vpss.h"

#define TPG_CHN   0
#define VPSS_GRP  0
#define SRC_W     1920
#define SRC_H     1080
#define TILE_W    480
#define TILE_H    270
#define GRID      4
#define NOUT      (GRID * GRID)

#define POOL_SRC  0
#define POOL_OUT  1 /* POOL_OUT + i，必须 < 32 */

/* 每路输出的变换：旋转、翻转、保持宽高比 */
typedef struct {
    int rotate;
    int flip_h;
    int flip_v;
    int keep_aspect;
} tile_cfg;

static const tile_cfg k_tiles[NOUT] = {
    { 0,   0, 0, 0 }, { 90,  0, 0, 0 }, { 180, 0, 0, 0 }, { 270, 0, 0, 0 },
    { 0,   1, 0, 0 }, { 0,   0, 1, 0 }, { 0,   1, 1, 0 }, { 0,   0, 0, 1 },
    { 90,  0, 0, 1 }, { 180, 0, 0, 1 }, { 270, 0, 0, 1 }, { 0,   1, 1, 1 },
    { 0,   0, 0, 0 }, { 90,  1, 0, 0 }, { 180, 1, 1, 0 }, { 270, 0, 1, 0 },
};

/* NV12 -> RGB24，BT.601 limited range */
static void nv12_to_rgb(const uint8_t *y, const uint8_t *uv,
                        int w, int h, uint32_t ypitch, uint32_t uvpitch,
                        uint8_t *dst, int dst_w, int dst_x, int dst_y)
{
    for (int r = 0; r < h; ++r) {
        const uint8_t *yr = y + (size_t)r * ypitch;
        const uint8_t *uvr = uv + (size_t)(r / 2) * uvpitch;
        uint8_t *out = dst + ((size_t)(dst_y + r) * dst_w + dst_x) * 3;

        for (int c = 0; c < w; ++c) {
            int Y = yr[c] - 16;
            int U = uvr[(c / 2) * 2] - 128;
            int V = uvr[(c / 2) * 2 + 1] - 128;
            int R = (298 * Y + 409 * V + 128) >> 8;
            int G = (298 * Y - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * Y + 516 * U + 128) >> 8;

            out[c * 3 + 0] = (uint8_t)(R < 0 ? 0 : R > 255 ? 255 : R);
            out[c * 3 + 1] = (uint8_t)(G < 0 ? 0 : G > 255 ? 255 : G);
            out[c * 3 + 2] = (uint8_t)(B < 0 ? 0 : B > 255 ? 255 : B);
        }
    }
}

/* RGB24 -> NV12，塞进一个池 Buffer 并按 VIDEO_FRAME 元数据发送。
 * SendFrame 成功后 buffer 归下游所有，不能再 PutBuffer。 */
static int send_rgb_as_nv12(int pool, int grp, const uint8_t *rgb)
{
    MEDIA_VIDEO_LAYOUT layout;
    size_t size = 0;
    MEDIA_BUFFER buf;
    MEDIA_BUFFER_META meta;
    uint8_t *base, *planar_y, *planar_uv;
    uint32_t yp, uvp;

    MEDIA_VIDEO_BuildLinearLayout(SRC_W, SRC_H, MEDIA_FORMAT_NV12, &layout, &size);
    if (MEDIA_POOL_GetBuffer(pool, &buf) != 0)
        return -1;
    base = (uint8_t *)MEDIA_POOL_GetVaddr(buf);
    if (!base || MEDIA_POOL_BeginCpuAccess(buf, DMA_BUF_SYNC_WRITE) != 0) {
        MEDIA_POOL_PutBuffer(buf);
        return -1;
    }

    yp = layout.pitch_bytes[0];
    uvp = layout.pitch_bytes[1];
    planar_y = base + layout.offset_bytes[0];
    planar_uv = base + layout.offset_bytes[1];

    for (int r = 0; r < SRC_H; ++r) {
        for (int c = 0; c < SRC_W; ++c) {
            const uint8_t *p = rgb + ((size_t)r * SRC_W + c) * 3;
            planar_y[(size_t)r * yp + c] =
                (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
        }
    }
    for (int r = 0; r < SRC_H / 2; ++r) {
        for (int c = 0; c < SRC_W / 2; ++c) {
            int sum_u = 0, sum_v = 0;

            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const uint8_t *p =
                        rgb + (((size_t)(2 * r + dy) * SRC_W + 2 * c + dx) * 3);
                    sum_u += ((-43 * p[0] - 85 * p[1] + 128 * p[2]) >> 8) + 128;
                    sum_v += ((128 * p[0] - 107 * p[1] - 21 * p[2]) >> 8) + 128;
                }
            }
            planar_uv[(size_t)r * uvp + (size_t)c * 2 + 0] = (uint8_t)(sum_u / 4);
            planar_uv[(size_t)r * uvp + (size_t)c * 2 + 1] = (uint8_t)(sum_v / 4);
        }
    }

    MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_WRITE);

    memset(&meta, 0, sizeof(meta));
    meta.valid = 1;
    meta.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    meta.media_format = MEDIA_FORMAT_NV12;
    meta.len = (uint32_t)size;
    meta.u.video = layout;

    if (MEDIA_BUFFER_SetMeta(buf, &meta) != 0 ||
        MEDIA_SYS_SendFrame("VPSS", grp, "input0", buf, 1000) != 0) {
        MEDIA_POOL_PutBuffer(buf);
        return -1;
    }
    return 0;
}

/* 把一个输出端口的帧贴到画布上。返回 1 表示成功。 */
static int blit_port(int grp, int idx, uint8_t *canvas, int canvas_w, int canvas_h)
{
    char port[32];
    MEDIA_BUFFER buf;
    MEDIA_BUFFER_META meta;
    void *vaddr;
    size_t bsize;
    int ok = 0;

    snprintf(port, sizeof(port), "output%d", idx);
    if (MEDIA_SYS_GetFrame("VPSS", grp, port, &buf, 1000) != 0)
        return 0;

    vaddr = MEDIA_POOL_GetVaddr(buf);
    bsize = MEDIA_POOL_GetSize(buf);
    if (vaddr && MEDIA_BUFFER_GetMeta(buf, &meta) == 0 && meta.valid) {
        const MEDIA_VIDEO_LAYOUT *L = &meta.u.video;
        const uint8_t *base = (const uint8_t *)vaddr;
        const uint8_t *yp = base + L->offset_bytes[0];
        const uint8_t *uvp = base + L->offset_bytes[1];
        int w = (int)L->visible_width;
        int h = (int)L->visible_height;
        int cx = (idx % GRID) * TILE_W;
        int cy = (idx / GRID) * TILE_H;

        if (yp + (size_t)h * L->pitch_bytes[0] <= base + bsize &&
            uvp + (size_t)(h / 2) * L->pitch_bytes[1] <= base + bsize &&
            cx + w <= canvas_w && cy + h <= canvas_h) {
            nv12_to_rgb(yp, uvp, w, h, L->pitch_bytes[0], L->pitch_bytes[1],
                        canvas, canvas_w, cx, cy);
            ok = 1;
        }
    }
    MEDIA_SYS_ReleaseFrame("VPSS", grp, port, buf);
    return ok;
}

int main(int argc, char **argv)
{
    const char *outdir = argc > 1 ? argv[1] : "frames";
    int nframes = argc > 2 ? atoi(argv[2]) : 1;
    int interval_ms = argc > 3 ? atoi(argv[3]) : 120;
    const char *src = argc > 4 ? argv[4] : "tpg";
    const char *img_path = strncmp(src, "img:", 4) == 0 ? src + 4 : NULL;
    const int canvas_w = TILE_W * GRID;
    const int canvas_h = TILE_H * GRID;
    MEDIA_VPSS_ATTR vattr;
    uint8_t *canvas;
    uint8_t *rgb_in = NULL;
    MEDIA_VIDEO_LAYOUT src_layout;
    size_t size = 0;
    char path[512];
    int ret, saved = 0;

    if (MEDIA_SYS_Init() != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed\n");
        return 1;
    }

    /* 输入池：TPG 与手动发送共用 */
    MEDIA_VIDEO_BuildLinearLayout(SRC_W, SRC_H, MEDIA_FORMAT_NV12, &src_layout, &size);
    if ((ret = MEDIA_POOL_CreateWithMode(POOL_SRC, size, 6,
                                         MEDIA_POOL_ALLOC_MODE_NATIVE)) != 0) {
        fprintf(stderr, "pool %d create failed: %d\n", POOL_SRC, ret);
        return 1;
    }

    if (img_path) {
        FILE *fp = fopen(img_path, "rb");
        const size_t want = (size_t)SRC_W * SRC_H * 3;

        if (!fp) {
            fprintf(stderr, "open %s failed\n", img_path);
            return 1;
        }
        rgb_in = malloc(want);
        if (!rgb_in || fread(rgb_in, 1, want, fp) != want) {
            fprintf(stderr, "%s must be raw RGB24 %dx%d (%zu bytes)\n",
                    img_path, SRC_W, SRC_H, want);
            return 1;
        }
        fclose(fp);
    } else {
        MEDIA_TPG_ATTR tattr;

        memset(&tattr, 0, sizeof(tattr));
        tattr.width = SRC_W;
        tattr.height = SRC_H;
        tattr.format = MEDIA_FORMAT_NV12;
        tattr.fps = 60;
        tattr.pool_id = POOL_SRC;
        tattr.pattern = MEDIA_TPG_PATTERN_COLOR_BARS;
        if ((ret = MEDIA_TPG_CreateChn(TPG_CHN, &tattr)) != 0) {
            fprintf(stderr, "TPG create failed: %d\n", ret);
            return 1;
        }
    }

    /* VPSS：16 路输出，每路一个独立输出池 */
    memset(&vattr, 0, sizeof(vattr));
    vattr.width = SRC_W;
    vattr.height = SRC_H;
    vattr.input_depth = 4;
    vattr.input_format = MEDIA_FORMAT_NV12;
    vattr.output_count = NOUT;
    vattr.backend = MEDIA_VPSS_BACKEND_GLES;
    vattr.input_color_space = MEDIA_COLOR_SPACE_BT601_LIMITED;
    for (int i = 0; i < NOUT; ++i) {
        MEDIA_VPSS_OUT_ATTR *o = &vattr.outputs[i];
        MEDIA_VIDEO_LAYOUT ol;
        size_t osize = 0;

        MEDIA_VIDEO_BuildLinearLayout(TILE_W, TILE_H, MEDIA_FORMAT_NV12, &ol, &osize);
        if ((ret = MEDIA_POOL_CreateWithMode(POOL_OUT + i, osize, 4,
                                             MEDIA_POOL_ALLOC_MODE_NATIVE)) != 0) {
            fprintf(stderr, "pool %d create failed: %d\n", POOL_OUT + i, ret);
            return 1;
        }
        o->output_id = i;
        o->out_width = TILE_W;
        o->out_height = TILE_H;
        o->out_stride = (int)ol.pitch_bytes[0];
        o->pool_id = POOL_OUT + i;
        o->crop_x = 0;
        o->crop_y = 0;
        o->crop_w = SRC_W;
        o->crop_h = SRC_H;
        o->in_fps = -1;
        o->out_fps = -1;
        o->flip_h = k_tiles[i].flip_h;
        o->flip_v = k_tiles[i].flip_v;
        o->rotate = k_tiles[i].rotate;
        o->output_format = MEDIA_FORMAT_NV12;
        o->keep_aspect_ratio = k_tiles[i].keep_aspect;
        o->compress = MEDIA_VIDEO_COMPRESS_NONE;
        o->dynamic_range = MEDIA_DYNAMIC_RANGE_SDR8;
        o->color_space = MEDIA_COLOR_SPACE_BT601_LIMITED;
    }
    if ((ret = MEDIA_VPSS_SetAttr(VPSS_GRP, &vattr)) != 0) {
        fprintf(stderr, "VPSS SetAttr failed: %d\n", ret);
        return 1;
    }

    if (!img_path)
        MEDIA_SYS_Bind("TPG", TPG_CHN, "output0", "VPSS", VPSS_GRP, "input0");

    MEDIA_VPSS_Enable(VPSS_GRP);
    if (!img_path)
        MEDIA_TPG_Enable(TPG_CHN);

    canvas = malloc((size_t)canvas_w * canvas_h * 3);
    if (!canvas) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    snprintf(path, sizeof(path), "%s/info.txt", outdir);
    FILE *info = fopen(path, "w");
    if (info) {
        fprintf(info, "%d %d\n", canvas_w, canvas_h);
        fclose(info);
    }

    for (int f = 0; f < nframes; ++f) {
        int got = 0;

        if (img_path && send_rgb_as_nv12(POOL_SRC, VPSS_GRP, rgb_in) != 0)
            fprintf(stderr, "send frame %d failed\n", f);
        memset(canvas, 16, (size_t)canvas_w * canvas_h * 3);
        for (int i = 0; i < NOUT; ++i)
            got += blit_port(VPSS_GRP, i, canvas, canvas_w, canvas_h);

        snprintf(path, sizeof(path), "%s/frame_%03d.rgb", outdir, saved);
        FILE *fp = fopen(path, "wb");
        if (fp) {
            fwrite(canvas, 1, (size_t)canvas_w * canvas_h * 3, fp);
            fclose(fp);
        }
        fprintf(stderr, "frame %d: %d/%d outputs -> %s\n", f, got, NOUT, path);
        saved++;
        if (interval_ms > 0) {
            struct timespec ts = { interval_ms / 1000,
                                   (interval_ms % 1000) * 1000000L };
            nanosleep(&ts, NULL);
        }
    }
    fprintf(stderr, "saved %d frame(s) to %s\n", saved, outdir);

    MEDIA_VPSS_Disable(VPSS_GRP);
    if (!img_path) {
        MEDIA_TPG_Disable(TPG_CHN);
        MEDIA_SYS_UnBind("TPG", TPG_CHN, "output0", "VPSS", VPSS_GRP, "input0");
    }
    MEDIA_VPSS_DestroyGrp(VPSS_GRP);
    if (!img_path)
        MEDIA_TPG_DestroyChn(TPG_CHN);
    for (int i = 0; i < NOUT; ++i)
        MEDIA_POOL_Destroy(POOL_OUT + i);
    MEDIA_POOL_Destroy(POOL_SRC);

    free(canvas);
    free(rgb_in);
    MEDIA_SYS_Exit();
    return 0;
}
