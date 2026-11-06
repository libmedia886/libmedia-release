# fluo_visible_fusion

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `FLUO_VISIBLE_FUSION_CL` | FILTER |

可见光 + 荧光 NV12 双路融合的 GLES 插件，产出 `libmedia_fluo_visible_fusion.so` 与
[`include/media_fluo_visible_fusion.h`](../../include/media_fluo_visible_fusion.h)。
`MEDIA_FLUO_VISIBLE_FUSION_CL_*` API 的函数名、参数、结构体名与字段顺序与旧内置模块源码兼容
type 名沿用 `FLUO_VISIBLE_FUSION_CL`，实例名为 `FLUO_VISIBLE_FUSION_CL_<grp>`。

## 功能

把一路可见光 NV12 与一路荧光 NV12 按荧光强度融合，支持五种方法：

| 方法 | 枚举值 | 行为 |
| --- | --- | --- |
| `MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_GREEN_OVERLAY` | 0 | 荧光区域以绿色（BT.601 近似）叠加在可见光上 |
| `MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_CYAN_OVERLAY` | 1 | 荧光区域以青色叠加 |
| `MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_HOT_OVERLAY` | 2 | 荧光越强越偏热红，色度随强度连续变化 |
| `MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_AVERAGE_GRAY` | 3 | 两路 Y 算术平均，输出中性灰（U/V=128） |
| `MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_LAPLACIAN_GRAY` | 4 | 5x5 盒式低频基底 + 两路较强高频细节，输出中性灰 |

三种 overlay 方法用 `smoothstep(thr_lo, thr_hi, 荧光)` 生成掩码，乘 `overlay_alpha` 后与可见光
亮度、色度做 `mix`；Y 目标值取 `max(可见光Y, 荧光Y * y_boost)`，因此只会提亮荧光区域。
UV 平面按旧 kernel 的做法用 2x2 荧光 Y 平均值重算掩码，避免 Y/UV 交界出现色边。

## 端口

| 端口 | 方向 | 说明 |
| --- | --- | --- |
| `input0` | 输入 | 可见光；Y 与 UV 都参与融合，输出时间戳/seq 也取自该路 |
| `input1` | 输入 | 荧光；只有 Y 亮度参与融合，UV 被忽略 |
| `output0` | 输出 | 融合结果，NV12 |

SendFrame 的 `input_id` 就是端口编号：`MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_VISIBLE`（0）与
`MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_FLUORESCENCE`（1）。插件是 FILTER + 自管模式，内部工作线程
负责收帧、GPU 提交和发帧，`MEDIA_PLUGIN_CLASS.process` 为空。

## 配置字段

`MEDIA_FLUO_VISIBLE_FUSION_CL_ATTR`（零初始化后按需填写）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | int | 可见宽（像素），2..8192 的偶数 |
| `height` | int | 可见高（像素），2..8192 的偶数 |
| `format` | int | 必须是 `MEDIA_FORMAT_NV12` |
| `input_depth` | int | 两个输入端口的队列深度，`<= 0` 取 4 |
| `output_pool_id` | int | 输出 Buffer 所在 Pool，必须 `>= 0`，由调用方事先创建 |
| `input_stride` | int | 输入 NV12 plane0 行字节，`>= width` 且 64 字节对齐 |
| `output_stride` | int | 输出 NV12 plane0 行字节，`>= width` 且 64 字节对齐 |
| `method` | int | 融合方法，只接受 0..4 |
| `thr_lo` | float | 掩码平滑下限（归一化 0..1），`<= 0` 取 0.20 |
| `thr_hi` | float | 掩码平滑上限（归一化 0..1），`<= 0` 取 0.65；不得小于 `thr_lo` |
| `overlay_alpha` | float | 荧光最强处的叠加透明度，`<= 0` 取 0.72，大于 1 时钳制为 1 |
| `y_boost` | float | 荧光亮度增益，`<= 0` 取 1.15 |

`CreateGrp` 会检查格式、尺寸、stride 对齐、方法与阈值关系，并对 NV12 的 GLES 输入/输出能力
调用 `MEDIA_GPU_FormatSupported` 预检；不满足时返回负错误码（格式/尺寸为 `MEDIA_ERR_UNSUPPORTED`
其余参数为 `MEDIA_ERR_INVALID_PARAM`）。

## 输入输出格式

- 逻辑格式固定 NV12（`MEDIA_FORMAT_NV12`），压缩固定 LINEAR（`MEDIA_VIDEO_COMPRESS_NONE`）。
- 两路输入尺寸必须与配置一致；可以逐帧携带 `MEDIA_BUFFER_META_VIDEO_FRAME`（会做
  `MEDIA_VIDEO_ValidateLayout` 校验），也可以不携带，此时按 `input_stride` 推定
  plane0 offset 0、plane1 offset `stride * height`。
- 输出固定为插件构造的 canonical 布局：`drm_fourcc` 由 `MEDIA_VIDEO_GetDrmFourcc` 取得
  plane0/plane1 pitch 均为 `output_stride`，plane1 offset 为 `output_stride * height`
  `len = output_stride * height * 3 / 2`。meta 中的 `time_stamp` 与 `seq` 复制自可见光输入。
- NV12 的两个平面分别作为 R8 EGLImage 导入（plane1 高度为 `height / 2`），Y 与 UV 分两次
  全屏绘制，最后只等一次 `MEDIA_GPU_Finish`。

## Pool 要求

- 调用方创建输出 Pool，slot 数建议 >= 3（测试里用 4）：插件每帧 `MEDIA_POOL_GetBuffer` 取一个
  取不到时本帧丢弃，不会阻塞等待。
