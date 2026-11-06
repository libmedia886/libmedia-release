# exposure_fusion

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `EXPOSURE_FUSION_CL` | FILTER |

两路 NV12 曝光融合的 GLES 外部插件，产出 `libmedia_exposure_fusion.so` 与
[`include/media_exposure_fusion.h`](../../include/media_exposure_fusion.h)。`MEDIA_EXPOSURE_FUSION_CL_*` 的函数名、
参数、属性结构体名与字段顺序与旧内置模块（`master:include/media_api.h`）完全一致；插件 type 名仍是
`EXPOSURE_FUSION_CL`，实例名 `EXPOSURE_FUSION_CL_<grp>`，应用侧 Bind 名不变。

## 功能

对同一场景的两路曝光（例如短曝光 / 长曝光）做 Mertens 风格的加权融合。算法直接在 YUV 编码值域工作
不做 RGB 颜色矩阵转换：

```
w     = contrast^cp * saturation^sp * exposedness^ep
w0n   = S(w0) / (S(w0) + S(w1) + epsilon)      /* S：3x3 邻域求和 */
fused = clamp(low + 0.45 * (w0n * (y0 - low0) + w1n * (y1 - low1)), 0, 1)
```

- 局部对比度 `contrast = |4*c - l - r - u - d|`（Y 平面的拉普拉斯响应）
- 色饱和度 `saturation = sqrt((U - 0.5)^2 + (V - 0.5)^2)`（2x2 块内取同一 UV）
- 曝光适度 `exposedness = exp(-0.5 * (y - 0.5)^2 / sigma^2)`
- 权重在 3x3 邻域求和平滑后归一化，1/2 分辨率上做 1-2-1 高斯低通
- 低频用平滑权重加权融合，高频细节按同一权重叠加回来
- U/V 用对应 2x2 块的权重平均值混合。

GLES 实现拆成 5 个渲染 pass：

| pass | 输出 | 说明 |
| --- | --- | --- |
| weight | `RGBA16F` = (w0, w1, y0, y1) | 全分辨率采样两路 NV12，计算原始权重 |
| smooth | `RGBA16F` = (w0n, w1n, ...) | 3x3 权重和归一化 |
| reduce | 半分辨率 `RGBA16F` = (low0, low1, fused_low) | 1-2-1 高斯低通 + 权重加权低频 |
| reconstruct | 全分辨率 `RGBA16F` = fused Y | 低频 + 加权高频细节 |
| compose | NV12 `output0` | `layout(yuv)` 直写 Y/UV，UV 用 2x2 块平均权重 |

## 设计

- FILTER 插件、**自管模式**（`process = NULL`）：插件工作线程用 `MEDIA_PLUGIN_RecvFrame` 收 input0/input1
  处理完用 `MEDIA_PLUGIN_EmitFrame` 输出、`MEDIA_PLUGIN_ReturnFrame` 归还输入。
- **GLES only**：`MEDIA_GPU_CreateContext / MakeCurrent / ImportBuffer / Finish / DestroyContext` 全部在该线程内
  两路 NV12 用 `__samplerExternal2DY2YEXT` 按编码值零拷贝采样，输出用 `layout(yuv)` FBO 直接写外部纹理。
- **时域配对**：每轮先等 input0 最多 100 ms，再等 input1 最多 100 ms。只有一路有新帧时，另一路复用上一次
  **成功输出过**的帧（旧模块的 last-frame 语义）；某一路从未有过历史则丢弃本次触发。两路都没有新帧时继续等待。
  在构建期用本机 Mali 驱动编成 program binary（`MEDIA_GPU_LoadProgramBinary`），binary 与生成的 `.c` 都不进 git。
- **完成与回收**：每对帧都 `MEDIA_GPU_Finish` 后才交还输入；Finish 失败时销毁上下文并进入 fail-stop
  不再处理新帧，直到 Stop/Destroy 回收 last-frame，避免 GPU 仍在访问时把 buffer 还给 Pool。
- **输出布局**：输出 buffer 的 meta 由插件按 `output_stride` 重新发布为 NV12 LINEAR（`pitch = output_stride`
  `offset1 = output_stride * height`，`len = output_stride * height * 3 / 2`），时间戳与帧序号取自本轮
  最新触发的那一路（两路都新时优先 input0）。
