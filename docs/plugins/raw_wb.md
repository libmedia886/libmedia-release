# raw_wb

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `RAW_WB` | FILTER |

RAW16_LE Bayer 手动白平衡。单个 GPU 渲染 pass 按 CFA 位置给 R/G/B 三个通道乘不同增益，输出仍是
同一尺寸的 RAW16_LE Bayer（供下游 demosaic 使用）。本模块产出 `libmedia_raw_wb.so` 与
[`include/media_raw_wb.h`](../../include/media_raw_wb.h)，`MEDIA_RAW_WB_*` API 与原内置模块的源码接口一致。

- 插件 type 名：`RAW_WB`（`MEDIA_SYS_Bind("RAW_WB", ...)` / `MEDIA_PLUGIN_CreateModule` 都用它）
- 实例名：`RAW_WB_<grp>`
- 端口：`input0`、`output0`，均为 `MEDIA_FORMAT_BAYER_RAW16_LE`、单平面 LINEAR、小端 16bit

## 功能与实现

- 只保留 GLES 路径：DMA-BUF → `EGLImage` → `GL_TEXTURE_2D`，输出 Buffer 直接挂成 FBO 颜色附件
  全程零 CPU 拷贝、零回读。
- 自管模式 FILTER：插件工作线程内 `RecvFrame` → 绘制 → fence 等待 → `EmitFrame` / `ReturnFrame`
  `MEDIA_GPU_CreateContext` 创建的上下文同样在该线程创建、使用、销毁。
- 数值语义与旧 OpenCL kernel 一致：`gain == 1.0f` 时逐位复制原样本；否则按
  `round-to-even` 舍入并饱和到 0..65535。CFA 通道由 `bayer_pattern` 的 2x2 相位决定。
- EGLImage 按 `(pool, index, Pool 代次, fd, 大小, layout)` 缓存，Start 时预导入输出 Pool 的所有 slot
  稳态不再重复导入。
- 提交后没有确认 GPU 完成时，本帧输入/输出进入隔离（quarantine），不再处理新帧；Stop 时等 fence
  或 `MEDIA_GPU_Finish` 证明 GPU 已释放后才归还 Buffer。
  驱动生成，二进制与生成的 C 均不进入发布包。

## 配置结构体

`MEDIA_RAW_WB_ATTR` 字段（顺序与旧 `include/media_api.h` 一致）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | `int` | 有效宽（像素），1..8192 |
| `height` | `int` | 有效高（像素），1..8192 |
| `input_depth` | `int` | `input0` 的 Bind 队列深度，1..64 |
| `output_pool_id` | `int` | 应用预先创建的输出 Pool ID，Create 时即校验其全部 slot |
| `input_stride` | `int` | 输入行节拍（字节），64 字节对齐且 `>= width*2` |
| `output_stride` | `int` | 输出行节拍（字节），64 字节对齐且 `>= width*2` |
| `bayer_pattern` | `MEDIA_BAYER_PATTERN` | `BGGR` / `GBRG` / `GRBG` / `RGGB` |
| `r_gain` | `float` | 红色通道线性增益，有限且 `>= 0` |
| `g_gain` | `float` | 绿色通道线性增益，有限且 `>= 0` |
| `b_gain` | `float` | 蓝色通道线性增益，有限且 `>= 0` |

环境变量 `MEDIA_RAW_WB_BACKEND`：未设置或为 `gles` 时正常运行；设置为其它值返回
`MEDIA_ERR_UNSUPPORTED`（沿用旧 GLES-only 交付的行为）。

## 输入 / 输出与 Pool 要求

- 输入：应用自建 Pool（`MEDIA_POOL_ALLOC_MODE_NATIVE`，可导出 DMA-BUF），每个 Buffer 至少
  `input_stride * height` 字节；送帧前写好 pixels，并建议设置 `MEDIA_BUFFER_META` 视频帧 metadata
  （格式必须为 `MEDIA_FORMAT_BAYER_RAW16_LE`、pitch/offset 与配置一致，layout 需通过
  `MEDIA_VIDEO_ValidateLayout`）。没有 metadata 时按配置的行节拍处理。
- 输出：`output_pool_id` 必须在 Create 之前创建好，每个 Buffer 至少 `output_stride * height` 字节
  slot 数至少 1（建议 ≥ 3 以免生产者被阻塞）。输出 metadata 由插件填写：`MEDIA_FORMAT_BAYER_RAW16_LE`、
  canonical `DRM_FORMAT_R16`、可见尺寸与 storage 尺寸、`seq`/`time_stamp` 从输入继承。
