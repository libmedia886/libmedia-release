# wavelet_nr

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `WAVELET_NR_CL` | FILTER |

NV12 亮度小波空间降噪与色度平滑，纯 GLES 实现。产出 `libmedia_wavelet_nr.so` 与
[`include/media_wavelet_nr.h`](../../include/media_wavelet_nr.h)；`MEDIA_WAVELET_NR_CL_*` API 的
函数名、参数、结构体名与字段顺序与旧 `include/media_api.h` 完全一致（源码级兼容），实例名为
`WAVELET_NR_CL_<grp>`。

## 功能

- 亮度：逐 2x2 块做 Haar 分解 `ll/hl/lh/hh`，对 `hl/lh/hh` 做软阈值收缩，再按像素相位重建。
- 二级模式（`levels=2`）：额外用 4x4 局部均值把 `ll` 往低频方向拉回，抑制两层分解的块状漂移。
- 色度：可选 5 列 x 3 行加权平滑（水平采样步长 2 像素，权重 `2/1` 行、`2/1` 列，与旧 kernel 一致）。
- 直通：`passthrough != 0` 时跳过算法；输出池与输入同池时零拷贝转发，异池时按可见区域 CPU 拷贝。
- 每帧透传输入 `time_stamp` / `seq`，输出真实 NV12 线性布局与 `len`。

## type 名与端口

| 项目 | 值 |
| --- | --- |
| 插件类型名（`MEDIA_SYS_Bind` 的 mod 参数） | `WAVELET_NR_CL` |
| 实例名 | `WAVELET_NR_CL_<grp>` |
| 插件类 | `MEDIA_PLUGIN_KIND_FILTER`，自管模式（`process = NULL`） |
| 输入端口 | `input0`：`MEDIA_FORMAT_NV12` + `MEDIA_VIDEO_COMPRESS_NONE`，宽高 = 配置宽高 |
| 输出端口 | `output0`：`MEDIA_FORMAT_NV12` + `MEDIA_VIDEO_COMPRESS_NONE`，宽高 = 配置宽高 |

输入/输出宽高必须 > 0 且为偶数；只支持线性（非 AFBC）NV12。

## 配置字段（`MEDIA_WAVELET_NR_CL_ATTR`）

字段顺序与旧 API 一致。`<= 0` 或 `<= 0.0f` 的取值表示取默认值/关闭，语义沿用旧模块。

| 字段 | 类型 | 默认 | 范围/语义 |
| --- | --- | --- | --- |
| `width` | `int` | 必填 | 图像宽度，> 0 且偶数 |
| `height` | `int` | 必填 | 图像高度，> 0 且偶数 |
| `format` | `int` | 必填 | 仅 `MEDIA_FORMAT_NV12` |
| `input_depth` | `int` | 4 | 输入端口队列深度，`<= 0` 取 4 |
| `output_pool_id` | `int` | 必填 | 输出 Pool ID；`< 0` 表示复用每帧输入所在 Pool |
| `input_stride` | `int` | 宽度按 64 字节对齐 | 输入 plane0/plane1 字节 pitch，`<= 0` 自动推导 |
| `output_stride` | `int` | 沿用 `input_stride` | 输出 plane0/plane1 字节 pitch，`<= 0` 沿用输入 pitch |
| `levels` | `int` | 1 | 分解层数，钳制到 1..2 |
| `threshold_y` | `float` | 6/255 | 亮度高频软阈值（归一化 0..1），钳制到 0..0.25 |
| `strength` | `float` | 0.60 | 高频收缩强度，钳制到 0..1 |
| `uv_enable` | `int` | 0 | 非 0 启用色度平滑 |
| `uv_strength` | `float` | 0 | 色度平滑混合强度，钳制到 0..1 |
| `passthrough` | `int` | 0 | 非 0 时跳过算法 |

运行期可通过 `MEDIA_WAVELET_NR_CL_SetLevels / SetThresholdY / SetStrength / SetUvEnable /
SetUvStrength / SetPassthrough` 调整；`MEDIA_WAVELET_NR_CL_GetLastPerf` 返回单帧耗时
（`cpu_ms` 为整帧处理耗时，`gpu_kernel_ms` 为提交绘制到 `MEDIA_GPU_Finish` 返回的耗时
`gpu_queue_ms` 为 `Finish` 等待 GPU 完成的耗时，`gpu_enabled == 1` 表示 GLES 后端已启用）。

