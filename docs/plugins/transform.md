# transform

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `TRANSFORM` | FILTER |

`libmedia_transform.so` 提供 `TRANSFORM` FILTER 类型（`MEDIA_TRANSFORM_TYPE`），沿用旧模块类型名，兼容旧应用的 `MEDIA_SYS_Bind("TRANSFORM", ...)`。它把输入视频的每个输出像素映射到一个 float32 XY LUT 坐标，使用 GLES fragment shader 双线性采样后写入输出 Pool。输入端口为 `input0`，输出端口为 `output0`；输入和输出格式、尺寸及 LINEAR/AFBC modifier 在创建时固定。

配置结构是 `MEDIA_TRANSFORM_ATTR`：`in_width/in_height/input_format/input_compress/in_stride` 描述输入；`out_width/out_height/output_format/output_compress/out_stride` 描述输出；`pool_id` 是应用创建的输出 Pool；`input_depth` 是输入队列深度；`lut_width/lut_height/lut/lut_size` 描述 XY LUT。LUT 每项按 `(x,y)` 排列，坐标相对于输入有效区域，越界坐标输出黑色（YUV 输出为黑色/中性色度）。LUT 尺寸可小于输出尺寸，运行时可通过 `MEDIA_TRANSFORM_UpdateLut` 更新。

示例：

```c
MEDIA_TRANSFORM_ATTR a = {
    .in_width = 1920
    .in_height = 1080
    .input_format = MEDIA_FORMAT_NV12
    .out_width = 1280
    .out_height = 720
    .output_format = MEDIA_FORMAT_ABGR8888
    .pool_id = output_pool
    .lut = xy
    .lut_width = 1280
    .lut_height = 720
    .lut_size = 1280u * 720u * 2u * sizeof(float)
}
MEDIA_TRANSFORM_CreateGrp(0, &a)
MEDIA_SYS_Bind("VI", 0, "output0", "TRANSFORM", 0, "input0")
MEDIA_TRANSFORM_Start(0)
```

输出 Pool 的 slot 数量应覆盖下游队列和在途帧，slot 容量至少为 `MEDIA_VIDEO_QueryLayout` 返回的大小。插件只使用公开 SDK 和 GLES；不提供 CPU、OpenCL、Vulkan 或 NPU 后端。输入帧必须带真实 `MEDIA_BUFFER_META_VIDEO_FRAME` 布局，压缩输入的 `stride` 必须为 0。

## 在发布包中使用

```c
#include "media_transform.h"
```

- 头文件：`include/media_transform.h`
- 模块映像：`plugins/libmedia_transform.so`（与 `libmedia_transform.so.sig` 必须同目录）
- 链接：`-lmedia_transform -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_transform.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("TRANSFORM", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

