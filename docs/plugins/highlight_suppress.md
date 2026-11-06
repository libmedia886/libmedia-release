# highlight_suppress

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `HIGHLIGHT_SUPPRESS` | FILTER |

NV12 高光软压制模块，仅使用 OpenGL ES 3 后端。产出
`libmedia_highlight_suppress.so` 与 [`include/media_highlight_suppress.h`](../../include/media_highlight_suppress.h)。
模块 type 名沿用旧内置模块的 `HIGHLIGHT_SUPPRESS`，实例名为
`HIGHLIGHT_SUPPRESS_<grp>`；`MEDIA_HIGHLIGHT_SUPPRESS_*` 函数名、参数、属性结构体名和
字段顺序与旧 `include/media_api.h` 一致，应用侧可直接使用原来的 Bind 名和调用方式。

## 功能与算法

模块读取 NV12 的 Y、UV 两个 LINEAR 平面，以单通道 R8 EGLImage 导入 GLES，按以下规则只改写
Y，UV 逐字节原样拷贝：

```text
yn      = Y / 255
chroma  = min(|U / 255 - 0.5| + |V / 255 - 0.5|, 1)
mask_y  = smoothstep(threshold_low, threshold_high, yn)
mask_c  = 1 - smoothstep(chroma_low, chroma_high, chroma)
mask    = clamp(mask_y * mask_c * strength, 0, 1)
soft    = yn > knee ? knee + (yn - knee) * ratio : yn
Yout    = clamp(yn + (soft - yn) * mask, 0, 1) * 255
```

低饱和区域中的高亮像素被压向软膝曲线，高饱和颜色和暗部基本保持不变。默认参数下
中性和 `Y=200` 的像素约输出 `181`，`Y=240` 约输出 `182`。

- 插件 kind：`FILTER`，自管模式。`Start` 创建固定工作线程，在该线程创建 GLES 上下文、
  收帧、绘制、等待 GPU 完成并发送输出。
- GPU 上下文通过公开 `MEDIA_GPU_*` 服务创建，EGLImage 使用 `MEDIA_GPU_GetEGLDisplay`
  返回的 display 导入；texture/FBO 在当前 GLES 上下文内创建。GLES program 在构建期由
- DMA-BUF surface 按 `(pool, index, Pool 代次, fd, size, layout)` 缓存，最多 32 项。稳态下
  每个参与处理的 Buffer 只导入一次；输入和输出分别缓存。
- 输出 metadata 写入真实 LINEAR 布局、`len`，并透传输入 `time_stamp` 和 `seq`。

## 模块名与端口

| 项目 | 值 |
| --- | --- |
| 模块 type | `HIGHLIGHT_SUPPRESS` |
| 实例名 | `HIGHLIGHT_SUPPRESS_<grp>` |
| 输入端口 | `input0` |
| 输出端口 | `output0` |
| 输入格式 | `MEDIA_FORMAT_NV12`，`MEDIA_VIDEO_COMPRESS_NONE`（LINEAR） |
| 输出格式 | `MEDIA_FORMAT_NV12`，`MEDIA_VIDEO_COMPRESS_NONE`（LINEAR） |
| 输入队列深度 | `input_depth`，默认 4 |

输入和输出的可见宽高相同，由创建属性固定。宽、高必须为正偶数。端口内部分别登记
`input_stride` 和 `output_stride`。

## 配置结构

