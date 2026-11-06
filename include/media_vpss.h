#ifndef MEDIA_VPSS_H
#define MEDIA_VPSS_H

/*
 * VPSS（GLES 视频处理：裁剪 / 缩放 / 旋转 / 翻转 / 色彩空间 / 格式与 AFBC 转换，最多 16 路输出）
 * 外部模块 API，由 libmedia_vpss.so 提供。与原内置 VPSS 源码兼容：包含本头文件并链接
 * libmedia_vpss.so。实例名 "VPSS_<grp>"，端口 input0 / output0..N，可直接
 * MEDIA_SYS_Bind("VPSS", grp, "outputN", ...)。
 */

#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_VPSS_MAX_OUTPUTS 16

typedef enum {
    MEDIA_VPSS_BACKEND_DEFAULT = 0,
    MEDIA_VPSS_BACKEND_VULKAN = 1,  /* 已移除，返回 MEDIA_ERR_UNSUPPORTED；保留数值兼容 */
    MEDIA_VPSS_BACKEND_OPENCL = 2,  /* 已移除，返回 MEDIA_ERR_UNSUPPORTED；保留数值兼容 */
    MEDIA_VPSS_BACKEND_GLES = 3,
} MEDIA_VPSS_BACKEND;

typedef struct {
    int output_id;
    int out_width;
    int out_height;
    int out_stride;  // plane0 byte pitch；LINEAR须匹配Query，AFBC须为0
    int pool_id;
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
    int in_fps;   // input fps reference (-1=disable control, 0=not allowed, >0=control fps)
    int out_fps;  // target output fps (-1=disable control, 0=not allowed, >0=control fps)
    int flip_h;
    int flip_v;
    int rotate;
    int output_format;  // GLES含ABGR2101010；BGR101010仅准入opaque XB30+AFBC 0x041输出
    int keep_aspect_ratio;  // 1=按比例缩放并在输出内居中填黑，0=拉伸到整幅输出
    MEDIA_VIDEO_COMPRESS compress;  // 0=LINEAR/NONE；非零值为精确AFBC block/flags组合
    MEDIA_DYNAMIC_RANGE dynamic_range; // GLES: SDR8或SDR10；零初始化为SDR8，无AUTO
    MEDIA_COLOR_SPACE color_space; // GLES: YUV用601/709×limited/full；packed RGB用RGB_FULL/RGB_LIMITED；无AUTO
} MEDIA_VPSS_OUT_ATTR;

typedef struct {
    int width;
    int height;
    int input_stride;  // plane0 byte pitch；非零 input_compress 必须为0并逐帧读取真实layout
    int input_depth;   // 输入端口队列深度（0 表示默认=4）
    int input_format;  // GLES可含实测高位格式
    MEDIA_VIDEO_COMPRESS input_compress; // 0=LINEAR/NONE；非零值为精确AFBC block/flags组合
    int in_fps;
    int out_fps;
    int output_count;  // 输出数量（1-MEDIA_VPSS_MAX_OUTPUTS）
    MEDIA_VPSS_OUT_ATTR outputs[MEDIA_VPSS_MAX_OUTPUTS];
    MEDIA_DYNAMIC_RANGE input_dynamic_range; // GLES: SDR8或SDR10；零初始化为SDR8，无AUTO
    MEDIA_COLOR_SPACE input_color_space; // GLES: YUV输入用matrix/range；packed RGB输入用RGB range；无AUTO
    /* 固定使用 GLES；DEFAULT 等价于 GLES，VULKAN/OPENCL 返回 MEDIA_ERR_UNSUPPORTED。 */
    MEDIA_VPSS_BACKEND backend;
} MEDIA_VPSS_ATTR;

int MEDIA_VPSS_SetAttr(int grp_id, const MEDIA_VPSS_ATTR *attr);
int MEDIA_VPSS_SetOutCrop(int grp_id, int output_id, int crop_x, int crop_y, int crop_w, int crop_h);
int MEDIA_VPSS_SetOutAttr(int grp_id, const MEDIA_VPSS_OUT_ATTR *attr);
int MEDIA_VPSS_DestroyGrp(int grp_id);
int MEDIA_VPSS_Enable(int grp_id);
int MEDIA_VPSS_Disable(int grp_id);
int MEDIA_VPSS_Group_SendFrame(int grp_id, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_VPSS_Chn_GetFrame(int grp_id, int output_id, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_VPSS_Chn_ReleaseFrame(int grp_id, int output_id, MEDIA_BUFFER buf);
int MEDIA_VPSS_SendFrame(int grp_id, const MEDIA_VIDEO_FRAME *frame, int timeout_ms);
int MEDIA_VPSS_GetChnFrame(int grp_id, int output_id, MEDIA_VIDEO_FRAME *frame, int timeout_ms);
int MEDIA_VPSS_ReleaseChnFrame(int grp_id, int output_id, const MEDIA_VIDEO_FRAME *frame);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_VPSS_H */
