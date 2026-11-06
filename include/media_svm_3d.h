#ifndef MEDIA_SVM_3D_H
#define MEDIA_SVM_3D_H

#include <stddef.h>
#include <stdint.h>
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_SVM3D_MAX_INPUTS 8
#define MEDIA_SVM3D_MAX_OUTPUTS 2
#define MEDIA_SVM3D_TYPE "SVM3D"

typedef struct {
    int input_count;          /* 输入路数，范围 2..8。 */
    int input_format;         /* 输入格式：NV12 或 RGB/BGR/RGBA/BGRA/ARGB/ABGR。 */
    int in_width;             /* 输入有效宽度。 */
    int in_height;            /* 输入有效高度。 */
    int in_stride;            /* 输入线性 stride，0 表示按格式查询。 */
    int output_format;        /* 输出格式：RGBA/BGRA/ARGB/ABGR8888。 */
    int out_width;            /* 输出有效宽度。 */
    int out_height;           /* 输出有效高度。 */
    int out_stride;           /* 输出线性 stride，0 表示按格式查询。 */
    int output_pool_id;       /* output0 使用的 Pool。 */
    int top_output_enable;    /* 非零时创建 output1。 */
    int top_output_pool_id;   /* output1 使用的 Pool。 */
    int input_depth;          /* 每个输入队列深度，0 使用框架默认值。 */
    int output_depth;         /* 为兼容旧属性保留。 */
    int sync_timeout_ms;      /* 等待其它输入的超时，<=0 使用 100 ms。 */
    const char *asset_path;   /* svm_3d_asset_v1 清单路径，可含 mesh_path。 */
    int sync_wait_inputs;     /* 非零等待每一路；零也会等待一次以保证成帧。 */
} MEDIA_SVM3D_ATTR;

typedef struct {
    float yaw_deg;            /* 环视视角水平角。 */
    float pitch_deg;          /* 环视视角俯仰角。 */
    float distance_m;         /* 相机距离。 */
    float target_z_m;         /* 目标高度。 */
    float fov_y_deg;          /* 垂直视场角。 */
} MEDIA_SVM3D_VIEW_ATTR;

int MEDIA_SVM3D_CreateGrp(int grp, const MEDIA_SVM3D_ATTR *attr);
int MEDIA_SVM3D_CreateGrpEx(int grp, const MEDIA_SVM3D_ATTR *attr, size_t attr_size);
int MEDIA_SVM3D_DestroyGrp(int grp);
int MEDIA_SVM3D_Start(int grp);
int MEDIA_SVM3D_Stop(int grp);
int MEDIA_SVM3D_Enable(int grp);
int MEDIA_SVM3D_Disable(int grp);
int MEDIA_SVM3D_SendFrame(int grp, int input_id, MEDIA_BUFFER buf, int timeout_ms);
int MEDIA_SVM3D_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_SVM3D_GetFrameEx(int grp, int output_id, MEDIA_BUFFER *buf, int timeout_ms);
int MEDIA_SVM3D_ReleaseFrame(int grp, MEDIA_BUFFER buf);
int MEDIA_SVM3D_SetView(int grp, const MEDIA_SVM3D_VIEW_ATTR *attr);
int MEDIA_SVM3D_GetView(int grp, MEDIA_SVM3D_VIEW_ATTR *attr);
int MEDIA_SVM3D_SetViewEx(int grp, int output_id, const MEDIA_SVM3D_VIEW_ATTR *attr);
int MEDIA_SVM3D_GetViewEx(int grp, int output_id, MEDIA_SVM3D_VIEW_ATTR *attr);

#ifdef __cplusplus
}
#endif
#endif
