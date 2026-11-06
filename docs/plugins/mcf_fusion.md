# mcf_fusion

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `MCF_FUSION_CL` | FILTER |

`mcf_fusion` 是 libmedia 的 GLES 单色/彩色图像融合插件，产出
`libmedia_mcf_fusion.so` 与公开头文件 `include/media_mcf_fusion.h`。插件实例的 type 名为
`MCF_FUSION_CL`，保留旧模块的 `MEDIA_MCF_*` 与 `MEDIA_MCF_FUSION_CL_*` 应用 API。

插件只依赖公开 SDK 的 `media_api.h`、`media_plugin.h` 和 EGL/GLESv2；不包含 core 内部头文件
也不提供 OpenCL、Vulkan、NPU 或 CPU 软件回退。

## 功能

- 两路输入融合：`input0` 为彩色 NV12，`input1` 为单色 GRAY8 或 NV12（只读取 Y 平面）。
- 三种算法：`LOCAL_REALTIME`、`MULTIBAND`、`LEGACY_SINGLE_SCALE`。
- 三种输出路径：彩色直通、单色直通、融合。
- 可选均值/标准差归一化，将单色亮度对齐到彩色 Y 的统计分布。
- 按配准置信度平滑过渡到彩色直通；低于 `fallback_threshold` 时完全直通。
- 可选时间戳配对与缺帧复用：`reuse_last_frame` 开启时，缺一路可复用该路上一帧。
- 自管工作线程：GPU 上下文创建、绘制、销毁均发生在插件线程内，不占用调用线程的 EGL 状态。
- 按 Pool slot、plane 几何和 Pool 代次缓存 EGLImage/纹理/FBO，循环复用时无需逐帧重新导入。
- 运行统计：`MEDIA_MCF_GetLastPerf` 可读取各 pass 耗时、处理帧数、配对/丢帧/复用/回退计数。

## 端口与格式

| 端口 | 方向 | 语义 | 格式 |
| --- | --- | --- | --- |
| `input0` | 输入 | 彩色路 | `MEDIA_FORMAT_NV12`，使用 Y 与 UV |
| `input1` | 输入 | 单色/近红外路 | `MEDIA_FORMAT_GRAY8` 或 `MEDIA_FORMAT_NV12`，只使用 Y |
| `output0` | 输出 | 融合结果 | `MEDIA_FORMAT_NV12` |

所有输入和输出必须具有相同有效尺寸，宽、高均为偶数，范围为 2..8192。LINEAR pitch 必须
不小于有效宽度、按 64 字节对齐，且不超过 32768。`input1` 为 NV12 时其 UV 平面会被忽略。

输出 Y 的语义如下：

- `PATH_COLOR`：逐像素复制 `input0` 的 Y。
- `PATH_MONO`：输出归一化或未归一化的 `input1` Y，UV 固定写 128。
- `PATH_FUSION`：按算法把单色细节注入彩色 Y；UV 沿用 `input0` 的 UV。

## 配置结构

### `MEDIA_MCF_PIPE_ATTR`

| 字段 | 说明 |
| --- | --- |
| `pipe_id` | 必须为 `MEDIA_MCF_PIPE_MONO` 或 `MEDIA_MCF_PIPE_COLOR`，并与端口对应 |
| `width`, `height` | 有效尺寸，必须为偶数；两路必须一致 |
| `format` | 彩色路只接受 NV12；单色路接受 GRAY8/NV12 |
| `stride` | 行字节数，0 时按宽度向上对齐到 64；非 0 时必须 64 字节对齐 |
| `depth` | 输入队列深度，0 表示缺省 4，显式范围 1..64 |

### `MEDIA_MCF_GRP_ATTR`

