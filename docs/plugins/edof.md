# edof

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `EDOF_CL` | FILTER |

扩展景深（Extended Depth Of Field）双路 NV12 融合，GLES 实现。产出 `libmedia_edof.so` 与
[`include/media_edof.h`](../../include/media_edof.h)；`MEDIA_EDOF_CL_*` API 与原内置模块
API 兼容（函数名、参数、`MEDIA_EDOF_CL_ATTR` / `MEDIA_EDOF_CL_PERF`
结构体名与字段顺序完全一致）。

模块 type 名为 `EDOF_CL`，实例名为 `EDOF_CL_<grp>`，因此应用侧的
`MEDIA_SYS_Bind("EDOF_CL", grp, ...)` 与旧的绑定名保持不变。

## 功能

同一个场景在两个焦面各拍一张，把两张图按局部清晰度融合：每张图在每个像素上统计
`(2r+1)×(2r+1)` 窗口内的清晰度分数，清晰度高的那一路在该处权重大；亮度和色度都按该权重融合
（色度以 2×2 亮度块为单位，权重取块左上像素）。算法四趟全部在 GPU 上完成：

1. **sharpness**：逐像素 `s = |4·Yc − Yl − Yr − Yu − Yd|`（Y 为 0..1 归一化码值，边界 clamp）
   两路结果分别写入中间纹理的 R/G 通道（RGBA16F，驱动不支持时退 RGBA32F）
2. **box-sum（水平）**：沿 x 做 `(2r+1)` 窗口平均，为垂直趟留出可累加的中间值
3. **weight**：沿 y 求和得到两路的窗口清晰度 `s0` / `s1`，`denom = s0 + s1 + eps`
   `w0 = s0 / denom`，`diff = |s0 − s1| / denom`；`diff ≥ 0.18` 时直接硬判（取更清晰的一路）
   否则取软权重 `clamp((w0 − 0.5)·2.4 + 0.5, 0, 1)`
4. **fuse**：`Y = w0·Y0 + (1 − w0)·Y1`（`w0` 是 input0 的权重）；U/V 以 2×2 亮度块为单位、
   权重取块左上像素，混合后写入 NV12 渲染目标。

可选 **仿射配准**：input1 采样前先做一次 2D 仿射变换，用于补偿两个焦面之间的轻微位移。
三张中间纹理都按 `width×height` 分配，与场景内容无关。

- 插件类型：FILTER，**自管模式**（插件工作线程自己 `RecvFrame` → 处理 → `EmitFrame`
  输入用 `ReturnFrame` 归还）。
- 生命周期：`MEDIA_GPU_CreateContext` / `MakeCurrent` / `ImportBuffer` / `Finish` /
  `ReleaseCurrent` / `DestroyContext` 全部在插件工作线程内完成，应用侧无需管理 GL 上下文。
  以 C 数组链接进 `.so`；仓库里不放二进制或生成的 `.c`。
- 防逆向：`C_VISIBILITY_PRESET hidden` + `-ffunction-sections -fdata-sections -fno-ident` +
  `--gc-sections` + 构建后 `strip`。动态符号只保留 `media_plugin_entry` 与 10 个
  `MEDIA_EDOF_CL_*`。

## 配置字段（`MEDIA_EDOF_CL_ATTR`）

字段顺序与旧 `media_api.h` 完全一致，新字段只能追加在末尾。

| 字段 | 类型 | 默认 / 取值 | 说明 |
| --- | --- | --- | --- |
| `width` | int | 2..8192，偶数 | 图像宽度，两路输入与输出必须一致。 |
| `height` | int | 2..8192，偶数 | 图像高度，同上。 |
| `format` | int | 只能是 `MEDIA_FORMAT_NV12` | 逻辑像素格式。 |
| `focus_radius` | int | `<=0` → 4 | 清晰度统计窗口半径 r，窗口边长 2r+1；上限 32。 |
| `input_depth` | int | `<=0` → 4 | 两个输入端口的 Bind 队列深度。 |
| `output_pool_id` | int | 必须 `>= 0` | 输出 Buffer 的 Pool。 |
| `input_stride` | int | `0` → 自动 | 输入 Y/UV 平面 pitch；非零须 `>= width` 且 64 字节对齐。 |
| `output_stride` | int | `0` → 自动 | 输出 Y/UV 平面 pitch，约束同上。 |
| `score_eps` | float | `<=0` → `1e-3f` | 权重分母保护值，避免两路都极清晰时除零。 |