`MEDIA_HIGHLIGHT_SUPPRESS_ATTR` 的字段顺序与旧 SDK 完全一致：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | `int` | 图像可见宽度，必须 `> 0` 且为偶数。 |
| `height` | `int` | 图像可见高度，必须 `> 0` 且为偶数。 |
| `format` | `int` | 只接受 `MEDIA_FORMAT_NV12`。 |
| `input_depth` | `int` | 输入端口队列深度；`<= 0` 时取 4。 |
| `output_pool_id` | `int` | 输出 Pool id；`< 0` 表示使用输入帧所在 Pool。 |
| `input_stride` | `int` | 输入 plane0 行字节；`<= 0` 时取 `width` 向上 64 字节对齐。必须覆盖 `width`。 |
| `output_stride` | `int` | 输出 plane0 行字节；`<= 0` 时等于 `input_stride`。必须覆盖 `width`。 |
| `threshold_low` | `float` | 高光起点，Y 归一化到 0..1；`<= 0` 取 0.58，随后钳制到 0..1。 |
| `threshold_high` | `float` | 高光终点；`<= 0` 取 0.86，随后钳制到 `threshold_low + 0.001`..1。 |
| `knee` | `float` | 软膝点；`<= 0` 取 0.68，钳制到 0..1。 |
| `ratio` | `float` | 膝点以上保留比例；`<= 0` 取 0.08，钳制到 0..1。 |
| `strength` | `float` | 压制强度；创建时 `<= 0` 取 0.94，钳制到 0..1。运行期可用 `SetStrength(0)` 关闭效果。 |
| `chroma_low` | `float` | 低饱和起点；`<= 0` 取 0.05，钳制到 0..1。 |
| `chroma_high` | `float` | 低饱和终点；`<= 0` 取 0.52，随后钳制到 `chroma_low + 0.001`..1。 |
| `passthrough` | `int` | 非 0 时只透传 Y/UV，不执行 GPU 像素运算；0 表示正常压制。 |

零初始化属性会使用上述默认参数，但 `width`、`height`、`format` 仍需显式填写。旧模块中
“数值 `<= 0` 时采用缺省值”的行为保留；因此需要在创建后关闭压制时，应在 `Start` 之后调用
`MEDIA_HIGHLIGHT_SUPPRESS_SetStrength(grp, 0.0f)`。

## Pool 与 Buffer 要求

1. 输入和输出 Pool 必须是可由 GLES 导入的 DMA-BUF Pool。推荐使用
   `MEDIA_POOL_CreateWithMode(..., MEDIA_POOL_ALLOC_MODE_NATIVE)`；`MEDIA_POOL_GetFd` 必须成功。
2. 每个 Buffer 的 capacity 至少覆盖按对应 stride 计算的 NV12 布局。推荐先用
   `MEDIA_VIDEO_QueryLayout(width, height, MEDIA_FORMAT_NV12, MEDIA_VIDEO_COMPRESS_NONE
   &layout, &size)` 得到 `size` 再建 Pool。输出 Buffer 小于插件计算出的输出大小时，该帧被丢弃。
3. `output_pool_id >= 0` 时使用独立输出 Pool；`output_pool_id < 0` 时输出也从输入 Pool 申请。
   这种同 Pool 模式至少要多留一个空闲 slot，避免输入尚未归还时无法取得输出。
4. 输入 Pool 的 slot 数应覆盖 `input_depth`、上游在途帧和下游共享引用；输出 Pool 应覆盖下游
   队列深度和在途输出。板上自测的 640x360 用例使用 4 个输入、4 个输出 slot。
5. 输入帧带 `MEDIA_BUFFER_META_VIDEO_FRAME` 时，其格式、宽高必须与属性一致，布局还必须通过
   `MEDIA_VIDEO_ValidateLayout` 且为 LINEAR。无 metadata 时按属性中的 `input_stride` 生成
   `valid_x=valid_y=0` 的 LINEAR 布局。
6. 输出 Pool 的实际 Buffer 必须能作为单通道 R8 渲染目标。插件在首次导入时会检查 FBO 完整性
   不满足条件的输出帧会被丢弃并计数。

## 使用示例

直接链接 `libmedia_highlight_suppress.so` 时，第一次调用 `MEDIA_HIGHLIGHT_SUPPRESS_CreateGrp`
会自动向 core 注册插件类；也可以把同一 `.so` 放入 `MEDIA_PLUGIN_PATH` 由 core 预加载。

