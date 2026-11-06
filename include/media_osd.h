#ifndef MEDIA_OSD_H
#define MEDIA_OSD_H

/*
 * OSD（GLES 多 region 叠加）外部模块 API，由 libmedia_osd.so 提供。
 *
 * 与原内置 OSD 源码兼容：函数名、参数、结构体字段顺序均保持不变，原先
 * include "media_api.h" 即可见的部分改为 include 本头文件。实例名仍为 "OSD_<grp>"，
 * 端口 input0 / output0，因此 MEDIA_SYS_Bind("OSD", grp, "output0", ...) 的规则与
 * 迁移前一致。
 *
 * 两种用法：
 *   1) 直接链接 libmedia_osd.so 调用 MEDIA_OSD_*（与旧内置 API 相同）；
 *   2) 作为插件 MEDIA_PLUGIN_Load("libmedia_osd.so") 后用
 *      MEDIA_PLUGIN_CreateModule("OSD", grp, &attr, sizeof(attr)) 创建。
 *
 * 限制：只支持 LINEAR 输入/输出（input_compress / output_compress 必须为 0），
 * 由 GPU 直接渲染；region 叠加在 GPU 上一次绘制完成，不做 CPU 回写。
 */

#include <stddef.h>
#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_OSD_CONTENT_NONE     0
#define MEDIA_OSD_CONTENT_RECT     1
#define MEDIA_OSD_CONTENT_A8_MASK  2
#define MEDIA_OSD_CONTENT_ARGB8888 3
#define MEDIA_OSD_MAX_REGIONS      64

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} MEDIA_OSD_COLOR;

typedef struct {
    int enabled;
    int x;
    int y;
    int width;
    int height;
    uint8_t global_alpha; /* later region_id covers earlier when overlapping */
} MEDIA_OSD_REGION_ATTR;

typedef struct {
    int filled;
    int line_width;
    MEDIA_OSD_COLOR color;
} MEDIA_OSD_RECT_DESC;

typedef struct {
    int width;
    int height;
    int stride;
    const uint8_t *data;
    size_t data_size;
    MEDIA_OSD_COLOR color;
} MEDIA_OSD_MASK_DESC;

typedef struct {
    int input_width;
    int input_height;
    int format;
    int input_depth;
    int output_pool_id;
    int input_stride;
    int output_stride;
    int max_regions;
    MEDIA_VIDEO_COMPRESS input_compress;
    MEDIA_VIDEO_COMPRESS output_compress;
} MEDIA_OSD_ATTR;

int MEDIA_OSD_CreateGrp(int grp, const MEDIA_OSD_ATTR *attr);
int MEDIA_OSD_DestroyGrp(int grp);
int MEDIA_OSD_Start(int grp);
int MEDIA_OSD_Stop(int grp);
int MEDIA_OSD_Enable(int grp);
int MEDIA_OSD_Disable(int grp);
int MEDIA_OSD_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_OSD_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_OSD_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_OSD_UpdateRegion(int grp, int region_id, const MEDIA_OSD_REGION_ATTR *attr);
int MEDIA_OSD_SetRegionRect(int grp, int region_id, const MEDIA_OSD_RECT_DESC *rect);
int MEDIA_OSD_SetRegionMask(int grp, int region_id, const MEDIA_OSD_MASK_DESC *mask);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_OSD_H */
