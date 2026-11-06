# retinex

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `RETINEX` | FILTER |

GLES Retinex 图像增强模块。产出 `libmedia_retinex.so` 与
[`include/media_retinex.h`](../../include/media_retinex.h)，沿用旧内置模块的
`MEDIA_RETINEX_*` API 与属性语义，实例名为 `RETINEX_<grp>`。

插件类型名为 **`RETINEX`**，可用于 `MEDIA_SYS_Bind("RETINEX", grp, ...)`
应用侧 Bind 名与旧模块一致。模块只依赖公开 SDK（`media_api.h`、`media_plugin.h`）
与 libc/EGL/GLESv2/pthread/libm，不包含 core 内部头文件。

## 功能与端口

对 NV12 图像的 Y 平面做多尺度 Retinex 增强，色度保持输入值：

| 端口 | 方向 | 格式 | 说明 |
| --- | --- | --- | --- |
| `input0` | 输入 | NV12 LINEAR | 原始帧，同时作为第 0 路照明估计（旧模块语义） |
| `input1` | 输入 | NV12 LINEAR | 第 1 路高斯模糊帧，仅 `scale_count >= 2` 时存在 |
| `input2` | 输入 | NV12 LINEAR | 第 2 路高斯模糊帧，仅 `scale_count == 3` 时存在 |
| `output0` | 输出 | NV12 LINEAR | 增强结果，尺寸与可见区域同输入 |

模块为 `FILTER` + **自管模式**（`process = NULL`）：每个实例在 `MEDIA_RETINEX_Start`
时创建自己的工作线程，在该线程内 `MEDIA_GPU_CreateContext` / `MEDIA_GPU_MakeCurrent`
并加载 program binary；`MEDIA_RETINEX_Stop` 时框架先关闭输入队列，插件再 join 线程并
销毁 EGL 上下文。

每产生一帧输出，工作线程按 `input0`、`input1`、`input2` 的顺序各取一帧。`input0`
最多等待 200 ms，其余端口最多等待 500 ms。`input0` 超时表示当前没有输入，属于空闲
不计入 `drop`；次级端口超时（或返回管线停止）时，已收到的输入全部归还，该组输入丢弃并
计入 `drop` 统计，不产生输出。多路输入**不做时间戳对齐**，只按到达顺序配对。

## 算法

移植自旧模块的 逐像素关系为：

```text
y_lg = (1 / N) * Σ_i [ safe_log(Y0) - safe_log(Yi) ]
Yout = clamp( gain * (Y0 + 20) / 128 * (y_lg - log_min) / 255, 0, 1 )
Yout = max(Yout, Y0 / 255)
```

- `N = scale_count`，`Y0` 取 `input0` 的亮度，`Yi` 取第 i 路（含 `input0` 自身）的亮度
  因此 `scale_count = 1` 时 `y_lg` 恒为 0，输出只由 `gain`、`log_min` 与原始亮度决定。
- `safe_log(0) = 0`，与旧 kernel 的 `log_table[0] = 0` 一致。
- 色度直接取 `input0` 的 2x2 块（`U`、`V` 原样写入输出），不做色调映射。
- 公式含 `+20` 偏置，`scale_count = 1` 时纯黑像素也可能被抬亮，这是旧算法行为。
- `threshold`、`log_max` 仅为源码兼容保留，当前算法不参与计算（旧 CL kernel 同样未使用）。
- 每个像素、每个尺度使用相同坐标采样；旧 CL kernel 因 `pos_ga.x` 跨通道/跨尺度累计
  第 2、3 路会整体右移，本实现已修正。

## 配置字段

`MEDIA_RETINEX_ATTR` 的字段名与顺序与旧 `media_api.h` 完全一致，仅在末尾追加
`output_pool_id`。旧调用方需用新头文件重新编译；建议先 `memset` 清零再做指定初始化。

| 字段 | 类型 | 默认/合法值 | 说明 |
| --- | --- | --- | --- |
| `scale_count` | `int` | 1..3，必填 | 输入路数与 Retinex 尺度数；第 0 路为原始帧 |
| `width` | `int` | 16..8192 的偶数，必填 | 图像有效宽度 |
| `height` | `int` | 16..8192 的偶数，必填 | 图像有效高度 |
| `format` | `int` | 仅 `MEDIA_FORMAT_NV12` | 输入输出像素格式 |
| `output_depth` | `int` | 0..64，0 取 4 | 兼容字段；插件框架不提供输出端口队列深度，当前不生效 |
| `input_depth` | `int` | 0..64，0 取 4 | 每个输入端口队列深度 |
| `input_stride` | `int` | 0 或 `>= width` 且 64 字节对齐 | 输入 Y 平面字节 pitch；0 取 SDK canonical pitch |
| `output_stride` | `int` | 0 或 `>= width` 且 64 字节对齐 | 输出 Y 平面字节 pitch；0 取 canonical pitch |
| `gain` | `float` | `> 0`，否则取 30.0 | Retinex 增益 |
| `threshold` | `float` | 0 或正数，否则取 0.5 | 兼容旧配置，当前不参与计算 |
| `log_min` | `float` | 由调用方给出 | 对数域亮度下限，直接参与输出映射 |
| `log_max` | `float` | 无默认 | 兼容旧配置，当前不参与计算 |
| `passthrough` | `int` | 0/1，仅 `scale_count == 1` 可为 1 | 创建即进入直通模式 |
| `output_pool_id` | `int` | `> 0` 时使用该 Pool；`<= 0` 复用 `input0` 的 Pool | 追加字段，旧调用方零初始化即保持旧行为 |