## 端口与格式

| 端口 | 方向 | 格式 | 说明 |
| --- | --- | --- | --- |
| `input0` | 输入 | NV12（LINEAR） | 主帧：输出的时间戳、序号取自它。 |
| `input1` | 输入 | NV12（LINEAR） | 副帧：可与 input0 同源场景不同焦面。 |
| `output0` | 输出 | NV12（LINEAR） | 融合结果，Buffer 从 `output_pool_id` 申请。 |

- 每路输入帧必须自带 `MEDIA_VIDEO_LAYOUT`（NV12 两个 plane 的 pitch/offset），插件按帧 meta
  校验尺寸与 pitch，不使用纯 CPU 拷贝。
- 两路输入尺寸、格式、pitch 必须与配置一致；输出 meta 由插件按配置填写（pitch/offset/len/
  seq/timestamp）。
- 端口支持 Bind（`MEDIA_SYS_Bind`，队列深度 = `input_depth`）与手动
  `MEDIA_SYS_SendFrame`，两条路径可混用。

## Pool 要求

- `output_pool_id` 指定的 Pool 必须能容纳一帧 NV12（`MEDIA_VIDEO_BuildLinearLayout` 得到的
  allocation size），且每个 slot 都能被 GPU 导入（DMA-BUF，`MEDIA_GPU_ImportBuffer`）。
- 输出 Pool 至少要留出 **1 个空闲 slot**：插件每帧向 Pool 申请一个输出 Buffer。Pool 里其它
  槽位被应用侧 `GetFrame` 持有时不会阻塞，但全部占满会让处理线程等待。
- 插件自身最多同时持有：**2 路上一帧输入**（用于单路缺帧时补齐）、**当前处理的 2 路输入**、
  以及隔离区里的 **2 路输入 + 1 路输出**（GPU 完成无法确认时暂不归还）。因此输入 Pool 建议
  至少 6 个 slot（last 2 + 当前 2 + 隔离 2），输出 Pool 至少 2 个空闲 slot；自测用 8 slot
  的 Pool。
- 输入 Buffer 由插件在处理完成后 `ReturnFrame` 归还，不需要应用手动回收（手动送帧路径下
  应用调用 `MEDIA_SYS_SendFrame` 后即交出所有权）。

## 使用示例

插件需先被加载：把 `libmedia_edof.so` 所在目录加进 `MEDIA_PLUGIN_PATH`（`MEDIA_SYS_Init`
自动扫描），或在 `MEDIA_SYS_Init` 之后调用 `MEDIA_PLUGIN_Load("/path/libmedia_edof.so")`
同进程内也可以直接调用 `MEDIA_EDOF_CL_CreateGrp`，由插件自己 `MEDIA_PLUGIN_LoadSelf`。

```c
#include "media_api.h"
#include "media_edof.h"

MEDIA_EDOF_CL_ATTR attr = {
    .width = 640
    .height = 360
    .format = MEDIA_FORMAT_NV12
    .focus_radius = 4
    .input_depth = 4
    .output_pool_id = POOL_OUT
    .input_stride = 0
    .output_stride = 0
    .score_eps = 1e-3f
}
MEDIA_SYS_Init
if (MEDIA_EDOF_CL_CreateGrp(0, &attr) != MEDIA_STATUS_OK) {
    /* 配置非法或 Pool 不可用 */
}
if (MEDIA_EDOF_CL_Start(0) != MEDIA_STATUS_OK) {   /* 等价于 Enable */
    /* GLES 上下文创建失败 */
}

/* 方式一：处理链绑定 */
MEDIA_SYS_Bind("TPG", 0, "output0", "EDOF_CL", 0, "input0")
MEDIA_SYS_Bind("TPG", 1, "output0", "EDOF_CL", 0, "input1")
/* 方式二：手动送帧（buf 的所有权交给模块） */
MEDIA_SYS_SendFrame(MEDIA_EDOF_CL_TYPE, 0, "input0", frame0, 0)
MEDIA_SYS_SendFrame(MEDIA_EDOF_CL_TYPE, 0, "input1", frame1, 0)
MEDIA_BUFFER out
if (MEDIA_EDOF_CL_GetFrame(0, &out, 1000) == MEDIA_STATUS_OK) {
    /* out.meta.video 为 NV12 640x360 */
    MEDIA_EDOF_CL_ReleaseFrame(0, out)
}

MEDIA_EDOF_CL_Stop(0)
MEDIA_EDOF_CL_DestroyGrp(0)
MEDIA_SYS_Exit
```

