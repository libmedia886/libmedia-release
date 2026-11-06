#ifndef MEDIA_RETINEX_H
#define MEDIA_RETINEX_H

/*
 * Retinex 图像增强外部模块 API，由 libmedia_retinex.so 提供。
 * 插件类型名为 "RETINEX"，实例名为 "RETINEX_<grp>"，端口为 input0..inputN-1
 * 与 output0。旧应用只需包含本头文件并链接 libmedia_retinex.so。
 */

#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_RETINEX_MAX_SCALE 3
#define MEDIA_RETINEX_TYPE "RETINEX"

typedef struct {
    int scale_count;     /* 输入路数：1、2 或 3；第 0 路为原始帧，其余为高斯模糊帧 */
    int width;           /* 图像有效宽度，必须为偶数 */
    int height;          /* 图像有效高度，必须为偶数 */
    int format;          /* 图像格式，仅支持 MEDIA_FORMAT_NV12 */
    int output_depth;    /* 输出端口队列深度，0 表示默认值 4 */
    int input_depth;     /* 每个输入端口队列深度，0 表示默认值 4 */
    int input_stride;    /* 输入 Y 平面字节 pitch；0 表示使用 SDK canonical pitch */
    int output_stride;   /* 输出 Y 平面字节 pitch；0 表示使用 SDK canonical pitch */
    float gain;          /* Retinex 增益，创建值不大于 0 时使用默认值 30.0 */
    float threshold;     /* 旧模块保留的阈值参数，当前 GLES 算法不参与计算 */
    float log_min;       /* 对数域的亮度下限，直接参与输出映射 */
    float log_max;       /* 旧模块保留的上限参数，当前 GLES 算法不参与计算 */
    int passthrough;     /* 1 表示直通；仅 scale_count == 1 时允许，否则拒绝 */
    /*
     * 输出 Buffer Pool：大于 0 时使用该 Pool；小于等于 0 时逐帧复用
     * input0 所在 Pool。该字段追加在旧结构体末尾，旧调用方可传旧结构体长度。
     */
    int output_pool_id;
} MEDIA_RETINEX_ATTR;

int MEDIA_RETINEX_CreateGrp(int grp, const MEDIA_RETINEX_ATTR *attr);
int MEDIA_RETINEX_SetGrpAttr(int grp, const MEDIA_RETINEX_ATTR *attr);
int MEDIA_RETINEX_DestroyGrp(int grp);
int MEDIA_RETINEX_Start(int grp);
int MEDIA_RETINEX_Stop(int grp);
int MEDIA_RETINEX_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_RETINEX_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_RETINEX_SetPassthrough(int grp, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_RETINEX_H */