| 字段 | 说明 |
| --- | --- |
| `sync_pipe_en` | 1 时比较两路时间戳；差值超过容差时丢弃较旧帧 |
| `mono_pipe_attr` | `input1` 属性 |
| `color_pipe_attr` | `input0` 属性 |
| `depth` | 分组级队列深度，0 表示缺省 4 |
| `path` | `MEDIA_MCF_PATH_FUSION`、`MEDIA_MCF_PATH_COLOR` 或 `MEDIA_MCF_PATH_MONO` |
| `timestamp_tolerance_us` | 配对容差，0 表示 2000us；显式范围 100..16667us |
| `reuse_last_frame` | 1 时允许复用缺失一路的上一帧，0 时等待成对输入 |

两路时间戳都为 0 时不做差值比较，只累计 `missing_timestamp_count`。开启复用后，插件会通过
`MEDIA_PLUGIN_RetainFrame` 持有一路上一帧，直到被新帧替换或 Stop/Destroy。

### `MEDIA_MCF_CHN_ATTR`

| 字段 | 说明 |
| --- | --- |
| `width`, `height` | 必须等于输入有效尺寸；0 时继承输入尺寸 |
| `format` | 只支持 NV12；0 在 `SetChnAttr` 中按 NV12 处理 |
| `stride` | 输出 pitch，0 时按宽度向上对齐到 64 |
| `depth` | 输出队列深度，0 表示缺省 4 |
| `output_pool_id` | 输出 Pool ID，Start 前必须已创建 |

第一版只支持通道 0。通道属性、算法静态字段和分组形状只能在 Stop 后修改；运行中
`MEDIA_MCF_SetAlgParam` 只允许更新 `registration_confidence`。

### `MEDIA_MCF_ALG_PARAM`

| 字段 | 缺省值 | 说明 |
| --- | --- | --- |
| `mode` | `LOCAL_REALTIME` | 选择 LOCAL / MULTIBAND / LEGACY |
| `normalize_mode` | `MEAN_STD` | `NONE` 或 `MEAN_STD` |
| `local_window` | 5 | 当前固定为 5 |
| `local_map_sigma` | 0.8 | 当前固定为 0.8 |
| `local_detail_sigma` | 3.0 | 当前固定为 3.0 |
| `local_detail_gain` | 0.90 | LOCAL 细节增益，范围 0..16 |
| `hf_cfg.filter_sigma` | 1.0 | 固定为 1.0 |
| `hf_cfg.detail_gain` | 1.15 | MULTIBAND 高频细节增益，0..16 |
| `hf_cfg.base_alpha` | 0.0 | 固定为 0.0 |
| `mf_cfg.filter_sigma` | 3.0 | 固定为 3.0 |
| `mf_cfg.detail_gain` | 0.60 | MULTIBAND 中频细节增益，0..16 |
| `mf_cfg.base_alpha` | 0.0 | 固定为 0.0 |
| `lf_cfg.filter_sigma` | 3.0 | 固定为 3.0 |
| `lf_cfg.detail_gain` | 0.0 | 固定为 0.0 |
| `lf_cfg.base_alpha` | 0.08 | MULTIBAND 低频权重，0..1 |
| `flatness_threshold` | 0.0007 | 平坦区域判定阈值，必须大于 0 |
| `gain_min` | 0.5 | 归一化增益下限，必须大于 0 |
| `gain_max` | 2.0 | 归一化增益上限，必须大于 `gain_min` |
| `epsilon` | 1e-6 | 数值保护项，必须大于 0 |
| `registration_confidence` | 1.0 | 配准置信度，范围 0..1，可运行中更新 |
| `fallback_threshold` | 0.5 | 置信度门限，范围 [0,1) |

融合强度按下式平滑过渡：

```text
scale = (registration_confidence - fallback_threshold) / (1 - fallback_threshold)
```

`scale <= 0` 时走 COLOR 直通并累计 `low_confidence_fallback_count`；`scale >= 1` 时使用完整
配置增益。`MEDIA_MCF_GetDefaultAlgParam` 可写入上表缺省值。

### `MEDIA_MCF_FUSION_LEGACY_PARAM`

