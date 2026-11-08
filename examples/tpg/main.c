/* TPG tutorial: generate one NV12 test frame and save its raw payload. */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_tpg.h"

#define WIDTH 640
#define HEIGHT 480
#define POOL_ID 0

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tpg_frame.nv12";
    MEDIA_VIDEO_LAYOUT layout;
    MEDIA_TPG_ATTR attr;
    MEDIA_BUFFER buf;
    MEDIA_BUFFER_META meta;
    size_t size = 0;
    FILE *fp = NULL;
    void *vaddr;
    size_t payload;
    int ret = 1;
    int created = 0;
    int enabled = 0;

    if (MEDIA_SYS_Init() != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed\n");
        return 1;
    }
    if (MEDIA_VIDEO_BuildLinearLayout(WIDTH, HEIGHT, MEDIA_FORMAT_NV12,
                                      &layout, &size) != 0 ||
        MEDIA_POOL_CreateWithMode(POOL_ID, size, 4,
                                  MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        fprintf(stderr, "NV12 pool setup failed\n");
        goto done;
    }

    memset(&attr, 0, sizeof(attr));
    attr.width = WIDTH;
    attr.height = HEIGHT;
    attr.format = MEDIA_FORMAT_NV12;
    attr.fps = 30;
    attr.pool_id = POOL_ID;
    attr.pattern = MEDIA_TPG_PATTERN_COLOR_BARS;
    attr.output_compress = MEDIA_VIDEO_COMPRESS_NONE;
    ret = MEDIA_TPG_CreateChn(0, &attr);
    if (ret == 0)
        created = 1;
    if (ret != 0 || (ret = MEDIA_TPG_Enable(0)) != 0) {
        fprintf(stderr, "TPG create/enable failed: %d\n", ret);
        goto destroy_tpg;
    }
    enabled = 1;

    if ((ret = MEDIA_TPG_GetFrame(0, &buf, 2000)) != 0) {
        fprintf(stderr, "TPG get frame failed: %d\n", ret);
        goto destroy_tpg;
    }
    memset(&meta, 0, sizeof(meta));
    vaddr = MEDIA_POOL_GetVaddr(buf);
    payload = MEDIA_POOL_GetSize(buf);
    if (!vaddr || MEDIA_BUFFER_GetMeta(buf, &meta) != 0 || !meta.valid ||
        MEDIA_POOL_BeginCpuAccess(buf, DMA_BUF_SYNC_READ) != 0) {
        fprintf(stderr, "TPG frame metadata/access failed\n");
        MEDIA_TPG_ReleaseFrame(0, buf);
        goto destroy_tpg;
    }
    if (meta.len > 0 && meta.len < payload)
        payload = meta.len;
    fp = fopen(path, "wb");
    if (!fp || fwrite(vaddr, 1, payload, fp) != payload) {
        fprintf(stderr, "write %s failed\n", path);
        if (fp)
            fclose(fp);
        MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_READ);
        MEDIA_TPG_ReleaseFrame(0, buf);
        goto destroy_tpg;
    }
    fclose(fp);
    MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_READ);
    MEDIA_TPG_ReleaseFrame(0, buf);
    printf("TPG NV12 %dx%d: %zu bytes -> %s\n", WIDTH, HEIGHT, payload, path);
    ret = 0;

destroy_tpg:
    if (enabled)
        MEDIA_TPG_Disable(0);
    if (created)
        MEDIA_TPG_DestroyChn(0);
destroy_pool:
    MEDIA_POOL_Destroy(POOL_ID);
done:
    MEDIA_SYS_Exit();
    return ret;
}
