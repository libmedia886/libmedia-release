# conv

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `CONV_CL` | FILTER |

GLES 二维卷积模块：对一帧视频做最大 31x31 的加权求和（卷积），权重表由应用逐帧可改。产出
`libmedia_conv.so` 与 [`include/media_conv.h`](../../include/media_conv.h)；`MEDIA_CONV_CL_*` 函数名、参数、
`MEDIA_CONV_CL_ATTR` / `MEDIA_CONV_CL_PERF` 字段顺序与旧内置 `conv_cl` 完全一致，应用侧源码无需改动。
插件注册的模块 type 沿用旧名 `CONV_CL`，实例名为 `CONV_CL_<grp>`，端口固定 `input0` / `output0`。

## 功能

- 单路输入、单路输出的 FILTER 模块，**托管模式**：框架工作线程每帧调用 `process`，插件导入输入、
  渲染到应用提供的输出 Pool、等 GPU 完成后交出一帧。
- 卷积核尺寸 1..31 的奇数（默认 5），权重表为 `kernel_size * kernel_size` 个 `float`，上传为
  `GL_R32F` 纹理，shader 内按 `kernel_size` 截断循环，因此同一份 program binary 支持全部尺寸。
- 权重作用于归一化后的像素值（0..1），结果 `clamp(0,1)` 后写回；权重全 1 的中心表即单位脉冲（直通）。
- 越界采样采用边缘复制（坐标 clamp 到 `[0, width-1] x [0, height-1]`）。
- YUV 输入用 `__samplerExternal2DY2YEXT` 采样、`layout(yuv)` 直接写回，不做 RGB 往返；非 YUV 输入用
  `samplerExternalOES` 采样、`layout(location=0)` 输出。
- 直通模式（`MEDIA_CONV_CL_SetPassthrough`）跳过卷积权重，用 1x1 单位表画一次，输出仍来自输出 Pool。
- 统计：`MEDIA_PLUGIN_ReportStats` 上报最近一帧 GPU kernel 耗时，detail 形如
  `gles k=5 pass=0 draws=12 err=0`，可用 `MEDIA_SYS_GetPipelineStats` / `MEDIA_SYS_debug` 查看。

## 端口与运行模式

| 项目 | 值 |
| --- | --- |
| 模块 type | `CONV_CL`（`MEDIA_CONV_TYPE`） |
| 实例名 | `CONV_CL_<grp>` |
| kind | `MEDIA_PLUGIN_KIND_FILTER` |
| 运行模式 | 托管（提供 `process`，无插件线程） |
| 输入 | `input0`，1 路，队列深度 `input_depth`（0 取默认 4） |
| 输出 | `output0`，1 路，缓冲来自 `output_pool_id` |
| 输入/输出格式 | 同一个 `MEDIA_FORMAT_*`，LINEAR（`MEDIA_VIDEO_COMPRESS_NONE`），同宽同高 |

GPU 上下文在 `MEDIA_CONV_CL_Start` 中创建（`MEDIA_GPU_CreateContext`，进程级共享 GBM EGLDisplay）
`Stop` 中销毁；`process` 每帧 `MEDIA_GPU_MakeCurrent` / `ReleaseCurrent`，与框架工作线程一一对应。

## 配置字段（`MEDIA_CONV_CL_ATTR`）

| 字段 | 含义 | 约束 |
| --- | --- | --- |
| `width` | 图像宽度（像素） | 16..8192 |
| `height` | 图像高度（像素） | 16..8192 |
| `format` | 输入/输出共用的 `MEDIA_FORMAT_*` | LINEAR；见下方格式表 |
| `kernel_size` | 卷积核边长 | 1..31 的奇数；0 表示默认 5 |
| `input_depth` | `input0` 队列深度 | 0 表示 4；其余为正值 |
| `output_pool_id` | 应用创建的输出 Pool | 必须 >= 0 且已创建、可导入 |
| `input_stride` | 输入 plane0 字节 pitch | 0 表示按格式推导；非 0 必须 >= canonical pitch 且 64 字节对齐（沿用旧 `g_conv_gles_caps.stride_align`） |
| `output_stride` | 输出 plane0 字节 pitch | 0 表示与 canonical pitch 相同；非 0 必须 >= canonical pitch 且 64 字节对齐，并按该 pitch 重算各 plane 偏移与总大小 |

逐帧输入的真实布局以 buffer metadata（`MEDIA_BUFFER_META.u.video`）为准：`input_stride` 只用于端口声明
每帧仍会 `MEDIA_VIDEO_ValidateLayout` 校验宽度、高度、plane pitch/offset 与 buffer 长度。

## 输入输出格式

创建时先过插件自己的格式白名单，再要求 `MEDIA_GPU_FormatSupported(format, MEDIA_PORT_DIR_INPUT)` 与
`MEDIA_GPU_FormatSupported(format, MEDIA_PORT_DIR_OUTPUT)` 同时为真（与内置 GLES 模块同一张能力表）：

```
GRAY8, NV12, YUV420SP, YCrCb_420SP, YUV422SP, YUV420P, YUV422_YUYV
RGB565, BGR565, RGBA5551, RGBA4444, RGB888, BGR888
RGBA8888, BGRA8888, ARGB8888, ABGR8888
YUV420SP_10BIT, P010, P210, Y210, Y410, Y0L2, ABGR2101010, YUV444P, BGR101010
```

实际可用集合取决于 Mali 驱动与 SDK 能力表；板上已验证 `GRAY8`、`RGB888` 与 `NV12`。格式不在白名单返回
`MEDIA_ERR_UNSUPPORTED`，白名单内但当前驱动不支持同样返回 `MEDIA_ERR_UNSUPPORTED`。