- 每个输出 slot 的容量必须 >= `output_stride * height * 3 / 2`，且能按 R8 + modifier 0 导入
  EGLImage；实际做法是同一个 DMA-BUF fd 覆盖两个 plane，plane1 紧接在 plane0 之后。
- 输入 Buffer 可以用任意 Pool，但必须单 fd、两个 plane、LINEAR，`pitch_bytes[i]` 覆盖有效行
  且 `offset/pitch` 不越界；AFBC 或非零 DRM modifier 会被拒绝。
- Buffer 所有权：`SendFrame` 成功即移交插件；插件处理后自动归还（或保留为“上一帧”）。
  `GetFrame` 得到的输出归调用者，必须用 `ReleaseFrame` 归还（该函数直接 `MEDIA_POOL_PutBuffer`
  `grp` 仅为旧 API 兼容保留）。
- 插件的 surface 缓存固定 32 项，按 `(pool, index, Pool 代次, fd, size, layout)` 复用
  输入/输出 slot 总数超过 32 时最久未用的非在用项会被释放后重建。

## 使用示例

```c
#include "media_api.h"
#include "media_fluo_visible_fusion.h"

MEDIA_FLUO_VISIBLE_FUSION_CL_ATTR attr = {0}
MEDIA_BUFFER out
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.input_depth = 4
attr.output_pool_id = 3;          /* 调用方已创建的输出 Pool */
attr.input_stride = 1920;         /* 64 字节对齐 */
attr.output_stride = 1920
attr.method = MEDIA_FLUO_VISIBLE_FUSION_CL_METHOD_GREEN_OVERLAY
/* 静态链接插件或 MEDIA_PLUGIN_Load 之后直接调用。 */
if (MEDIA_FLUO_VISIBLE_FUSION_CL_CreateGrp(0, &attr) != 0) {
    return -1
}
if (MEDIA_FLUO_VISIBLE_FUSION_CL_Start(0) != 0) {
    MEDIA_FLUO_VISIBLE_FUSION_CL_DestroyGrp(0)
    return -1
}

/* visible / fluorescence 都是已填好的 NV12 MEDIA_BUFFER，成功即移交所有权。 */
MEDIA_FLUO_VISIBLE_FUSION_CL_SendFrame(
    0, MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_VISIBLE, visible, 1000)
MEDIA_FLUO_VISIBLE_FUSION_CL_SendFrame(
    0, MEDIA_FLUO_VISIBLE_FUSION_CL_INPUT_FLUORESCENCE, fluorescence, 1000)
if (MEDIA_FLUO_VISIBLE_FUSION_CL_GetFrame(0, &out, 1000) == 0) {
    /* out 中带有效 MEDIA_BUFFER_META_VIDEO_FRAME，可直接交给下游或 CPU 访问。 */
    MEDIA_FLUO_VISIBLE_FUSION_CL_ReleaseFrame(0, out)
}

MEDIA_FLUO_VISIBLE_FUSION_CL_Stop(0)
MEDIA_FLUO_VISIBLE_FUSION_CL_DestroyGrp(0)
```

自动取流时与其它模块一样用 Bind 连接：

```c
MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_FLUO_VISIBLE_FUSION_CL_TYPE, 0, "input0")
MEDIA_SYS_Bind("TPG", 1, "output0", MEDIA_FLUO_VISIBLE_FUSION_CL_TYPE, 0, "input1")
MEDIA_SYS_Bind(MEDIA_FLUO_VISIBLE_FUSION_CL_TYPE, 0, "output0", "VO", 0, "input0")
```

也可以只用通用插件 API：`MEDIA_PLUGIN_CreateModule("FLUO_VISIBLE_FUSION_CL", grp, &attr
sizeof(attr))`，之后用 `MEDIA_PLUGIN_SendFrame/GetFrame/GetParam` 驱动；`GetParam("perf")`
返回 `MEDIA_FLUO_VISIBLE_FUSION_CL_PERF`。

## 限制

- 只支持 NV12 + LINEAR + 单 DMA-BUF fd 的两平面图像；AFBC、tiled modifier、多 fd 均不支持。
- 只保留 GLES 后端，没有 OpenCL/Vulkan/NPU 或 CPU 回退；GLES 上下文创建失败时 `Start` 返回
  `MEDIA_ERR_UNSUPPORTED`。
- 双路按“谁先到谁触发”配对：触发帧到达后最多等另一路 100ms，超时则复用该路上一帧
  某一路从未有过有效帧时，本次触发帧会被归还并等待下一次。
- 输出帧由可见光驱动，`timestamp`/`seq` 只来自 `input0`。
- 最大尺寸 8192x8192；输出 Pool 容量不足或耗尽时静默丢帧（stderr 限次打印原因）。
- 首次使用某个 Buffer 时需要导入 EGLImage 并建 texture/FBO，稳态靠 32 项缓存复用。
- 与旧 OpenCL kernel 的浮点顺序、纹理采样细节不同，8bit 输出可能存在 ±1 的量化差异。

## 在发布包中使用

```c
#include "media_fluo_visible_fusion.h"
```

- 头文件：`include/media_fluo_visible_fusion.h`
- 模块映像：`plugins/libmedia_fluo_visible_fusion.so`（与 `libmedia_fluo_visible_fusion.so.sig` 必须同目录）
- 链接：`-lmedia_fluo_visible_fusion -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_fluo_visible_fusion.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("FLUO_VISIBLE_FUSION_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