创建期校验失败返回 `MEDIA_ERR_INVALID_PARAM` / `MEDIA_ERR_UNSUPPORTED`。`scale_count`
超出 1..3、宽高为奇数或越界、stride 过小或未对齐、队列深度越界、格式非 NV12、
多尺度直通、`output_pool_id` 不存在或单 Buffer 容量不足都会被拒绝。

## 应用 API

```c
int MEDIA_RETINEX_CreateGrp(int grp, const MEDIA_RETINEX_ATTR *attr)
int MEDIA_RETINEX_SetGrpAttr(int grp, const MEDIA_RETINEX_ATTR *attr)
int MEDIA_RETINEX_DestroyGrp(int grp)
int MEDIA_RETINEX_Start(int grp)
int MEDIA_RETINEX_Stop(int grp)
int MEDIA_RETINEX_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms)
int MEDIA_RETINEX_ReleaseFrame(int grp, MEDIA_BUFFER buf)
int MEDIA_RETINEX_SetPassthrough(int grp, int enable)
```

- `SetGrpAttr` 沿用旧 glue 语义：仅当 `gain > 0` 时更新 gain，仅当 `threshold > 0`
  时更新 threshold，仅当 `passthrough != 0` 时进入直通；关闭直通请用
  `MEDIA_RETINEX_SetPassthrough(grp, 0)`。
- 运行期参数键：`gain`（float）、`threshold`（float）、`passthrough_mode`（int）。
  `scale_count > 1` 时打开直通返回 `MEDIA_ERR_INVALID_PARAM`。
- `GetFrame` / `ReleaseFrame` 固定访问 `output0`；未释放的输出 Buffer 会占用输出 Pool。
- 统计：`MEDIA_SYS_GetPipelineStats` 中该实例 `has_gpu = 1`，`gpu_kernel_ms` 为最近一次
  render 的墙钟耗时，detail 形如 `gles retinex scale=3 pass=0 draw=12 err=0 drop=0`。

## Pool 要求

- 输入 Buffer 必须来自 `MEDIA_POOL_*` 创建的 DMA-BUF Pool，metadata 为
  `MEDIA_BUFFER_META_VIDEO_FRAME` + NV12，可见尺寸必须与配置一致，layout 需通过
  `MEDIA_VIDEO_ValidateLayout`，Buffer 容量不小于 layout 所需大小。
- 每个输入端口各需一个 Pool（或共用满足容量与数量的 Pool）。Pool 数量需覆盖端口
  队列深度与生产者/消费者在途帧，建议每个输入至少 4 个 Buffer。
- 输出默认从 `input0` 所在 Pool 取 Buffer（与旧模块一致），此时该 Pool 需同时容纳
  输入在途帧与在途输出帧；生产环境建议用独立 `output_pool_id`，避免上下游互相挤占。
- 输出单 Buffer 容量需不小于 `output_stride * height * 3 / 2`（`output_stride = 0`
  时为 canonical NV12 大小），否则 Create 直接拒绝。输出 Pool 的 Buffer 必须按配置的
  `output_stride`（0 表示 canonical pitch）布局；插件发布的 metadata 使用该布局
  Pool 实际 pitch 不一致时下游会按错误行距解读，Create 无法完全识别这种情况。
- 所有输入与输出 Buffer 在 Stop/Destroy 后归还；可用 `MEDIA_SYS_GetPipelineStats`
  检查各 Pool 的 `used_count` 是否为 0，或 `MEDIA_POOL_Destroy` 是否返回
  `MEDIA_ERR_BUSY`。

## 使用示例

以下为 `scale_count = 1` 的最小链路：TPG 产生 NV12，经 Retinex 后由应用取帧。
多尺度时再创建 `scale_count - 1` 路模糊源，分别 Bind 到 `input1`、`input2`。