- 统计：`MEDIA_PLUGIN_RecordProcess` 记录处理耗时，`MEDIA_PLUGIN_ReportStats` 上报 GPU 耗时
  `MEDIA_SYS_GetPipelineStats` 的 `gpu_detail` 形如
  `gles pairs=5 reuse=2 skip=0 fail=0`（core 还会追加 `held=` / `leaked=`）。

## 端口

| 端口 | 方向 | 格式 | 说明 |
| --- | --- | --- | --- |
| `input0` | 输入 | NV12 LINEAR | 第一路；队列深度 `input_depth` |
| `input1` | 输入 | NV12 LINEAR | 第二路；队列深度 `input_depth` |
| `output0` | 输出 | NV12 LINEAR | 融合结果，从 `output_pool_id` 指定的 Pool 分配 |

两路输入的 visible 尺寸必须等于 `width × height`，plane0 pitch >= width；只接受
`MEDIA_VIDEO_COMPRESS_NONE`（LINEAR）且 `drm_modifier == 0` 的帧，AFBC 等压缩布局一律丢弃并计数。
插件按每帧真实 layout 的 `valid_x/valid_y/storage_*` 做归一化采样，因此输入 pitch 可以大于 `input_stride`
建议让输入 Pool 的 pitch 与配置一致，便于上游对齐。

## 配置字段

`MEDIA_EXPOSURE_FUSION_CL_ATTR` 的字段名称、类型与顺序与旧头文件逐字一致：

| 字段 | 含义 | 默认 / 合法值 |
| --- | --- | --- |
| `width` | 图像宽（像素），输入输出共用 | 2..8192，必须为偶数 |
| `height` | 图像高（像素），输入输出共用 | 2..8192，必须为偶数 |
| `format` | 输入与输出格式 | 只支持 `MEDIA_FORMAT_NV12` |
| `input_depth` | input0/input1 的 Bind 队列深度 | 0 = `MEDIA_EXPOSURE_FUSION_CL_DEFAULT_DEPTH`(4)；1..64 |
| `output_pool_id` | 输出 buffer 所在 Pool | 必须 >= 0，Pool 至少 1 个 buffer 且容量 >= 一帧 NV12 |
| `input_stride` | 输入 plane0 pitch | 必须 > 0、>= width 且 64 字节对齐 |
| `output_stride` | 输出 plane0 pitch | 必须 > 0、>= width 且 64 字节对齐 |
| `contrast_power` | 对比度项指数 | < 0 使用 1.0；0 使该项恒为 1 |
| `saturation_power` | 饱和度项指数 | < 0 使用 1.0；0 使该项恒为 1 |
| `exposedness_power` | 曝光适度项指数 | < 0 使用 1.0；0 使该项恒为 1 |
| `sigma` | 曝光适度高斯 sigma（Y 归一化到 0..1） | <= 0 使用 0.2 |
| `epsilon` | 幂运算与权重归一化保护项 | <= 0 使用 1e-6 |

非法组合在 `MEDIA_EXPOSURE_FUSION_CL_CreateGrp`（即 `MEDIA_PLUGIN_CreateModule`）阶段就被拒绝
不会留下半成品实例；`output_pool_id` 容量不足返回 `MEDIA_ERR_INVALID_PARAM`，GPU 不支持 NV12
导入/导出返回 `MEDIA_ERR_UNSUPPORTED`。

## Pool 要求

- **输出 Pool**：由 `output_pool_id` 指定，必须显式给出；每个 buffer 大小 >=
  `output_stride * height * 3 / 2`，建议至少 2~3 个 buffer 以免下游取帧不及时时丢帧。
- **输入 Pool**：两路输入各自所在 Pool 的 buffer 必须能按 NV12 VIDEO_FRAME 导入 GLES
  pitch >= width，visible 尺寸与配置一致。
- 输入 buffer 由插件在工作线程内 `ReturnFrame`，输出由 `MEDIA_PLUGIN_EmitFrame` 交给获取方（手动 GetFrame
  或下游模块）。调用方只需保证自己不再引用，不要重复归还。
