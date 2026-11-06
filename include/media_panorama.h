#ifndef MEDIA_PANO_H
#define MEDIA_PANO_H

#include <stddef.h>
#include <stdint.h>

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_PANO_MAX_INPUTS 8
#define MEDIA_AVM_MAX_INPUTS MEDIA_PANO_MAX_INPUTS
#define MEDIA_PANO_TYPE "PANO"
#define MEDIA_AVM_TYPE "AVM"

/* PANO 逐路输入的配置。字段顺序与旧版公开 ABI 一致。 */
typedef struct {
    int input_count; /* 输入路数，范围为 2 到 8。 */
    int in_width; /* 每路输入图像的有效宽度。 */
    int in_height; /* 每路输入图像的有效高度。 */
    int in_stride; /* LINEAR 输入字节步长，0 表示默认步长。 */
    int out_width; /* 输出图像宽度。 */
    int out_height; /* 输出图像高度，NV12 奇数高度会向上取偶数。 */
    int out_stride; /* LINEAR 输出字节步长，0 表示默认步长。 */
    int crop_enable; /* 是否启用 PTO 全景域裁剪。 */
    int crop_x; /* 裁剪窗口左上角 X。 */
    int crop_y; /* 裁剪窗口左上角 Y。 */
    int crop_width; /* 裁剪窗口宽度，0 表示域剩余宽度。 */
    int crop_height; /* 裁剪窗口高度，0 表示域剩余高度。 */
    int output_pool_id; /* 输出 buffer 池 ID。 */
    int input_depth; /* 输入端口队列深度，0 表示默认值。 */
    int output_depth; /* 输出端口队列深度，0 表示默认值。 */
    int sync_timeout_ms; /* 多路同步超时，非正值使用默认值。 */
    int lut_width; /* LUT 宽度，0 表示默认使用输出宽度。 */
    int lut_height; /* LUT 高度，0 表示默认使用输出高度。 */
    const char *pto_path; /* PTO 或 PTS 标定文件路径。 */
    MEDIA_VIDEO_COMPRESS input_compress[MEDIA_PANO_MAX_INPUTS]; /* 每路输入的存储压缩。 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 输出存储压缩，当前只支持 LINEAR。 */
    int gain_y_enable; /* 是否启用逐相机 Y 增益补偿。 */
    uint16_t gain_y_q8[MEDIA_PANO_MAX_INPUTS]; /* Q8.8 增益，256 表示 1.0。 */
    int sync_wait_inputs; /* 是否等待其它输入路，CreateGrpEx 尾字段。 */
} MEDIA_PANO_ATTR;

/* AVM 逐路输入的配置。字段顺序与旧版公开 ABI 一致。 */
typedef struct {
    int input_count; /* 输入路数，范围为 2 到 8。 */
    int in_width; /* 每路输入图像的有效宽度。 */
    int in_height; /* 每路输入图像的有效高度。 */
    int in_stride; /* LINEAR 输入字节步长，0 表示默认步长。 */
    int out_width; /* 输出图像宽度。 */
    int out_height; /* 输出图像高度，NV12 奇数高度会向上取偶数。 */
    int out_stride; /* LINEAR 输出字节步长，0 表示默认步长。 */
    int output_pool_id; /* 输出 buffer 池 ID。 */
    int input_depth; /* 输入端口队列深度，0 表示默认值。 */
    int output_depth; /* 输出端口队列深度，0 表示默认值。 */
    int sync_timeout_ms; /* 多路同步超时，非正值使用默认值。 */
    int lut_width; /* LUT 宽度，0 表示输出宽度。 */
    int lut_height; /* LUT 高度，0 表示输出高度。 */
    const char *lut_path; /* 预计算二进制 LUT 文件路径。 */
    MEDIA_VIDEO_COMPRESS input_compress[MEDIA_AVM_MAX_INPUTS]; /* 每路输入的存储压缩。 */
    MEDIA_VIDEO_COMPRESS output_compress; /* 输出存储压缩，当前只支持 LINEAR。 */
    int sync_wait_inputs; /* 是否等待其它输入路，CreateGrpEx 尾字段。 */
} MEDIA_AVM_ATTR;

#define MEDIA_PANO_LUT_XY(x, y) \
    ((uint32_t)(x) | ((uint32_t)(y) << 16))
#define MEDIA_PANO_LUT_INFO(camera, valid, alpha) \
    ((uint32_t)(camera) | ((uint32_t)((valid) != 0) << 8) | ((uint32_t)(alpha) << 16))

int MEDIA_PANO_CreateGrp(int grp, const MEDIA_PANO_ATTR *attr);
int MEDIA_PANO_CreateGrpEx(int grp, const MEDIA_PANO_ATTR *attr, size_t attr_size);
int MEDIA_PANO_DestroyGrp(int grp);
int MEDIA_PANO_Start(int grp);
int MEDIA_PANO_Stop(int grp);
int MEDIA_PANO_Enable(int grp);
int MEDIA_PANO_Disable(int grp);
int MEDIA_PANO_SetCrop(int grp, int crop_x, int crop_y);
int MEDIA_PANO_SendFrame(int grp, int input_id, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_PANO_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_PANO_ReleaseFrame(int grp, MEDIA_BUFFER buf);

int MEDIA_AVM_CreateGrp(int grp, const MEDIA_AVM_ATTR *attr);
int MEDIA_AVM_CreateGrpEx(int grp, const MEDIA_AVM_ATTR *attr, size_t attr_size);
int MEDIA_AVM_DestroyGrp(int grp);
int MEDIA_AVM_Start(int grp);
int MEDIA_AVM_Stop(int grp);
int MEDIA_AVM_Enable(int grp);
int MEDIA_AVM_Disable(int grp);
int MEDIA_AVM_SendFrame(int grp, int input_id, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_AVM_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_AVM_ReleaseFrame(int grp, MEDIA_BUFFER buf);

#ifdef __cplusplus
}
#endif

#endif

