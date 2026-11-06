# vmix

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `VMIX` | FILTER |

GLES 多路视频混合/拼接：最多 16 路输入按通道布局（位置、目标尺寸、alpha）合成为一路输出。产出
`libmedia_vmix.so` 与 [`include/media_vmix.h`](../../include/media_vmix.h)；`MEDIA_VMIX_*` API 与原内置 VMIX
源码兼容（结构体、枚举、函数名、字段顺序均未变），type 名 `VMIX`，实例名 `VMIX_<grp>`。

> core 内仍内置的 `VMIX_RGA`（RGA 硬件合成，type `VMIX_RGA`）是独立模块，不属于本模块，两者不共享代码、
> 格式列表或限制。

## 模块接口

| 项 | 说明 |
| --- | --- |
| type | `VMIX`（FILTER，自管模式） |
| 输入端口 | `input0` … `input<input_count-1>`，**端口序号等于通道号**；宽高声明为 0（接受任意尺寸的源） |
| 输出端口 | `output0`，尺寸 `output_width × output_height`，格式 `format`，压缩 `output_compress` |
| Bind | `MEDIA_SYS_Bind("VMIX", grp, "output0", ...)`、`MEDIA_SYS_Bind(src, id, "outputN", "VMIX", grp, "input<ch>")` |
| 手动 | 输入 `MEDIA_SYS_SendFrame("VMIX", grp, "input<ch>", buf, timeout)`；输出 `MEDIA_VMIX_GetFrame / ReleaseFrame` |
| 插件用法 | `MEDIA_PLUGIN_Load("libmedia_vmix.so")` 后 `MEDIA_PLUGIN_CreateModule("VMIX", grp, &attr, sizeof(attr))`，config 即 `MEDIA_VMIX_ATTR` |

API：`MEDIA_VMIX_SetAttr / CreateGrp`（创建，二者等价）、`DestroyGrp`、`Start / Enable`、`Stop / Disable`、
`GetFrame`、`ReleaseFrame`。直接链接 `libmedia_vmix.so` 时首次调用会自动向 libmedia 注册插件类，无需先 Load。

### 属性（`MEDIA_VMIX_ATTR`）

| 字段 | 含义 |
| --- | --- |
| `channels[i]` | `enabled / x / y / width / height / alpha / stride / format / compress`；`width/height` 是目标矩形，同为 0 表示按源 visible 尺寸、不缩放；`format` 为 0 或 -1 时取全局 `format`；`stride` 为 LINEAR 输入的 byte pitch（0 = 不限定，逐帧以 metadata 为准；非 0 时必须与帧 pitch 相同）；`compress` 为该输入的精确存储契约 |
| `input_count` | 1–16，必须至少有一个 enabled 通道 |
| `output_width / output_height` | 16–8192 |
| `format` | `output0` 格式，同时是通道的默认输入格式 |
| `input_depth` | 每个输入端口的 Bind 队列深度，0 = 4 |
| `output_pool_id` | 输出 Buffer 池，**应用创建**；每个 slot 必须是足够大的 DMA-BUF |
| `output_stride` | LINEAR 输出 byte pitch，0 = 格式默认对齐（NV12 16 字节，紧凑 10bit / packed RGB 64 字节） |
| `output_compress` | `output0` 精确存储契约；非 0 时 `output_stride` 必须为 0 |
| `primary_index` | 主输入（触发节奏与时间轴来源）；-1 = 第一个 enabled 通道 |

### 格式与压缩

- 输入/输出格式（十种，group 创建后固定）：`NV12`、紧凑 10bit `YUV420SP_10BIT`(NV15) / `YUV422SP_10BIT`(NV20) /
  `YUV444SP_10BIT`(NV30)、`RGB888`、`BGR888`、`RGBA8888`、`BGRA8888`、`ARGB8888`、`ABGR8888`
  每个 enabled 通道可独立选择格式，输出也可选其一。