- 插件在 Stop/Destroy 时 `join` 工作线程并归还所有 last-frame 引用；`MEDIA_POOL_Destroy` 成功即说明没有泄漏。

## 使用示例

```c
#include "media_api.h"
#include "media_exposure_fusion.h"

/* 输出 Pool 必须由应用创建并传给插件。 */
size_t frame_size = (size_t)1984 * 1080 * 3 / 2
MEDIA_POOL_Create(9, frame_size, 3)
MEDIA_EXPOSURE_FUSION_CL_ATTR attr = {
    .width = 1920
    .height = 1080
    .format = MEDIA_FORMAT_NV12
    .input_depth = 4,           /* 0 也等价于 4 */
    .output_pool_id = 9
    .input_stride = 1920,       /* 必须 64 字节对齐 */
    .output_stride = 1984,      /* 1920 向上对齐到 64 */
    .contrast_power = -1.0f,    /* < 0：默认 1.0 */
    .saturation_power = -1.0f
    .exposedness_power = -1.0f
    .sigma = -1.0f,             /* <= 0：默认 0.2 */
    .epsilon = -1.0f,           /* <= 0：默认 1e-6 */
}
/* 链接 libmedia_exposure_fusion.so 后首次调用会自行注册插件，也可先 MEDIA_PLUGIN_Load(path)。 */
MEDIA_EXPOSURE_FUSION_CL_CreateGrp(0, &attr)
MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_EXPOSURE_FUSION_CL_TYPE, 0, "input0")
MEDIA_SYS_Bind("TPG", 1, "output0", MEDIA_EXPOSURE_FUSION_CL_TYPE, 0, "input1")
MEDIA_EXPOSURE_FUSION_CL_Enable(0)
MEDIA_BUFFER out
if (MEDIA_EXPOSURE_FUSION_CL_GetFrame(0, &out, 100) == MEDIA_STATUS_OK) {
    /* 使用 out ... */
    MEDIA_EXPOSURE_FUSION_CL_ReleaseFrame(0, out)
}

MEDIA_EXPOSURE_FUSION_CL_Disable(0)
MEDIA_EXPOSURE_FUSION_CL_DestroyGrp(0)
```

`Enable` / `Disable` 与 `Start` / `Stop` 等价；`ReleaseFrame` 忽略 `grp` 参数，直接 `MEDIA_POOL_PutBuffer`。

## 限制

- 只支持 NV12 LINEAR 输入输出，无 RGB / P010 / AFBC；旧模块的 OpenCL 后端以及 Vulkan/NPU/CPU fallback
  全部丢弃，没有软件回退。
- 宽高必须为偶数（2..8192）；stride 必须显式给出、>= width 且 64 字节对齐。
- 需要驱动支持 `GL_OES_EGL_image_external_essl3`、`GL_EXT_YUV_target` 和可渲染的 RGBA16F
  （`GL_EXT_color_buffer_half_float` 或 `GL_EXT_color_buffer_float`）；任一缺失时 Start 返回 `MEDIA_ERR_RESOURCE`。
- 中间纹理固定为 3 张全分辨率 + 1 张半分辨率 RGBA16F，1080p 约占 54 MB 显存，4K 约占 216 MB。
- 只有单路先到、另一路还没有任何历史帧时，第一对触发会被丢弃并计入 `skip`；两路持续送帧时第一对即可输出。
- `MEDIA_EXPOSURE_FUSION_CL_GetFrame` 超时或实例未运行时返回 `-1`（`MEDIA_PLUGIN_GetFrame` 的既有语义）
  不区分“暂无帧”和“已停止”。
- `MEDIA_EXPOSURE_FUSION_CL_ReleaseFrame` 的 `grp` 参数仅为兼容旧 API 保留。
- 旧模块的 `module_check_license` 校验没有移植（公开 SDK 无对应接口），插件本身不做许可证检查。

## 在发布包中使用

```c
#include "media_exposure_fusion.h"
```

- 头文件：`include/media_exposure_fusion.h`
- 模块映像：`plugins/libmedia_exposure_fusion.so`（与 `libmedia_exposure_fusion.so.sig` 必须同目录）
- 链接：`-lmedia_exposure_fusion -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_exposure_fusion.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("EXPOSURE_FUSION_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