```c
#include "media_api.h"
#include "media_retinex.h"
#include "media_tpg.h"

#define GRP 0
#define POOL_IN 1
#define POOL_OUT 2

MEDIA_VIDEO_LAYOUT layout
MEDIA_RETINEX_ATTR attr
MEDIA_TPG_ATTR tpg
MEDIA_BUFFER frame
size_t size = 0
MEDIA_SYS_Init
/* 直接链接 libmedia_retinex.so 时，首次调用 MEDIA_RETINEX_* 会自动注册插件
 * 仅 dlopen 方式则先 MEDIA_PLUGIN_Load("libmedia_retinex.so")。 */
MEDIA_VIDEO_QueryLayout(1920, 1080, MEDIA_FORMAT_NV12, 0, &layout, &size)
MEDIA_POOL_CreateWithMode(POOL_IN, size, 8, MEDIA_POOL_ALLOC_MODE_NATIVE)
MEDIA_POOL_CreateWithMode(POOL_OUT, size, 6, MEDIA_POOL_ALLOC_MODE_NATIVE)
memset(&attr, 0, sizeof(attr))
attr.scale_count = 1
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.gain = 32.0f
attr.log_min = -3.0f
attr.output_pool_id = POOL_OUT
if (MEDIA_RETINEX_CreateGrp(GRP, &attr) != MEDIA_STATUS_OK) {
    return -1
}

memset(&tpg, 0, sizeof(tpg))
tpg.width = 1920
tpg.height = 1080
tpg.stride = (int)layout.pitch_bytes[0]; /* 与 Retinex 输入端口声明的 stride 一致 */
tpg.format = MEDIA_FORMAT_NV12
tpg.pool_id = POOL_IN
tpg.pattern = MEDIA_TPG_PATTERN_COLOR_BARS
MEDIA_TPG_CreateChn(0, &tpg)
MEDIA_SYS_Bind("TPG", 0, "output0", "RETINEX", GRP, "input0")
MEDIA_RETINEX_Start(GRP)
MEDIA_TPG_Start(0)
if (MEDIA_RETINEX_GetFrame(GRP, &frame, 1000) == MEDIA_STATUS_OK) {
    /* 使用 frame，例如送 VO 或读取 Y 平面 */
    MEDIA_RETINEX_ReleaseFrame(GRP, frame)
}

MEDIA_TPG_Stop(0)
MEDIA_RETINEX_Stop(GRP)
MEDIA_SYS_UnBind("TPG", 0, "output0", "RETINEX", GRP, "input0")
MEDIA_RETINEX_DestroyGrp(GRP)
MEDIA_TPG_DestroyChn(0)
MEDIA_POOL_Destroy(POOL_OUT)
MEDIA_POOL_Destroy(POOL_IN)
MEDIA_SYS_Exit
```

插件形式的加载也可用环境变量：`MEDIA_SYS_Init` 会自动加载
`MEDIA_PLUGIN_PATH`（冒号分隔目录）下全部 `*.so`，之后直接用
`MEDIA_PLUGIN_CreateModule("RETINEX", grp, &attr, sizeof(attr))` 创建实例。
上述示例需链接 `libmedia_retinex.so`、`libmedia_tpg.so` 与 `libmedia.so`。

## 限制

- 仅 GLES 后端；旧 retinex 的 OpenCL 实现未移植。旧目录本来也没有 Vulkan/NPU/CPU 后端。
- 仅 `MEDIA_FORMAT_NV12` + LINEAR；不支持 AFBC、P010/10bit、RGB 或裁剪/缩放。
- 宽高必须为偶数且在 16..8192 之间；stride 必须为 0 或 64 字节对齐且不小于宽度。
- 最多 3 路输入；多路输入不做时间戳对齐，缺一路即整组丢弃。
- 直通仅支持 `scale_count == 1`，且仍经 GPU 拷贝到输出 Pool，不是零拷贝引用输入。
- 直通只跳过增强公式，输出仍写入 NV12（色度取 `input0`）。
- `output_stride > width` 时，GPU 只写可见区域，行尾 padding 内容未定义。
- `output_depth` 为兼容字段，插件框架的输出端口不使用队列深度，当前不生效。
- program binary 与构建机 GL 指纹绑定，驱动升级后需重新构建插件。
- 每帧调用 `MEDIA_GPU_ImportBuffer` 导入输入/输出；SDK 按 (pool, index, 代次, layout)
  缓存导入结果，稳态下不重复创建 EGLImage。当前未做性能基准，4K 多路场景请自行压测。

## 在发布包中使用

```c
#include "media_retinex.h"
```

- 头文件：`include/media_retinex.h`
- 模块映像：`plugins/libmedia_retinex.so`（与 `libmedia_retinex.so.sig` 必须同目录）
- 链接：`-lmedia_retinex -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_retinex.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("RETINEX", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