- LINEAR：紧凑 10bit 与 packed RGB 的 pitch 必须 64 字节对齐；NV15/NV20/NV30 的色度采样分别为 4:2:0、4:2:2、
  4:4:4，UV offset = `luma_pitch × height`，NV15/NV20 的 UV pitch 等于 Y pitch，NV30 为其两倍。
- AFBC：仅 `NV12`、`BGR888`、`ABGR8888` 可用，精确 `format×compress` 由 core 的共享 GLES 事实源
  （`MEDIA_GPU_CompressSupported`）决定（输入 51 个 tuple、输出 47 个 tuple），不在其内的组合在创建时拒绝。
  AFBC 输入逐帧接受合法的 producer-specific layout（storage/valid origin/pitch/offset）；AFBC 输出 Pool 必须按
  `MEDIA_VIDEO_QueryLayout` 的真实 allocation size 创建（`MEDIA_POOL_ALLOC_MODE_NATIVE`），VMIX 整帧直接作为 FBO 渲染。
- 色彩：RGB↔YUV 沿用原有矩阵与 range 语义——**全量程 BT.601**（JPEG 系数，无 16/235 缩放），NV12 层转 NV12 输出时
  原样直通。当前 metadata 不含 colorimetry，不自动识别 BT.709/BT.2020 或 limited/full range。NV12 ↔ YUV10
  code value 左移/四舍五入右移两位。
- packed RGB 的 `MEDIA_FORMAT_*` 名称表示内存字节序（例如 `ABGR8888` 内存为 A,B,G,R）。

### 合成语义

- 主输入（`primary_index`）每到一帧立即触发一次合成，输出一帧；其余 enabled 通道以 `timeout=0` 取队列里已有的
  一帧，**没有新帧就复用上一帧**（VMIX 持有各路最近一帧直到被替换或 Stop），首帧缺失的通道不参与本次合成，不阻塞。
- 绘制顺序：主输入最先，其余 enabled 通道按通道号递增；像素 alpha（RGBA 类输入）与通道 alpha 相乘后做 source-over
  同时作用于亮度与色度。画布外/负坐标区域被裁剪；目标尺寸与源尺寸不同时由 fragment 纹理采样缩放。背景初始为黑。
- 输出 metadata 完整发布真实 layout、`len`，`time_stamp/seq` 取自主输入当帧（主输入缺席时取第一个有效层）。
- 每帧提交前校验真实 `VIDEO_FRAME`：格式、完整 compression、visible/storage/valid、FourCC、plane/pitch/offset、
  `len`、DMA-BUF fd 与 capacity，任一不符整帧拒绝（不 fallback）。
- 未启用（`enabled=0`）通道的输入端口仍然存在（端口序号 = 通道号），收到的帧立即丢弃、不占用上游 Pool。

### Buffer / Pool 要求

- 输出 Pool：应用创建，slot 容量 ≥ 输出布局所需大小（LINEAR 由 `output_stride`/默认 stride 与高度算出，AFBC 由
  `MEDIA_VIDEO_QueryLayout`）；创建 group 时逐 slot 预检 DMA-BUF 与容量；count 要覆盖下游队列深度与在途数量。
- 输入 Pool（上游的）：VMIX 为每个 enabled 通道**多持有 1 帧**（上一帧），Pool 的 slot 数需在原有在途数量之上再留余量
  Stop / Disable 时全部归还。
- Stop 后所有输入帧、输出帧均已归还（测试以 `MEDIA_BUFFER_GetRefCount` 逐 slot 校验）。
- 每帧只在 GPU 完成（单次 fence）后才归还 Buffer；fence 超时的 Buffer 进入隔离区，不会交回 Pool，Stop 时重试。

### 环境变量

`MEDIA_VMIX_BACKEND`：为空或 `gles` 选择 GLES（唯一后端）；其它非空值在创建时返回错误。

## 使用示例