| 字段 | 缺省值 | 说明 |
| --- | --- | --- |
| `blur_radius` | 2 | 旧版单尺度底图半径；<=0 取 2，>2 截到 2 |
| `base_alpha` | 0.25 | 低频混合权重，0..1 |
| `detail_gain` | 0.80 | 细节注入增益，0..16 |
| `alpha_min` | 0.15 | 梯度权重下限，0..1 |
| `alpha_max` | 0.85 | 梯度权重上限，0..1，必须大于 `alpha_min` |

`MEDIA_MCF_GetDefaultLegacyParam` 可写入缺省值。`LEGACY_SINGLE_SCALE` 使用该结构；通用 API
通过插件参数键 `mcf_legacy_param` 读写，Stop 后才能修改。

### 版本化配置

直接调用 `MEDIA_PLUGIN_CreateModule` 时，配置类型为 `MEDIA_MCF_FUSION_CONFIG`。
`config_size` 至少覆盖到 `alg_param` 之前；未覆盖的算法和 legacy 部分会补旧版缺省值。新字段只允许
追加到结构体末尾。应用通常使用 `MEDIA_MCF_CreateGrp` 或
`MEDIA_MCF_FUSION_CL_CreateGrp`，无需手工组装该结构。

## Pool 要求

- 输入、输出都必须是 native DMA-BUF Pool；host memory 无法通过 EGLImage 导入。
- 每个输入 buffer 必须具备有效的 `MEDIA_BUFFER_META_VIDEO_FRAME` metadata，且 layout 能通过
  `MEDIA_VIDEO_ValidateLayout`。
- COLOR 输入 buffer 至少为 `stride * height * 3 / 2` 字节；GRAY8 输入至少为 `stride * height` 字节。
- 输出 Pool 必须在 Start 前创建，slot 数不少于 `chn.depth`，每个 slot 容量不少于
  `chn.stride * chn.height * 3 / 2`。
- 融合或复用会产生额外在途帧，建议输入 Pool 至少 3 个 slot、输出 Pool 至少 `depth + 2` 个 slot。
- `MEDIA_MCF_SendPipeFrame` 成功后输入 buffer 所有权交给框架；失败时调用方仍持有并负责归还。
- `MEDIA_MCF_GetChnFrame` 成功后必须用 `MEDIA_MCF_ReleaseChnFrame` 归还。

## 使用示例

下面的片段省略了错误处理与输入填充，只展示典型生命周期：

```c
#include <stddef.h>
#include "media_api.h"
#include "media_mcf_fusion.h"

MEDIA_MCF_GRP_ATTR grp = {0}
MEDIA_MCF_CHN_ATTR chn = {0}
MEDIA_MCF_ALG_PARAM alg
MEDIA_SYS_Init
/* 也可让运行时从插件目录加载；直接链接本库时 MEDIA_MCF_* 会自动注册。 */
if (MEDIA_PLUGIN_FindType(MEDIA_MCF_FUSION_TYPE) < 0) {
    MEDIA_PLUGIN_Load("/opt/libmedia/plugins/libmedia_mcf_fusion.so")
}

grp.sync_pipe_en = 1
grp.path = MEDIA_MCF_PATH_FUSION
grp.timestamp_tolerance_us = MEDIA_MCF_DEFAULT_TOLERANCE_US
grp.reuse_last_frame = 0
grp.color_pipe_attr.pipe_id = MEDIA_MCF_PIPE_COLOR
grp.color_pipe_attr.width = 1920
grp.color_pipe_attr.height = 1080
grp.color_pipe_attr.format = MEDIA_FORMAT_NV12
grp.color_pipe_attr.stride = 1920
grp.color_pipe_attr.depth = 4
grp.mono_pipe_attr = grp.color_pipe_attr
grp.mono_pipe_attr.pipe_id = MEDIA_MCF_PIPE_MONO
grp.mono_pipe_attr.format = MEDIA_FORMAT_GRAY8
chn.width = 1920
chn.height = 1080
chn.format = MEDIA_FORMAT_NV12
chn.stride = 1920
chn.depth = 4
chn.output_pool_id = 10; /* 由应用事先创建 */

MEDIA_MCF_CreateGrp(0, &grp)
MEDIA_MCF_SetChnAttr(0, 0, &chn)
MEDIA_MCF_EnableChn(0, 0)
MEDIA_MCF_GetDefaultAlgParam(&alg)
alg.mode = MEDIA_MCF_ALG_MULTIBAND
alg.normalize_mode = MEDIA_MCF_NORMALIZE_MEAN_STD
alg.registration_confidence = 1.0f
MEDIA_MCF_SetAlgParam(0, &alg)
/* 输出 Pool 的 slot 大小由 SDK layout 查询得到，示例 */
MEDIA_MCF_StartGrp(0)
/* 成功发送后 frame 所有权转交插件。 */
MEDIA_MCF_SendPipeFrame(0, MEDIA_MCF_PIPE_COLOR, color_frame, 1000)
MEDIA_MCF_SendPipeFrame(0, MEDIA_MCF_PIPE_MONO, mono_frame, 1000)
MEDIA_BUFFER out
MEDIA_MCF_GetChnFrame(0, 0, &out, 1000)
/* 使用 out 的 metadata 与 DMA-BUF 内容 ... */
MEDIA_MCF_ReleaseChnFrame(0, 0, out)
MEDIA_MCF_StopGrp(0)
MEDIA_MCF_DestroyGrp(0)
MEDIA_SYS_Exit
```