## Pool 要求

- Pool 必须是可被 `MEDIA_GPU_ImportBuffer` 导入的 DMA-BUF 线性 NV12 池，推荐
  `MEDIA_POOL_CreateWithMode(id, size, count, MEDIA_POOL_ALLOC_MODE_NATIVE)`；HOST 池未经验证，不支持。
- 输出 Pool 每个 buffer 容量 ≥ `output_stride * height * 3 / 2`（Y 平面 `output_stride*height`
  UV 平面 `output_stride*height/2`）。
- 输出 Pool 的 count 要覆盖输入队列深度与下游在途：建议 ≥ `input_depth + 下游队列深度 + 2`
  板端测试 640x360 用 6、64x64 用 4。
- `output_pool_id < 0` 时输出取自输入帧所在 Pool，该 Pool 需多留至少 1 个 slot（输入帧在处理期间仍被持有）。
- 输入帧布局以逐帧 metadata 为准：`plane_count == 2`、visible 尺寸与配置一致、两 plane pitch ≥ 宽度。

## 使用示例

```c
#include "media_api.h"
#include "media_wavelet_nr.h"

MEDIA_VIDEO_LAYOUT layout
size_t size = 0
MEDIA_WAVELET_NR_CL_ATTR attr = {0}
MEDIA_WAVELET_NR_CL_PERF perf
MEDIA_BUFFER frame
MEDIA_VIDEO_QueryLayout(640, 360, MEDIA_FORMAT_NV12, MEDIA_VIDEO_COMPRESS_NONE, &layout, &size)
MEDIA_POOL_CreateWithMode(10, size, 8, MEDIA_POOL_ALLOC_MODE_NATIVE); /* 输入 */
MEDIA_POOL_CreateWithMode(11, size, 6, MEDIA_POOL_ALLOC_MODE_NATIVE); /* 输出 */

attr.width = 640
attr.height = 360
attr.format = MEDIA_FORMAT_NV12
attr.output_pool_id = 11
attr.levels = 2
attr.threshold_y = MEDIA_WAVELET_NR_CL_DEFAULT_THRESHOLD_Y
attr.strength = MEDIA_WAVELET_NR_CL_DEFAULT_STRENGTH
attr.uv_enable = 1
attr.uv_strength = 0.5f
MEDIA_WAVELET_NR_CL_CreateGrp(0, &attr)
MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_WAVELET_NR_CL_TYPE, 0, "input0")
MEDIA_WAVELET_NR_CL_Enable(0);            /* 等价 Start，在插件线程建 GLES 上下文 */
MEDIA_WAVELET_NR_CL_GetFrame(0, &frame, 1000)
/* 使用 frame，然后归还 */
MEDIA_WAVELET_NR_CL_ReleaseFrame(0, frame)
MEDIA_WAVELET_NR_CL_GetLastPerf(0, &perf)
MEDIA_WAVELET_NR_CL_Disable(0);           /* 等价 Stop，join 工作线程并销毁上下文 */
MEDIA_WAVELET_NR_CL_DestroyGrp(0)
```

也可以用 `MEDIA_WAVELET_NR_CL_SendFrame` 手动送帧，再用 `GetFrame` / `ReleaseFrame` 取回输出。

## 限制

- 只有 GLES 后端；无 OpenCL/Vulkan/NPU/CPU 算法回退，GPU 上下文或 YUV 扩展不可用时 `Start`/`Enable` 失败。
- 只接受线性 NV12（含 AFBC 的输入/输出不支持），宽高必须为偶数。
- `levels` 只有 1 与 2 两档；更大值钳制为 2。
- 依赖 `GL_EXT_YUV_target` 与 `GL_OES_EGL_image_external_essl3`；缺失时明确报错。
- program binary 与驱动/GPU 绑定，跨设备或跨驱动版本需重新构建。
- 直通且 `output_pool_id < 0` 时直接转发同一 buffer，输出 pitch/布局即输入布局（不做重排）。

## 在发布包中使用

```c
#include "media_wavelet_nr.h"
```

- 头文件：`include/media_wavelet_nr.h`
- 模块映像：`plugins/libmedia_wavelet_nr.so`（与 `libmedia_wavelet_nr.so.sig` 必须同目录）
- 链接：`-lmedia_wavelet_nr -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_wavelet_nr.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("WAVELET_NR_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

