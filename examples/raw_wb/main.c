#include <linux/dma-buf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_raw_wb.h"

#define W 64
#define H 48
#define STRIDE 128
#define IN_POOL 0
#define OUT_POOL 1

static int write_frame(const char *path, MEDIA_BUFFER b)
{
    FILE *f = fopen(path, "wb");
    void *p = MEDIA_POOL_GetVaddr(b);
    size_t n = MEDIA_POOL_GetSize(b);
    int rc = -1;
    if (f && p && MEDIA_POOL_BeginCpuAccess(b, DMA_BUF_SYNC_READ) == 0) {
        rc = fwrite(p, 1, n, f) == n ? 0 : -1;
        MEDIA_POOL_EndCpuAccess(b, DMA_BUF_SYNC_READ);
    }
    if (f) fclose(f);
    return rc;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "raw_wb_out.raw16";
    MEDIA_BUFFER in, result;
    MEDIA_BUFFER_META meta;
    MEDIA_VIDEO_LAYOUT layout;
    MEDIA_RAW_WB_ATTR a;
    int made = 0, started = 0, rc = 1;
    size_t size;
    if (MEDIA_SYS_Init() != 0) return 1;
    if (MEDIA_VIDEO_BuildLinearLayout(W, H, MEDIA_FORMAT_BAYER_RAW16_LE, &layout, &size) != 0 ||
        MEDIA_POOL_CreateWithMode(IN_POOL, STRIDE * H, 2, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0 ||
        MEDIA_POOL_CreateWithMode(OUT_POOL, STRIDE * H, 2, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        fprintf(stderr, "RAW_WB pool/layout setup failed\n"); goto done;
    }
    memset(&a, 0, sizeof(a));
    a.width = W; a.height = H; a.input_depth = 2; a.output_pool_id = OUT_POOL;
    a.input_stride = STRIDE; a.output_stride = STRIDE;
    a.bayer_pattern = MEDIA_BAYER_PATTERN_RGGB;
    a.r_gain = 1.8f; a.g_gain = 1.0f; a.b_gain = 1.35f;
    if (MEDIA_RAW_WB_Create(0, &a) != 0 || (made = 1, MEDIA_RAW_WB_Start(0)) != 0) {
        fprintf(stderr, "RAW_WB create/start failed\n"); goto cleanup;
    }
    started = 1;
    if (MEDIA_POOL_GetBuffer(IN_POOL, &in) != 0) goto cleanup;
    memset(&meta, 0, sizeof(meta));
    meta.valid = 1; meta.type = MEDIA_BUFFER_META_VIDEO_FRAME;
    meta.media_format = MEDIA_FORMAT_BAYER_RAW16_LE; meta.len = STRIDE * H;
    meta.u.video = layout;
    uint16_t *px = (uint16_t *)MEDIA_POOL_GetVaddr(in);
    if (!px || MEDIA_POOL_BeginCpuAccess(in, DMA_BUF_SYNC_WRITE) != 0) { MEDIA_POOL_PutBuffer(in); goto cleanup; }
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
        *(uint16_t *)((uint8_t *)px + y * STRIDE + x * 2) = (uint16_t)(1000 + x * 200 + y * 50);
    MEDIA_POOL_EndCpuAccess(in, DMA_BUF_SYNC_WRITE);
    if (MEDIA_BUFFER_SetMeta(in, &meta) != 0 || MEDIA_SYS_SendFrame("RAW_WB", 0, "input0", in, 1000) != 0) {
        MEDIA_POOL_PutBuffer(in); goto cleanup;
    }
    if (MEDIA_RAW_WB_GetFrame(0, &result, 2000) != 0 || write_frame(out, result) != 0) {
        fprintf(stderr, "RAW_WB output unavailable\n"); goto cleanup;
    }
    MEDIA_RAW_WB_ReleaseFrame(0, result);
    printf("RAW_WB RGGB gains (%.2f, %.2f, %.2f): %dx%d -> %s\n", a.r_gain, a.g_gain, a.b_gain, W, H, out);
    rc = 0;
cleanup:
    if (started) MEDIA_RAW_WB_Stop(0);
    if (made) MEDIA_RAW_WB_Destroy(0);
    MEDIA_POOL_Destroy(OUT_POOL); MEDIA_POOL_Destroy(IN_POOL);
done:
    MEDIA_SYS_Exit();
    return rc;
}