```c
#include "media_api.h"
#include "media_tpg.h"
#include "media_vmix.h"

MEDIA_VMIX_ATTR a = {0}
a.input_count = 2
a.output_width = 1280
a.output_height = 720
a.format = MEDIA_FORMAT_NV12
a.output_pool_id = OUT_POOL
a.primary_index = -1
a.channels[0] = (MEDIA_VMIX_CHANNEL){ .enabled = 1, .alpha = 1.0f, .format = MEDIA_FORMAT_NV12 }
a.channels[1] = (MEDIA_VMIX_CHANNEL){ .enabled = 1, .x = 640, .y = 360, .width = 320, .height = 180
                                      .alpha = 1.0f, .format = MEDIA_FORMAT_ABGR8888 }
MEDIA_VMIX_CreateGrp(0, &a)
MEDIA_SYS_Bind("TPG", 0, "output0", "VMIX", 0, "input0")
MEDIA_SYS_Bind("TPG", 1, "output0", "VMIX", 0, "input1")
MEDIA_VMIX_Enable(0)
MEDIA_BUFFER out
if (MEDIA_VMIX_GetFrame(0, &out, 1000) == 0) {
    /* 读 out 的 metadata / 像素 ... */
    MEDIA_VMIX_ReleaseFrame(0, out)
}
```

## 设计

- FILTER 插件、**自管模式**：插件工作线程在主输入上 `RecvFrame` 等帧，其余通道 `RecvFrame(timeout=0)`，一次 GPU
  提交合成全部图层并编码进输出 DMA-BUF，只等一次 fence，再 `EmitFrame` 输出、`ReturnFrame`
  被替换的旧帧。GPU 上下文（`MEDIA_GPU_CreateContext`，共享进程级 GBM EGLDisplay）在该线程创建、使用和销毁。
- 引擎：各层先合成到 RGBA16F 中间纹理（YUV 输出时为 10bit 量纲的 YUV 域），再由 encode shader 写出输出：LINEAR
  输出写入内部 R8 attachment 后用 `glCopyImageSubData` 拷到导入的输出 plane（Mali 对导入 R8/NV15 FBO 直接写
  不可靠），AFBC 输出直接渲染到整帧 EGLImage 的 FBO。
- 零拷贝 + 缓存：DMA-BUF → EGLImage → texture，按 `(pool, index, Pool 代次, plane, fd, size, format, layout)` 缓存
  AFBC 输出的每个 slot 在 Start 时预导入，稳态零导入、零 CPU copy/map/readback，不按帧编译 shader。
  `MEDIA_GPU_LoadProgramBinary` 加载；shader 源码不进入发布包。**必须在目标板上构建**。
- 故障隔离：GPU 提交后无法确认完成（fence 超时）时，本帧输入与输出都不归还，模块停止处理新帧并在统计中显示
  `FAILED`；Stop 时重试等待，确认 GPU 不再访问后才归还，仍失败则 Stop 返回错误。
- 统计：`MEDIA_PLUGIN_RecordProcess` 记录每次合成耗时，`MEDIA_PLUGIN_ReportStats` 上报 GPU 耗时与
  `cache/imp/draw/copy/wait/strong/err` 计数（`MEDIA_SYS_debug` 可见）。

## 限制

- 仅 GLES，无 CPU / Vulkan / OpenCL 后端，也无 fallback；需要 EGL dma-buf import（含 modifiers）、`GL_EXT_copy_image`
  （或 OES）、`GL_EXT_color_buffer_half_float/float`；AFBC YUV 还需 `GL_OES_EGL_image_external_essl3` + `GL_EXT_YUV_target`。
- 创建失败时 `MEDIA_VMIX_CreateGrp` 统一返回 `MEDIA_ERR_RESOURCE`（框架不透传插件 `create` 的具体错误码），具体原因见 stderr 日志。
- 驱动升级后 program binary 指纹不匹配会拒绝加载，需要重新构建本模块。

## 在发布包中使用

```c
#include "media_vmix.h"
```

- 头文件：`include/media_vmix.h`
- 模块映像：`plugins/libmedia_vmix.so`（与 `libmedia_vmix.so.sig` 必须同目录）
- 链接：`-lmedia_vmix -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vmix.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VMIX", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

