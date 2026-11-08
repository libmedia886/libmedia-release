/*
 * vi_mipi - capture frames from a MIPI camera through the libmedia VI plugin.
 *
 * The default device is /dev/video11, the RKISP main path on the RK3588 board.
 * Use a RKCIF node such as /dev/video0 when a raw Bayer stream is desired and
 * the VI plugin accepts that node/format on the target image.
 *
 * Usage:
 *   ./vi_mipi [device] [width] [height] [frames] [output_dir] [fps]
 *
 * The default output is NV12. Each frame is written as the driver's linear
 * payload (Y plane followed by UV plane) and info.txt records the actual
 * layout returned by VI. The output is intentionally raw so it can be fed to
 * ffmpeg, YUV viewers, or a later libmedia pipeline without conversion.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/dma-buf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "media_api.h"
#include "media_vi.h"

#define POOL_ID 0
#define BUF_COUNT 6
#define DEFAULT_DEVICE "/dev/video11"
#define DEFAULT_WIDTH 3840
#define DEFAULT_HEIGHT 2160
#define DEFAULT_FRAMES 1
#define DEFAULT_FPS 30
#define DEFAULT_OUT "frames_vi"

static int parse_positive(const char *s, const char *name, int *value)
{
    char *end = NULL;
    long n;

    errno = 0;
    n = strtol(s, &end, 10);
    if (errno || !end || *end != '\0' || n <= 0 || n > 100000) {
        fprintf(stderr, "%s must be a positive integer: %s\n", name, s);
        return -1;
    }
    *value = (int)n;
    return 0;
}

static int make_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;
    fprintf(stderr, "mkdir %s failed: %s\n", path, strerror(errno));
    return -1;
}

static void print_layout(FILE *fp, const MEDIA_BUFFER_META *meta, size_t pool_size)
{
    const MEDIA_VIDEO_LAYOUT *l = &meta->u.video;

    fprintf(fp, "format=%d\n", meta->media_format);
    fprintf(fp, "len=%u\n", meta->len);
    fprintf(fp, "timestamp_ns=%llu\n", (unsigned long long)meta->time_stamp);
    fprintf(fp, "seq=%u\n", meta->seq);
    fprintf(fp, "visible=%u %u\n", l->visible_width, l->visible_height);
    fprintf(fp, "storage=%u %u\n", l->storage_width, l->storage_height);
    fprintf(fp, "plane_count=%u\n", l->plane_count);
    fprintf(fp, "pitch=%u %u %u %u\n", l->pitch_bytes[0], l->pitch_bytes[1],
            l->pitch_bytes[2], l->pitch_bytes[3]);
    fprintf(fp, "offset=%u %u %u %u\n", l->offset_bytes[0], l->offset_bytes[1],
            l->offset_bytes[2], l->offset_bytes[3]);
    fprintf(fp, "drm_fourcc=0x%08x\n", l->drm_fourcc);
    fprintf(fp, "drm_modifier=0x%016llx\n", (unsigned long long)l->drm_modifier);
    fprintf(fp, "pool_size=%zu\n", pool_size);
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : DEFAULT_DEVICE;
    const char *outdir = argc > 5 ? argv[5] : DEFAULT_OUT;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int frames = DEFAULT_FRAMES;
    int fps = DEFAULT_FPS;
    MEDIA_VIDEO_LAYOUT expected;
    MEDIA_VI_ATTR attr;
    MEDIA_BUFFER_META meta;
    size_t pool_size = 0;
    int ret = 1;
    int enabled = 0;
    int initialized = 0;
    int failed = 0;
    char info_path[512];

    if (argc > 2 && parse_positive(argv[2], "width", &width) != 0)
        return 2;
    if (argc > 3 && parse_positive(argv[3], "height", &height) != 0)
        return 2;
    if (argc > 4 && parse_positive(argv[4], "frames", &frames) != 0)
        return 2;
    if (argc > 6 && parse_positive(argv[6], "fps", &fps) != 0)
        return 2;

    if (make_dir(outdir) != 0)
        return 1;

    if (MEDIA_SYS_Init() != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed\n");
        return 1;
    }
    initialized = 1;

    /* VI imports DMA-BUFs into the camera driver, so use CMA/native buffers. */
    if (MEDIA_VIDEO_BuildLinearLayout((uint32_t)width, (uint32_t)height,
                                      MEDIA_FORMAT_NV12, &expected, &pool_size) != 0) {
        fprintf(stderr, "unsupported NV12 layout for %dx%d\n", width, height);
        goto done;
    }
    if ((ret = MEDIA_POOL_CreateWithMode(POOL_ID, pool_size, BUF_COUNT,
                                         MEDIA_POOL_ALLOC_MODE_NATIVE)) != 0) {
        fprintf(stderr, "pool create failed: %d\n", ret);
        goto done;
    }

    memset(&attr, 0, sizeof(attr));
    attr.device = device;
    attr.width = width;
    attr.height = height;
    attr.fps = fps;
    attr.buf_cnt = BUF_COUNT - 2;
    attr.pool_id = POOL_ID;
    attr.format = MEDIA_FORMAT_NV12;
    attr.output_compress = MEDIA_VIDEO_COMPRESS_NONE;

    if ((ret = MEDIA_VI_SetAttr(0, &attr)) != 0) {
        fprintf(stderr, "MEDIA_VI_SetAttr(%s, %dx%d NV12) failed: %d\n",
                device, width, height, ret);
        goto destroy_pool;
    }
    if ((ret = MEDIA_VI_Enable(0)) != 0) {
        fprintf(stderr, "MEDIA_VI_Enable(%s) failed: %d\n", device, ret);
        goto destroy_vi;
    }
    enabled = 1;

    for (int i = 0; i < frames; ++i) {
        char path[512];
        FILE *fp;
        MEDIA_BUFFER buf;
        void *vaddr;
        size_t capacity;
        size_t payload;

        ret = MEDIA_VI_GetFrame(0, &buf, 3000);
        if (ret != 0) {
            fprintf(stderr, "frame %d: MEDIA_VI_GetFrame timeout/error: %d\n", i, ret);
            failed = 1;
            break;
        }

        memset(&meta, 0, sizeof(meta));
        vaddr = MEDIA_POOL_GetVaddr(buf);
        capacity = MEDIA_POOL_GetSize(buf);
        if (!vaddr || MEDIA_BUFFER_GetMeta(buf, &meta) != 0 || !meta.valid ||
            meta.type != MEDIA_BUFFER_META_VIDEO_FRAME || meta.media_format != MEDIA_FORMAT_NV12) {
            fprintf(stderr, "frame %d: invalid VI metadata\n", i);
            MEDIA_VI_ReleaseFrame(0, buf);
            failed = 1;
            break;
        }

        payload = meta.len;
        if (payload == 0 || payload > capacity)
            payload = capacity;
        if (MEDIA_POOL_BeginCpuAccess(buf, DMA_BUF_SYNC_READ) != 0) {
            fprintf(stderr, "frame %d: begin CPU access failed\n", i);
            MEDIA_VI_ReleaseFrame(0, buf);
            failed = 1;
            break;
        }

        snprintf(path, sizeof(path), "%s/frame_%03d.nv12", outdir, i);
        fp = fopen(path, "wb");
        if (!fp || fwrite(vaddr, 1, payload, fp) != payload) {
            fprintf(stderr, "frame %d: write %s failed\n", i, path);
            if (fp)
                fclose(fp);
            MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_READ);
            MEDIA_VI_ReleaseFrame(0, buf);
            failed = 1;
            break;
        }
        fclose(fp);
        MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_READ);

        if (i == 0) {
            FILE *info;

            snprintf(info_path, sizeof(info_path), "%s/info.txt", outdir);
            info = fopen(info_path, "w");
            if (info) {
                fprintf(info, "device=%s\n", device);
                print_layout(info, &meta, capacity);
                fclose(info);
            } else {
                fprintf(stderr, "open %s failed: %s\n", info_path, strerror(errno));
            }
        }
        printf("frame %d: %zu bytes -> %s\n", i, payload, path);
        MEDIA_VI_ReleaseFrame(0, buf);
    }

    ret = failed ? 1 : 0;

destroy_vi:
    if (enabled)
        MEDIA_VI_Disable(0);
    MEDIA_VI_Destroy(0);
destroy_pool:
    MEDIA_POOL_Destroy(POOL_ID);
done:
    if (initialized)
        MEDIA_SYS_Exit();
    return ret;
}