运行期参数走通用入口 `MEDIA_PLUGIN_SetParam(type, id, key, value, size)` /
`MEDIA_PLUGIN_GetParam(type, id, key, value, size)`（`type` 为 `"EDOF_CL"`）：

| key | 类型 | 说明 |
| --- | --- | --- |
| `affine_warp` | `float[6]` | input1 → input0 的仿射矩阵（行优先），从下一帧生效；单位矩阵只取消几何位移，见下方说明。 |
| `focus_radius` | int | 运行期修改窗口半径，范围 1..32。 |
| `score_eps` | float | 运行期修改权重保护值，必须 > 0。 |
| `last_perf` | `MEDIA_EDOF_CL_PERF` | 只读，最近一帧的耗时统计。 |

`MEDIA_EDOF_CL_SetAffineWarp(grp, m)` 等价于写 `affine_warp`；`MEDIA_EDOF_CL_GetLastPerf`
等价于读 `last_perf`。

仿射状态是**粘滞**的：旧模块里 `has_warp` 一旦被 `SetAffineWarp` 置位就不再复位，本插件保持
同一语义。旧 OpenCL 的线性采样在非归一化坐标下把纹素中心放在整数+0.5 处（已在板端实测）
所以配准生效后即使传单位矩阵，input1 也会经过一次半像素的双线性重采样——只是没有几何位移。
要回到"完全不重采样 input1"的状态，需要重建实例。

旧模块的 `focus_radius` / `input_depth` / `score_eps` 默认值语义原样保留：创建时
`<=0` 收敛到默认值，之后 `GetParam` 读到的是实际生效值。

## 限制

- 只有 GLES 后端；旧模块的 OpenCL / Vulkan / NPU / CPU fallback 都没有移植。
- 只支持 NV12、LINEAR、`compress = 0`；不支持 AFBC、其他 YUV/RGB 格式、多路（>2）输入。
- `focus_radius` 上限 32（见 `MEDIA_EDOF_CL_MAX_FOCUS_RADIUS`）：更大窗口在 GLES 盒式求和
  实现里耗时随 r 线性增长，收益很小。
- `MEDIA_EDOF_CL_PERF.gpu_queue_total_ms` 恒为 `-1.0`：GLES 后端没有 OpenCL event 那种
  acquire→release 时间跨度。`gpu_kernel_total_ms` 是包含 4 趟绘制与 `Finish` 的墙钟时间
  不是逐 kernel 累计。
- 中间精度：清晰度/权重纹理为 RGBA16F（不可用时 RGBA32F）。板端合成用例的实测最大误差为
  `|dY| ≤ 1`、`|dUV| ≤ 1` 码值；极低对比度区域（权重处在软/硬阈值附近）误差会放大，本插件
  不保证与旧 OpenCL 的 FP32 结果逐位一致。
- 两路输入没有同时到达时，插件用上一帧补齐；上一帧也不存在时丢弃当前帧（旧模块语义一致）。
- 单路缺帧补齐与 `input_depth` 队列深度都在插件侧实现，应用侧无法感知哪一帧用了上一帧。
- GPU 完成无法确认时，本帧 buffer 进入隔离区并停止处理新帧，`Stop` 返回
  `MEDIA_ERR_RESOURCE`；此时必须 `DestroyGrp` 后重建实例才能继续。

## 在发布包中使用

```c
#include "media_edof.h"
```

- 头文件：`include/media_edof.h`
- 模块映像：`plugins/libmedia_edof.so`（与 `libmedia_edof.so.sig` 必须同目录）
- 链接：`-lmedia_edof -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_edof.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("EDOF_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

