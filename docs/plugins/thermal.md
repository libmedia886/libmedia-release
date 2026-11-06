# thermal

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `THERMAL` | FILTER |

`THERMAL` 是 GLES 热成像伪彩色模块。它把输入帧的 Y 通道按 256×3 LUT 映射为 YUV，再由 fragment shader 输出 YUV、GRAY8 或 packed RGB。插件是 `FILTER`，端口固定为 `input0` 和 `output0`，实例名为 `THERMAL_<id>`。

配置使用 `MEDIA_THERMAL_ATTR`：`width/height` 是输入输出可见尺寸，`format` 是 8 位 YUV 输入格式，`color_mode` 为 16 种 `MEDIA_THERMAL_COLOR_*` 之一，`input_depth/output_depth` 为队列深度；`input_stride/output_stride` 只用于 LINEAR，压缩布局必须填 0；`input_compress/output_compress` 是精确存储 modifier；`output_format` 选择输出格式（UNSET 默认为 NV12），`output_pool_id` 必须指向容量足够的外部 Pool。

示例：

```c
MEDIA_THERMAL_ATTR a = {0}
a.width = 640; a.height = 480; a.format = MEDIA_FORMAT_NV12
a.color_mode = MEDIA_THERMAL_COLOR_RAINBOW
a.output_format = MEDIA_FORMAT_RGBA8888; a.output_pool_id = pool
MEDIA_THERMAL_CreateGrp(0, &a)
MEDIA_THERMAL_Start(0)
MEDIA_SYS_SendFrame("THERMAL", 0, "input0", input, 1000)
MEDIA_SYS_GetFrame("THERMAL", 0, "output0", &output, 2000)
MEDIA_SYS_ReleaseFrame("THERMAL", 0, "output0", output)
```

旧应用也可以使用同步接口：`MEDIA_THERMAL_Process(0, input, &output)` 会等待一帧
处理完成；成功返回的 `output` 用 `MEDIA_POOL_PutBuffer` 归还。传入 `NULL` 输出指针
时只等待处理完成并把结果送到 `output0`。

输入支持公开 SDK 中已验证的 8 位 YUV 格式：`MEDIA_FORMAT_NV12`、`MEDIA_FORMAT_YUV420SP`、`MEDIA_FORMAT_YCrCb_420SP`、`MEDIA_FORMAT_YUV422SP`、`MEDIA_FORMAT_YUV420P`、`MEDIA_FORMAT_YUV444P` 和 `MEDIA_FORMAT_YUV422_YUYV`；输出支持这些 YUV 格式、`MEDIA_FORMAT_GRAY8` 以及公开 GLES 能力表中的 packed RGB 格式。输入输出尺寸相同，不做缩放，输入端口为 `input0`，输出端口为 `output0`。

输出 metadata 会透传输入 timestamp/seq，并发布真实 `MEDIA_VIDEO_LAYOUT` 和 payload 长度。输入 Pool 必须有足够的 slot 支撑 Bind 队列深度及在途帧数量；`output_pool_id` 必须是已创建、容量至少为 `MEDIA_VIDEO_QueryLayout` 返回值、且每个 slot 都能被 GLES 导入的 Pool。LINEAR 时 stride 为 0 或 canonical pitch，压缩布局时 stride 必须为 0。只支持公开 SDK 的 GLES 路径，弃用旧 Vulkan/OpenCL/CPU fallback；不支持 10 位输入、缩放、多路输入或动态尺寸。

独立构建：

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DMEDIA_SDK_DIR=/root/work/rktohi/wt/sdk
cmake --build build -j2
LD_LIBRARY_PATH=/root/work/rktohi/wt/sdk/lib (cd build && ctest) --output-on-failure
```

## 在发布包中使用

```c
#include "media_thermal.h"
```

- 头文件：`include/media_thermal.h`
- 模块映像：`plugins/libmedia_thermal.so`（与 `libmedia_thermal.so.sig` 必须同目录）
- 链接：`-lmedia_thermal -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_thermal.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("THERMAL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