每帧输入必须是 `MEDIA_BUFFER_META_VIDEO_FRAME`，`media_format`、`visible_width`、`visible_height` 与
配置一致，`compress` 必须为 NONE；输出 metadata 由插件填写（同一 format/尺寸/layout，复制输入的
`time_stamp` 与 `seq`，`len` 为输出 layout 的总大小）。

## Pool 要求

- 输入 Pool：`MEDIA_POOL_ALLOC_MODE_NATIVE`（非 HOST 内存），建议 `input_depth + 1` 个以上 slot。
- 输出 Pool：应用创建，slot 数 >= 1，**每个 slot** 都要满足 `MEDIA_BUFFER_IsHostMemory == 0`、
  `MEDIA_POOL_GetFd` 成功，且容量不小于本插件计算出的输出 layout 大小（多平面含所有 plane）。
- 输入与输出不能是同一个 DMA-BUF fd（同 Pool 直通会被拒绝）。
- 输出 Pool 在整个 `CreateGrp` .. `DestroyGrp` 生命周期内必须有效，由应用在 `DestroyGrp` 之后销毁。
- 插件只借用输入（框架在 `process` 返回后归还），输出由插件从 Pool 取出后直接交给框架。

## 使用示例

```c
#include "media_api.h"
#include "media_conv.h"

#define POOL_IN   10
#define POOL_OUT  11
#define W 1920
#define H 1080

MEDIA_VIDEO_LAYOUT layout
size_t size = 0
MEDIA_CONV_CL_ATTR attr = {
    .width = W
    .height = H
    .format = MEDIA_FORMAT_NV12
    .kernel_size = 5
    .input_depth = 4
    .output_pool_id = POOL_OUT
    .input_stride = 0
    .output_stride = 0
}
float mean25[25]
MEDIA_SYS_Init
MEDIA_VIDEO_QueryLayout(W, H, MEDIA_FORMAT_NV12, MEDIA_VIDEO_COMPRESS_NONE, &layout, &size)
MEDIA_POOL_CreateWithMode(POOL_IN, size, 6, MEDIA_POOL_ALLOC_MODE_NATIVE)
MEDIA_POOL_CreateWithMode(POOL_OUT, size, 4, MEDIA_POOL_ALLOC_MODE_NATIVE)
MEDIA_CONV_CL_CreateGrp(0, &attr)
for (int i = 0; i < 25; ++i) {
    mean25[i] = 0.04f;                 /* 5x5 均值核 */
}
MEDIA_CONV_CL_SetTable(0, mean25, 25)
MEDIA_CONV_CL_Start(0)
/* 手动送帧：从 POOL_IN 取 buffer、填像素并写好 VIDEO_FRAME metadata；成功后所有权移交队列。 */
MEDIA_SYS_SendFrame(MEDIA_CONV_TYPE, 0, "input0", in_buf, 1000)
MEDIA_BUFFER out
if (MEDIA_CONV_CL_GetFrame(0, &out, 1000) == 0) {
    /* 读取 out 的 metadata / 像素，或转交下游 */
    MEDIA_CONV_CL_ReleaseFrame(0, out)
}

MEDIA_CONV_CL_Stop(0)
MEDIA_CONV_CL_DestroyGrp(0)
MEDIA_POOL_Destroy(POOL_OUT)
MEDIA_POOL_Destroy(POOL_IN)
MEDIA_SYS_Exit
```

流水线里也可以直接按 type 名 Bind，名字与旧模块一致：

```c
MEDIA_SYS_Bind("CONV_CL", 0, "output0", "SOME_MODULE", 1, "input0")
```

## 限制

- 只移植了 GLES 后端；旧模块的 OpenCL / Vulkan / NPU / CPU 回退均不提供。
- 只支持 LINEAR 存储，压缩（AFBC）输入/输出不支持；输入输出必须同格式、同尺寸、不缩放不裁剪。
- 卷积核边长上限 31（受 shader 循环上界约束），且必须为奇数；权重必须是有限 `float`，表长必须等于
  `kernel_size * kernel_size`。
- `SetTable` / `SetKernelSize` 可在运行中调用，下一帧生效；`SetKernelSize` 会把表重置为单位脉冲。
- 直通模式仍会占用一个输出 Pool buffer 并做一次 GPU 绘制，不是零拷贝转发。
- 每帧同步等待 GPU 完成（`MEDIA_GPU_Finish`，失败时回退 `glFinish`），因此单帧延迟包含 GPU 时间
  本模块不做多帧流水，也不在插件内缓存跨帧数据。
- YUV 的 EGL 色彩空间提示由 `MEDIA_GPU_ImportBuffer` 固定为 REC601 + 窄范围，旧模块使用的 REC709
  无法从插件设置；恒值帧数值不受影响，非恒值 YUV 可能有细微差别。
- `MEDIA_CONV_CL_GetLastPerf` 通过 `MEDIA_SYS_GetPipelineStats` 读取 `CONV_CL_<grp>` 的统计项：
  尚未处理过帧时返回 `MEDIA_ERR_RESOURCE`；`gpu_queue_total_ms` 固定为 -1（旧模块未统计队列时间）。
- `Start` 在未运行时新建 GLES 上下文并加载 program binary；运行中重复调用返回成功且不重建
  （与旧 `conv_gles_start` 的幂等语义一致）。`Stop` 后重新 `Start` 可以恢复，`Stop` 可重复调用。

## 在发布包中使用

```c
#include "media_conv.h"
```

- 头文件：`include/media_conv.h`
- 模块映像：`plugins/libmedia_conv.so`（与 `libmedia_conv.so.sig` 必须同目录）
- 链接：`-lmedia_conv -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_conv.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("CONV_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