- 两个 stride 都必须是 64 的整数倍：Mali 线性 DMA-BUF 导入要求 plane0 pitch 64 字节对齐
  （实测 `192/256/320` 可导入，`160/208/224` 直接返回 `EGL_BAD_ALLOC`）。不合规的配置在 Create
  阶段就返回 `MEDIA_ERR_INVALID_PARAM`。

## 使用示例

```c
#include "media_api.h"
#include "media_raw_wb.h"

#define IN_POOL  1
#define OUT_POOL 2

static const int kWidth = 1920
static const int kHeight = 1080
static const int kInStride = 3840;   /* 64 字节对齐 */
static const int kOutStride = 3840
static int raw_wb_demo(void) {
    MEDIA_RAW_WB_ATTR attr
    MEDIA_BUFFER in
    MEDIA_BUFFER out
    MEDIA_SYS_Init
    MEDIA_POOL_CreateWithMode(IN_POOL, (size_t)kInStride * kHeight, 4
                              MEDIA_POOL_ALLOC_MODE_NATIVE)
    MEDIA_POOL_CreateWithMode(OUT_POOL, (size_t)kOutStride * kHeight, 4
                              MEDIA_POOL_ALLOC_MODE_NATIVE)
    memset(&attr, 0, sizeof(attr))
    attr.width = kWidth
    attr.height = kHeight
    attr.input_depth = 3
    attr.output_pool_id = OUT_POOL
    attr.input_stride = kInStride
    attr.output_stride = kOutStride
    attr.bayer_pattern = MEDIA_BAYER_PATTERN_RGGB
    attr.r_gain = 1.8f
    attr.g_gain = 1.0f
    attr.b_gain = 1.5f
    if (MEDIA_RAW_WB_Create(0, &attr) != MEDIA_STATUS_OK) {
        return -1
    }
    MEDIA_RAW_WB_Start(0)
    /* 生产者：填充 RAW16_LE 后送入 input0；SendFrame 成功即转移所有权。 */
    if (MEDIA_POOL_GetBuffer(IN_POOL, &in) == MEDIA_STATUS_OK) {
        /* ... 写 in 的像素与 metadata ... */
        if (MEDIA_SYS_SendFrame(MEDIA_RAW_WB_TYPE, 0, "input0", in, 1000) !=
            MEDIA_STATUS_OK) {
            MEDIA_POOL_PutBuffer(in)
        }
    }

    /* 消费者：取一帧并按需释放。 */
    if (MEDIA_RAW_WB_GetFrame(0, &out, 1000) == MEDIA_STATUS_OK) {
        /* ... 使用 out ... */
        MEDIA_RAW_WB_ReleaseFrame(0, out)
    }

    MEDIA_RAW_WB_Stop(0)
    MEDIA_RAW_WB_Destroy(0)
    MEDIA_POOL_Destroy(IN_POOL)
    MEDIA_POOL_Destroy(OUT_POOL)
    MEDIA_SYS_Exit
    return 0
}
```

也可以走 Bind：`MEDIA_SYS_Bind("RAW_WB", 0, "input0", "XXX", grp, "output0")`，或
`MEDIA_SYS_Bind("RAW_WB", 0, "output0", "XXX", grp, "input0")`，由框架负责搬运与引用计数。

## 限制

- 只有 GLES 后端，没有 OpenCL / Vulkan / NPU / CPU 回退。
- 只接受单平面 LINEAR `MEDIA_FORMAT_BAYER_RAW16_LE`，不支持 AFBC、10bit 打包格式、多平面或多路输入。
- 两个 stride 必须 64 字节对齐（Mali 线性 DMA-BUF 导入限制）；不合规配置在 Create 时被拒绝。
- 配置在 Create 后不可修改：宽高、stride、Bayer 相位、增益都不能运行期调整，改配置需 Destroy 后重建。
- 输出 Pool 必须能导出 DMA-BUF（`MEDIA_POOL_ALLOC_MODE_NATIVE`）；CPU 内存 Pool 会在 Create 时被拒绝。
- `MEDIA_RAW_WB_ReleaseFrame` 与旧 glue 一样忽略 `grp`，直接对 Buffer 做引用计数减一。

## 在发布包中使用

```c
#include "media_raw_wb.h"
```

- 头文件：`include/media_raw_wb.h`
- 模块映像：`plugins/libmedia_raw_wb.so`（与 `libmedia_raw_wb.so.sig` 必须同目录）
- 链接：`-lmedia_raw_wb -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_raw_wb.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("RAW_WB", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

