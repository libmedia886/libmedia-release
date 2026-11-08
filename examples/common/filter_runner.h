#ifndef LIBMEDIA_FILTER_RUNNER_H
#define LIBMEDIA_FILTER_RUNNER_H

/* Small helpers shared by the single-input filter tutorials. */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_tpg.h"

static int tutorial_make_pool(int pool_id, int width, int height, int format,
                              size_t *size_out)
{
    MEDIA_VIDEO_LAYOUT layout;
    size_t size = 0;
    int ret = MEDIA_VIDEO_BuildLinearLayout((uint32_t)width, (uint32_t)height,
                                            format, &layout, &size);
    if (ret != MEDIA_STATUS_OK)
        return ret;
    ret = MEDIA_POOL_CreateWithMode(pool_id, size, 8,
                                    MEDIA_POOL_ALLOC_MODE_NATIVE);
    if (ret == MEDIA_STATUS_OK && size_out)
        *size_out = size;
    return ret;
}

static int tutorial_save_buffer(const char *path, MEDIA_BUFFER buf)
{
    MEDIA_BUFFER_META meta;
    void *vaddr;
    size_t payload;
    FILE *fp;
    int ret = -1;

    memset(&meta, 0, sizeof(meta));
    vaddr = MEDIA_POOL_GetVaddr(buf);
    payload = MEDIA_POOL_GetSize(buf);
    if (!vaddr || MEDIA_BUFFER_GetMeta(buf, &meta) != MEDIA_STATUS_OK ||
        !meta.valid || MEDIA_POOL_BeginCpuAccess(buf, DMA_BUF_SYNC_READ) != 0)
        return -1;
    if (meta.len > 0 && meta.len <= payload)
        payload = meta.len;
    fp = fopen(path, "wb");
    if (fp && fwrite(vaddr, 1, payload, fp) == payload)
        ret = 0;
    if (fp)
        fclose(fp);
    MEDIA_POOL_EndCpuAccess(buf, DMA_BUF_SYNC_READ);
    if (ret == 0)
        printf("saved %zu bytes to %s (format=%d %ux%u)\n", payload, path,
               meta.media_format, meta.u.video.visible_width,
               meta.u.video.visible_height);
    return ret;
}

static int tutorial_load_pair(const char *plugin_dir, const char *plugin_name)
{
    char path[512];
    int ret;
    snprintf(path, sizeof(path), "%s/libmedia_tpg.so", plugin_dir);
    ret = MEDIA_PLUGIN_Load(path);
    if (ret < 0 && ret != MEDIA_ERR_ALREADY_EXISTS)
        return ret;
    snprintf(path, sizeof(path), "%s/%s", plugin_dir, plugin_name);
    ret = MEDIA_PLUGIN_Load(path);
    if (ret < 0 && ret != MEDIA_ERR_ALREADY_EXISTS)
        return ret;
    return MEDIA_STATUS_OK;
}

static int tutorial_run_pipeline(const char *type, const char *output_path,
                                 int width, int height, int input_format,
                                 int output_width, int output_height,
                                 int output_format, const void *attr,
                                 size_t attr_size, int frames)
{
    const int input_pool = 20;
    const int output_pool = 21;
    MEDIA_TPG_ATTR tpg;
    MEDIA_BUFFER out;
    size_t in_size = 0, out_size = 0;
    char path[512];
    int created_tpg = 0, created_filter = 0, started_tpg = 0;
    int started_filter = 0, bound = 0, ret = 1;

    if (tutorial_make_pool(input_pool, width, height, input_format, &in_size) != 0 ||
        tutorial_make_pool(output_pool, output_width, output_height, output_format,
                           &out_size) != 0) {
        fprintf(stderr, "failed to create input/output pools\n");
        goto done;
    }
    memset(&tpg, 0, sizeof(tpg));
    tpg.width = width;
    tpg.height = height;
    tpg.format = input_format;
    tpg.fps = 30;
    tpg.pool_id = input_pool;
    tpg.pattern = MEDIA_TPG_PATTERN_COLOR_BARS;
    tpg.block_size = 32;
    tpg.fg_color = 0xff8040;
    tpg.bg_color = 0x102040;
    tpg.output_compress = MEDIA_VIDEO_COMPRESS_NONE;
    ret = MEDIA_PLUGIN_CreateModule("TPG", 0, &tpg, sizeof(tpg));
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "TPG CreateModule failed: %d\n", ret);
        goto done;
    }
    created_tpg = 1;
    ret = MEDIA_PLUGIN_CreateModule(type, 0, attr, attr_size);
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "%s CreateModule failed: %d\n", type, ret);
        goto done;
    }
    created_filter = 1;
    ret = MEDIA_SYS_Bind("TPG", 0, "output0", type, 0, "input0");
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "Bind TPG -> %s failed: %d\n", type, ret);
        goto done;
    }
    bound = 1;
    ret = MEDIA_PLUGIN_Start(type, 0);
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "%s Start failed: %d\n", type, ret);
        goto done;
    }
    started_filter = 1;
    ret = MEDIA_PLUGIN_Start("TPG", 0);
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "TPG Start failed: %d\n", ret);
        goto done;
    }
    started_tpg = 1;
    if (frames < 1)
        frames = 1;
    for (int i = 0; i < frames; ++i) {
        ret = MEDIA_PLUGIN_GetFrame(type, 0, 0, &out, 3000);
        if (ret != MEDIA_STATUS_OK) {
            fprintf(stderr, "%s GetFrame[%d] failed: %d\n", type, i, ret);
            goto done;
        }
        if (frames == 1)
            snprintf(path, sizeof(path), "%s", output_path);
        else
            snprintf(path, sizeof(path), "%s.%02d", output_path, i);
        if (tutorial_save_buffer(path, out) != 0)
            fprintf(stderr, "could not save %s\n", path);
        MEDIA_PLUGIN_ReleaseFrame(type, 0, 0, out);
    }
    ret = 0;

done:
    if (started_tpg)
        MEDIA_PLUGIN_Stop("TPG", 0);
    if (started_filter)
        MEDIA_PLUGIN_Stop(type, 0);
    if (bound)
        MEDIA_SYS_UnBind("TPG", 0, "output0", type, 0, "input0");
    if (created_filter)
        MEDIA_PLUGIN_DestroyModule(type, 0);
    if (created_tpg)
        MEDIA_PLUGIN_DestroyModule("TPG", 0);
    MEDIA_POOL_Destroy(output_pool);
    MEDIA_POOL_Destroy(input_pool);
    (void)in_size;
    (void)out_size;
    return ret;
}

#endif
