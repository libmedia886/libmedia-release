# clahe

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `CLAHE` | FILTER |

GLES 局部对比度增强（Contrast Limited Adaptive Histogram Equalization）。产出
`libmedia_clahe.so` 与 [`include/media_clahe.h`](../../include/media_clahe.h)；`MEDIA_CLAHE_*` API 与原内置模块
源码级兼容（函数名、参数、`MEDIA_CLAHE_ATTR` 字段顺序完全一致），模块 type 名为 `CLAHE`
实例名 `CLAHE_<grp>`。

## 功能

- 单通道 NV12 输入，按 tile 网格做对比度受限直方图均衡，再对相邻 tile 的 LUT 做双线性混合
  避免块状边界。
- 可选高光保护：亮度超过阈值后逐步退回原始亮度，抑制亮部过曝与噪声放大。
- 可选直通（passthrough）：不消耗 GPU、不申请输出 Pool，直接把输入 Buffer 交给输出端口。
- 运行期可改 `clip_limit`、高光保护参数与直通开关，无需 Stop/Start。

算法与旧内置模块的 OpenCL kernel（）逐式对应，只是改成 GLES
渲染管线实现。

## 设计

- FILTER 插件、**自管模式**：插件工作线程 `RecvFrame` 取帧，`MEDIA_POOL_GetBuffer` 申请输出
  跑完 GPU 管线后 `EmitFrame` 输出、`ReturnFrame` 归还输入。GPU 上下文
  （`MEDIA_GPU_CreateContext`）在该线程创建、使用和销毁，不使用 `process` 回调。
- **五步渲染**（全部在同一个 EGL 上下文里，每帧 5 次 draw）：
  1. `luma`：NV12 的 Y 平面（外部 OES 纹理）按像素中心采样，写入 R32F 亮度纹理
  2. `hist`：按 `W*H` 个点绘制，每个点固定输出 1，用 `GL_ONE/GL_ONE` 加法混合把每个像素
     累加到直方图纹理的 `(bin, tile)` 纹素上
  3. `tile-info`：每个 tile 一个片元，复刻 `kernel_clahe_clip_build_lut` 的裁剪上限与 excess
     均摊量（`limit / add_all / remainder / area`）
  4. `clip-lut`：每个 `(tile, bin)` 一个片元，按旧 kernel 的“先裁剪、再摊回、最后 CDF”生成整型
     LUT，用 R32F 保存以免 8 位中间量舍入
  5. `apply`：对每个像素双线性混合四个相邻 tile 的 LUT，施加可选高光保护，写 NV12 输出；U/V
     在 2x2 色度样本中心读取后原样写回。
  `MEDIA_GPU_LoadProgramBinary` 加载，shader 源码不进入发布包。
- **隔离（quarantine）**：`MEDIA_GPU_Finish` 失败时无法确认 GPU 是否仍在使用这两块内存，此时
  输入/输出都留在插件里，之后不再处理新帧，等 Stop/线程退出、上下文销毁后才归还 Pool。
- **防逆向**：`C_VISIBILITY_PRESET hidden`、strip、`-ffunction-sections -fdata-sections -fno-ident`、
  `--gc-sections`；`libmedia_clahe.so` 只导出 `media_plugin_entry` 与 11 个 `MEDIA_CLAHE_*`。

## 端口与格式

| 端口 | 方向 | 格式 | 说明 |
| --- | --- | --- | --- |
| `input0` | 输入 | NV12（LINEAR） | 有效尺寸必须等于配置的 `width x height` |
| `output0` | 输出 | NV12（LINEAR） | 从 `output_pool_id` 指定的 Pool 申请 |

Bind 示例：`MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_CLAHE_TYPE, grp, "input0")`。

## 配置字段

`MEDIA_CLAHE_ATTR`（字段顺序与旧 `media_api.h` 一致，新字段只能追加到末尾）：

| 字段 | 类型 | 默认/取值 | 说明 |
| --- | --- | --- | --- |
| `width` | `int` | 必须显式给出 | 图像宽度，偶数，16..8192 |
| `height` | `int` | 必须显式给出 | 图像高度，偶数，16..8192 |
| `format` | `int` | `MEDIA_FORMAT_NV12` | 只支持 NV12 |
| `tile_grid_x` | `int` | `<=0` → 8 | 水平分块数，1..255 且不超过 `width` |
| `tile_grid_y` | `int` | `<=0` → 8 | 垂直分块数，1..255 且不超过 `height` |
| `bins` | `int` | `<=0` → 256 | 只支持 256 个 bin，其它正值创建失败 |
| `input_depth` | `int` | `<=0` → 4 | `input0` 输入队列深度 |
| `output_pool_id` | `int` | 必须 `>=0` | 输出 Buffer 的来源 Pool |
| `input_stride` | `int` | `0` → canonical pitch | Y 平面 pitch，非 0 时必须是 64 的倍数且不小于 canonical |
| `output_stride` | `int` | `0` → canonical pitch | 输出 Y/UV plane pitch，同上 |
| `clip_limit` | `float` | `<=0` → 2.0 | 直方图裁剪系数，越大对比度越强、噪声放大越明显 |
| `highlight_protect_start` | `float` | `<=0` 关闭 | 高光保护起始**归一化**亮度（0..1，0.5 ≈ code 128） |
| `highlight_protect_strength` | `float` | `<=0` → 1.0 | 高光保护强度（0..1，1.0 = 完全回到原始亮度） |
| `passthrough` | `int` | 0 | 非 0 直通输出输入 Buffer |