```c
#include "media_api.h"
#include "media_highlight_suppress.h"

enum { IN_POOL = 1, OUT_POOL = 2 }
MEDIA_VIDEO_LAYOUT layout
size_t pool_size = 0
MEDIA_HIGHLIGHT_SUPPRESS_ATTR attr = {0}
MEDIA_SYS_Init
MEDIA_VIDEO_QueryLayout(1920, 1080, MEDIA_FORMAT_NV12
                        MEDIA_VIDEO_COMPRESS_NONE, &layout, &pool_size)
MEDIA_POOL_CreateWithMode(IN_POOL, pool_size, 8, MEDIA_POOL_ALLOC_MODE_NATIVE)
MEDIA_POOL_CreateWithMode(OUT_POOL, pool_size, 4, MEDIA_POOL_ALLOC_MODE_NATIVE)
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.output_pool_id = OUT_POOL
attr.input_stride = (int)layout.pitch_bytes[0]
attr.output_stride = (int)layout.pitch_bytes[0]
MEDIA_HIGHLIGHT_SUPPRESS_CreateGrp(0, &attr)
MEDIA_SYS_Bind("VI", 0, "output0", MEDIA_HIGHLIGHT_SUPPRESS_TYPE, 0
               MEDIA_HIGHLIGHT_SUPPRESS_INPUT_PORT)
MEDIA_HIGHLIGHT_SUPPRESS_Start(0)
/* 下游可 Bind output0，也可手动取帧。 */
MEDIA_BUFFER out
if (MEDIA_HIGHLIGHT_SUPPRESS_GetFrame(0, &out, 1000) == 0) {
    /* 使用 out；所有权在本调用者。 */
    MEDIA_HIGHLIGHT_SUPPRESS_ReleaseFrame(0, out)
}

MEDIA_HIGHLIGHT_SUPPRESS_Stop(0)
MEDIA_SYS_UnBind("VI", 0, "output0", MEDIA_HIGHLIGHT_SUPPRESS_TYPE, 0
                 MEDIA_HIGHLIGHT_SUPPRESS_INPUT_PORT)
MEDIA_HIGHLIGHT_SUPPRESS_DestroyGrp(0)
MEDIA_POOL_Destroy(OUT_POOL)
MEDIA_POOL_Destroy(IN_POOL)
MEDIA_SYS_Exit
```

手动送帧时，先从输入 Pool 取 Buffer、填好像素并写 NV12 metadata，再把所有权交给
`MEDIA_SYS_SendFrame(MEDIA_HIGHLIGHT_SUPPRESS_TYPE, grp, "input0", buf, timeout_ms)` 或
`MEDIA_HIGHLIGHT_SUPPRESS_SendFrame`。完整填充与布局校验代码见
`tests/test_highlight_suppress.c`。

运行期参数还可通过 `MEDIA_PLUGIN_SetParam` / `MEDIA_SYS_SetParam` 设置，键名为
`threshold_low`、`threshold_high`、`knee`、`ratio`、`strength`、`chroma_low`、`chroma_high`
（`float`）和 `passthrough`（`int`）。`GetLastPerf` 返回最近一帧耗时；GLES 后端的
`gpu_queue_ms` 恒为 `-1`。

## 限制与注意事项

- 仅支持 NV12、LINEAR；P010、NV16、AFBC 等格式会在创建或逐帧校验时被拒绝。
- 仅保留 GLES 后端，没有 OpenCL、Vulkan、NPU 或 CPU/LUT 回退。GLES 上下文、program 或
  EGLImage 初始化失败时，`Start` 返回 `MEDIA_ERR_UNSUPPORTED`（若初始化仍在 5 秒超时内未完成
  会先返回成功，之后由工作线程记录失败并停止处理）。
- 透传模式仍从输出 Pool 申请 Buffer 并由 CPU 拷贝 Y/UV，不是旧同 Pool 情况下的零拷贝引用转发
  此时 `gpu_kernel_ms=-1`。
- 输入 `valid_x`/`valid_y` 非零时，插件会把偏移加到 EGLImage 的 plane offset。驱动对 DMA-BUF
  导入 offset 有对齐要求，未对齐的输入可能导入失败并丢帧；生产输入建议使用
  `valid_x=valid_y=0`，或确认实际 offset 满足 Mali 驱动要求。
- 同一实例最多缓存 32 个 surface。不同 Pool、Buffer 组合很多且不断变化时会发生重新导入
  影响耗时但不会改变结果。
- `GetLastPerf` 的 `gpu_kernel_ms` 是两次绘制加 `MEDIA_GPU_Finish` 的墙钟时间，不是硬件
  kernel 计数器；GPU 争用会使其波动。

## 在发布包中使用

```c
#include "media_highlight_suppress.h"
```

- 头文件：`include/media_highlight_suppress.h`
- 模块映像：`plugins/libmedia_highlight_suppress.so`（与 `libmedia_highlight_suppress.so.sig` 必须同目录）
- 链接：`-lmedia_highlight_suppress -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_highlight_suppress.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("HIGHLIGHT_SUPPRESS", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

