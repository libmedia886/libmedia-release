# eis

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `EIS` | FILTER |


当前实现支持偶数尺寸、线性 `MEDIA_FORMAT_NV12` 输入和输出，输入输出尺寸相同。输入端口的 `input_depth` 为队列深度，输出从 `output_pool_id` 申请，Pool 至少应覆盖下游队列和在途帧。输出 metadata 透传时间戳和序号，并发布 SDK 查询到的真实 NV12 布局。`MEDIA_EIS_SetPassthrough` 保留旧 API 的直通开关；当前 GLES 恒等 warp 已经是无运动校正的直通结果。

配置字段沿用 `MEDIA_EIS_ATTR`：`width`/`height` 是帧尺寸，`format` 当前必须为 NV12，`input_depth` 是输入队列深度，`output_pool_id` 是输出 Pool，`input_stride`/`output_stride` 是线性 plane0 步长，`crop_ratio`、`smoothing_window`、`estimate_width`、`search_radius`、`block_step` 保留旧 API 语义，`passthrough` 初始直通开关。DIS、IMU 和统计结构及 `MEDIA_EIS_*` 函数名与旧公开头完全一致；未实现的控制字段会被保存并可读回。

典型使用方式：

```c
MEDIA_EIS_ATTR attr = {
    .width = 1920, .height = 1080, .format = MEDIA_FORMAT_NV12
    .input_depth = 4, .output_pool_id = 12
    .input_stride = 1920, .output_stride = 1920
}
MEDIA_EIS_CreateGrp(0, &attr)
MEDIA_SYS_Bind("VI", 0, "output0", "EIS", 0, "input0")
MEDIA_EIS_Start(0)
/* 从 EIS output0 取帧，使用 MEDIA_EIS_ReleaseFrame 归还。 */
MEDIA_EIS_Stop(0)
MEDIA_SYS_UnBind("VI", 0, "output0", "EIS", 0, "input0")
MEDIA_EIS_DestroyGrp(0)
```

独立构建：

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DMEDIA_SDK_DIR=/root/work/rktohi/wt/sdk
cmake --build build -j2
LD_LIBRARY_PATH=/root/work/rktohi/wt/sdk/lib (cd build && ctest) --output-on-failure
```

限制：仅 GLES、仅 NV12 线性布局、固定尺寸一进一出；不支持 AFBC、多路输入、RAW Bayer、滚动快门行变形和运行期改变端口尺寸。应用若要使用传感器或融合模式，可以调用兼容 API，但当前版本不会执行旧算法的 IMU/视觉估计。

## 在发布包中使用

```c
#include "media_eis.h"
```

- 头文件：`include/media_eis.h`
- 模块映像：`plugins/libmedia_eis.so`（与 `libmedia_eis.so.sig` 必须同目录）
- 链接：`-lmedia_eis -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_eis.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("EIS", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