`tiles_x * tiles_y` 上限 1024；`tile_grid_x` 最多 255（沿用旧模块用 `uchar` 存 tile 下标的限制）。

## 运行期参数

`MEDIA_PLUGIN_SetParam/GetParam(MEDIA_CLAHE_TYPE, grp, key, ...)`，也有 `MEDIA_CLAHE_Set*` 便捷封装：

| key | 类型 | 说明 |
| --- | --- | --- |
| `clip_limit` | `float` | 必须 `> 0`，否则返回 `MEDIA_ERR_INVALID_PARAM` |
| `highlight_protect_start` | `float` | 归一化起始亮度，可随时改 |
| `highlight_protect_strength` | `float` | 0 关闭高光保护 |
| `passthrough_mode` | `int` | 非 0 直通 |

未知 key 返回 `MEDIA_ERR_UNSUPPORTED`。参数读取/写入都在插件内部互斥量下完成，工作线程每帧取
一次快照，所以同一帧内参数一致。

## Pool 要求

- `output_pool_id` 必须指向一个至少能容纳一帧 NV12（`stride * height * 3 / 2`）的 Pool；插件在
  每帧申请时用 `MEDIA_POOL_GetSize` 复核容量，不足则丢帧并计数。
- 输入帧 metadata 必须是有效的 `MEDIA_BUFFER_META_VIDEO_FRAME`、`media_format == NV12`，且
  `visible_width/height` 与配置一致、`offset_bytes[1] + pitch_bytes[1] * height/2 <= len`。
- 直通模式不申请输出 Pool，可以把 `output_pool_id` 配成一个 1 slot 的占位 Pool。
- 插件不持有 Buffer：正常路径每帧 Put 回输出 Pool 的引用交给下游、输入通过 `ReturnFrame` 还给
  框架；Stop/Destroy 后 Pool slot 计数应完全复原（`MEDIA_POOL_Destroy` 成功即证明无泄漏）。

## 使用示例

```c
#include "media_api.h"
#include "media_clahe.h"

MEDIA_CLAHE_ATTR attr = { 0 }
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.tile_grid_x = 8;          /* 0 则使用默认 8 */
attr.tile_grid_y = 8
attr.bins = 256
attr.input_depth = 4
attr.output_pool_id = out_pool
attr.clip_limit = 2.0f
attr.highlight_protect_start = 0.9f;   /* 0 表示关闭 */
attr.highlight_protect_strength = 0.8f
if (MEDIA_CLAHE_CreateGrp(0, &attr) != MEDIA_STATUS_OK) { /* 处理错误 */ }
MEDIA_SYS_Bind("VPSS", 0, "output0", MEDIA_CLAHE_TYPE, 0, "input0")
MEDIA_CLAHE_Start(0)
/* 拉帧 */
MEDIA_BUFFER frame
if (MEDIA_CLAHE_GetFrame(0, &frame, 1000) == MEDIA_STATUS_OK) {
    /* frame.pool_id / frame.index 指向 output_pool 的某块 NV12 */
    MEDIA_CLAHE_ReleaseFrame(0, frame)
}

/* 运行期调参 */
MEDIA_CLAHE_SetClipLimit(0, 3.0f)
MEDIA_CLAHE_SetHighlightProtect(0, 0.85f, 1.0f)
MEDIA_CLAHE_SetPassthrough(0, 0)
MEDIA_CLAHE_Stop(0)
MEDIA_CLAHE_DestroyGrp(0)
```

手动送帧（不经 Bind）用 `MEDIA_PLUGIN_SendFrame(MEDIA_CLAHE_TYPE, grp, 0, buf, timeout_ms)`
Buffer 需先 `MEDIA_BUFFER_SetMeta` 成 NV12 视频帧。

## 限制

- 只支持 NV12 单输入单输出；不支持 RAW Bayer、多路输入、10bit 或 RGB。
- 只走 GLES。旧模块的 OpenCL/Vulkan/NPU 后端与 CPU fallback 都没有移植。
- 需要 `GL_OES_EGL_image_external_essl3`、`GL_EXT_YUV_target`、`GL_EXT_color_buffer_float`、
  `GL_EXT_float_blend`；缺失时 Start 直接失败（`MEDIA_ERR_RESOURCE`）。
- program binary 与驱动版本绑定，必须在板上构建；不支持运行时编译 shader。
- 直方图用点图元 + 加法混合累计，没有 compute shader / atomic；分辨率越高越吃带宽
  1080p 以上建议先 benchmark。
- 每帧 5 次 draw + 1 次 `MEDIA_GPU_Finish`，没有做多帧流水叠加。

移植过程、丢弃内容、行为差异与 SDK 缺口

## 在发布包中使用

```c
#include "media_clahe.h"
```

- 头文件：`include/media_clahe.h`
- 模块映像：`plugins/libmedia_clahe.so`（与 `libmedia_clahe.so.sig` 必须同目录）
- 链接：`-lmedia_clahe -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_clahe.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("CLAHE", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

