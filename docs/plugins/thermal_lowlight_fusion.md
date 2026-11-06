# thermal_lowlight_fusion

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `THERMAL_LOWLIGHT_FUSION_CL` | FILTER |

GLES 双路 NV12 图像融合：以低照度图像为亮度基准，从热成像图像中提取局部细节注入亮度
并可按热成像热点区域叠加红/黑色调。产出 `libmedia_thermal_lowlight_fusion.so` 与
[`include/media_thermal_lowlight_fusion.h`](../../include/media_thermal_lowlight_fusion.h)。

模块 type 名沿用旧内置模块的 `THERMAL_LOWLIGHT_FUSION_CL`，实例名为
`THERMAL_LOWLIGHT_FUSION_CL_<grp>`，因此应用侧 `MEDIA_SYS_Bind` 的模块名不变。
`MEDIA_THERMAL_LOWLIGHT_FUSION_CL_*` 的函数名、参数、结构体名与字段顺序与旧
`include/media_api.h` 完全一致，可源码级替换。

## 端口与数据流

| 方向 | 端口 | 格式 | 说明 |
| --- | --- | --- | --- |
| 输入 0 | `input0` | NV12（LINEAR） | 热成像图像；提供细节、热点判定、输出时间戳与序号 |
| 输入 1 | `input1` | NV12（LINEAR） | 低照度图像；提供融合基准亮度 |
| 输出 0 | `output0` | NV12（LINEAR） | 融合结果，来自 `output_pool_id` 指定的 Pool |

两路输入必须同宽同高，并与 `attr.width/height` 一致；第一路（热成像）到达后 100ms
内未收到配对的低照度帧时，该热成像帧被丢弃，等待下一次配对。输出时间戳、序号取自热成像帧。

## 处理流程

1. **extract**：两路 NV12 的 Y 平面各采样一次（`samplerExternal2DY2YEXT`，只取 Y 码值
   不做色彩转换），写入 RG16F 中间纹理：R = 热成像 Y，G = 低照度 Y。
2. **box H / box V**：对 R/G 通道做可分离 box 滤波，得到两路各自的局部基底，边界 clamp-to-edge。
   半径由 `detail_base_radius` 决定，窗口为 `2r+1`。
3. **output**：一次绘制写满 NV12 输出（`layout(yuv)`，R=Y、G=U、B=V）：
   - `luma_mode == LOWLIGHT`：Y 直接取低照度 Y。
   - `luma_mode == THERMAL_DETAIL`：
     `thermal_detail = thermalY - baseThermal`，`lowlight_detail = lowlightY - baseLowlight`
     `thermal_mask = smoothstep(t, 2.5t, |thermal_detail|)`
     `missing = 1 - smoothstep(0.75t, 2t, |lowlight_detail|)`
     `dominance = smoothstep(t, 3t, |thermal_detail| - |lowlight_detail|)`
     `mask = thermal_mask * clamp(0.7*missing + 0.3*dominance, 0, 1)`
     `delta = clamp(detail_gain * mask * thermal_detail, -max_luma_delta, +max_luma_delta)`
     `Y = clamp(lowlightY + delta, 0, 1)`，其中 `t = detail_threshold`（最小 1/255）。
   - `mode == GRAY`：UV 恒为 128（中性灰）。
   - `mode == HOT_OVERLAY`：对热成像 Y 取 2x2 均值 `hot`
     `mask = smoothstep(hot_threshold - w/2, hot_threshold + w/2, hot)`
     `alpha = mask * overlay_alpha`，红调 `U = 0.5 - 0.169*hot`、`V = 0.5 + 0.5*hot`
     与中性灰按 `alpha` 混合。

算法语义按旧模块 OpenCL kernel 逐像素重建（旧目录里的 GLES 文件本身是空 stub）。

## 配置字段

结构体 `MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ATTR`（详见公共头文件，字段顺序与旧 API 一致）：

| 字段 | 类型 | 含义与取值 |
| --- | --- | --- |
| `width` | int | 可见宽度，2..8192 且为偶数（输入输出共用） |
| `height` | int | 可见高度，2..8192 且为偶数（输入输出共用） |
| `format` | int | 必须为 `MEDIA_FORMAT_NV12` |
| `input_depth` | int | 输入队列深度，`<=0` 取 4，上限 64 |
| `output_pool_id` | int | 输出 Buffer 的 Pool id，必须 `>= 0` |
| `input_stride` | int | 输入 plane0 行字节，`>= width` 且 64 字节对齐 |
| `output_stride` | int | 输出 plane0 行字节，`>= width` 且 64 字节对齐 |
| `mode` | int | `MODE_GRAY(0)` 或 `MODE_HOT_OVERLAY(1)`；`MODE_BLACK_RED` 是 `HOT_OVERLAY` 的旧别名 |
| `thermal_weight` | float | 旧热成像权重，取值 (0,1]，非法取 0.5；当前路径不使用，仅兼容 |
| `hot_threshold` | float | 热点阈值，取值 (0,1)，非法取 0.65 |
| `hot_soft_width` | float | 热点过渡宽度，取值 (0,1]，非法取 0.15（最小 1/255） |
| `overlay_alpha` | float | 伪彩叠加透明度，取值 (0,1]，非法取 0.70 |
| `pyramid_levels` | int | 0 取 2，允许 1..4；已废弃，仅兼容 |
| `algo` | int | 必须为 `ALGO_PYRAMID(0)`；`ALGO_TIF(1)` 在创建时返回 `MEDIA_ERR_UNSUPPORTED` |
| `tif_base_radius` | int | 0 取 8，允许 1..32；已废弃，仅兼容 |
| `luma_mode` | int | `LUMA_LOWLIGHT(0)` 或 `LUMA_THERMAL_DETAIL(1)` |
| `detail_gain` | float | 细节增益，取值 (0,1]，非法取 0.35 |
| `detail_threshold` | float | 归一化细节阈值，取值 (0,1)，非法取 0.035 |
| `max_luma_delta` | float | 单像素 Y 最大注入量，取值 (0,1]，非法取 0.12 |
| `detail_base_radius` | int | 局部基底 box 半径，0 取 6，允许 1..32 |