若使用框架绑定，可把 `MEDIA_MCF_FUSION_TYPE` 作为模块名，把两路源分别绑到 `input0`、`input1`
再绑定 `output0`。旧版简化接口仍可直接使用：

```c
MEDIA_MCF_FUSION_CL_ATTR attr = {0}
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.input_depth = 4
attr.output_pool_id = 10
attr.path = MEDIA_MCF_FUSION_CL_PATH_FUSION
attr.normalize_mode = MEDIA_MCF_FUSION_CL_NORMALIZE_MEAN_STD
MEDIA_MCF_FUSION_CL_CreateGrp(0, &attr)
MEDIA_MCF_FUSION_CL_Start(0)
```

旧版接口固定使用 `LEGACY_SINGLE_SCALE`，输出格式与输入格式相同，当前只接受 NV12
`input_stride`/`output_stride` 为 0 时按 64 字节对齐。

## 限制

- 仅实现 GLES 后端；OpenCL、Vulkan、NPU 和 CPU fallback 均未移植。
- 只支持一路输出、通道 0；不支持缩放、裁剪、镜像、翻转、旋转、扩展通道和 VI 直连。
- 对应的 22 个高级 `MEDIA_MCF_*` 接口保留符号并返回 `MEDIA_ERR_UNSUPPORTED`。
- 两路输入尺寸必须一致，输出尺寸必须等于输入尺寸，输出只支持 NV12。
- 高斯 sigma、局部窗口及部分频段固定参数不可调；修改会返回 `MEDIA_ERR_INVALID_PARAM`。
- 当前后端没有 GPU 回读；缓存项占用少量纹理/EGLImage 对象，Stop/Destroy 时统一释放。
- 中间纹理优先使用 RGBA32F，驱动不支持时退到 RGBA16F；大分辨率会占用较多显存。
- 每帧结束调用 `MEDIA_GPU_Finish`，优先保证输出 buffer 可立即安全复用，不追求最低延迟。
- 插件保留旧版应用 API 的源码级兼容，但不承诺与旧内置模块的二进制 ABI 兼容。

## 在发布包中使用

```c
#include "media_mcf_fusion.h"
```

- 头文件：`include/media_mcf_fusion.h`
- 模块映像：`plugins/libmedia_mcf_fusion.so`（与 `libmedia_mcf_fusion.so.sig` 必须同目录）
- 链接：`-lmedia_mcf_fusion -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_mcf_fusion.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("MCF_FUSION_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