非法字段在 `CreateGrp` 阶段返回 `MEDIA_ERR_INVALID_PARAM`，不会留下半成品实例
浮点字段为 0 或越界时按上表取缺省值（与旧 glue 的归一化行为一致）。

## Pool 要求

- 输出必须使用调用方提供的 Pool（`output_pool_id`），插件按 `output_stride * height * 3 / 2`
  计算所需容量。创建时若能查到 Pool 信息（`MEDIA_POOL_GetInfo`），Pool 不存在、slot 数为 0
  或 slot 容量不足都会直接返回 `MEDIA_ERR_INVALID_PARAM`
  若 Pool 在创建时不存在，创建会成功，但处理线程取不到 buffer，运行期报错并计入 `fail` 统计。
- 输入 Buffer 由上游或 `SendFrame` 提供，插件只借用，处理完成后归还引用。
- `GetFrame` 得到的输出 buffer 必须用 `ReleaseFrame` 归还；Stop/Destroy 不会替你释放应用已取走的帧。

## 使用示例

```c
#include "media_api.h"
#include "media_thermal_lowlight_fusion.h"

MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ATTR attr = {0}
MEDIA_BUFFER out
MEDIA_SYS_Init
attr.width = 1280
attr.height = 720
attr.format = MEDIA_FORMAT_NV12
attr.output_pool_id = out_pool
attr.input_stride = 1280
attr.output_stride = 1280
attr.mode = MEDIA_THERMAL_LOWLIGHT_FUSION_CL_MODE_HOT_OVERLAY
attr.luma_mode = MEDIA_THERMAL_LOWLIGHT_FUSION_CL_LUMA_THERMAL_DETAIL
MEDIA_THERMAL_LOWLIGHT_FUSION_CL_CreateGrp(0, &attr)
/* 上游驱动：TPG 0 -> input0（热成像），TPG 1 -> input1（低照度）。 */
MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_THERMAL_LOWLIGHT_FUSION_CL_TYPE, 0
               MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_THERMAL_PORT)
MEDIA_SYS_Bind("TPG", 1, "output0", MEDIA_THERMAL_LOWLIGHT_FUSION_CL_TYPE, 0
               MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_LOWLIGHT_PORT)
MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Start(0)
if (MEDIA_THERMAL_LOWLIGHT_FUSION_CL_GetFrame(0, &out, 1000) == MEDIA_STATUS_OK) {
    /* 消费 out ... */
    MEDIA_THERMAL_LOWLIGHT_FUSION_CL_ReleaseFrame(0, out)
}

MEDIA_THERMAL_LOWLIGHT_FUSION_CL_Stop(0)
MEDIA_THERMAL_LOWLIGHT_FUSION_CL_DestroyGrp(0)
```

未接上游时可直接送帧：

```c
MEDIA_THERMAL_LOWLIGHT_FUSION_CL_SendFrame(0, MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_THERMAL
                                           thermal_buf, 0)
MEDIA_THERMAL_LOWLIGHT_FUSION_CL_SendFrame(0, MEDIA_THERMAL_LOWLIGHT_FUSION_CL_INPUT_LOWLIGHT
                                           lowlight_buf, 0)
```

分阶段耗时通过 `MEDIA_THERMAL_LOWLIGHT_FUSION_CL_GetLastPerf` 读取（`y_kernel_ms`、
`uv_kernel_ms`、`gpu_total_ms`）。

## 限制

- 只支持 NV12 + LINEAR；不支持裁剪、缩放、旋转、AFBC 与高位深。
- 只实现 GLES 后端。旧模块的 OpenCL/Vulkan/NPU/CPU fallback 与 TIF、金字塔算法路径均未移植
  `algo = TIF` 会被拒绝，`pyramid_levels`、`tif_base_radius`、`thermal_weight` 仅保留字段兼容。
- 两路输入必须尺寸一致，且与属性中的宽高一致；插件不做缩放对齐。
- GLES 上下文在 Start 时由插件私有线程创建，Stop/Destroy 时销毁；运行中不要在其他线程调用 GL。
- 性能数据是墙钟时间 + `MEDIA_GPU_Finish` 等待，不是硬件 timer query。
- 输出色度按 NV12 2x2 下采样，热点边界存在半像素级的色度过渡，属于预期行为。

## 在发布包中使用

```c
#include "media_thermal_lowlight_fusion.h"
```

- 头文件：`include/media_thermal_lowlight_fusion.h`
- 模块映像：`plugins/libmedia_thermal_lowlight_fusion.so`（与 `libmedia_thermal_lowlight_fusion.so.sig` 必须同目录）
- 链接：`-lmedia_thermal_lowlight_fusion -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_thermal_lowlight_fusion.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("THERMAL_LOWLIGHT_FUSION_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

