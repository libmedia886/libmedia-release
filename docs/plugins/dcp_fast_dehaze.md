# dcp_fast_dehaze

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `DCP_FAST_DEHAZE` | FILTER |

基于暗通道先验（Dark Channel Prior）的快速去雾模块，后端为 OpenGL ES。产出
`libmedia_dcp_fast_dehaze.so` 与 [`include/media_dcp_fast_dehaze.h`](../../include/media_dcp_fast_dehaze.h)。
模块 type 名为 `DCP_FAST_DEHAZE`，实例名为 `DCP_FAST_DEHAZE_<grp>`，`MEDIA_DCP_FAST_DEHAZE_*`
API 与旧内置模块源码兼容。

## 功能

对单路 packed RGB 图像做去雾，算法流程沿用旧模块的 OpenCL 实现：

1. 按 `refine_scale` 最近邻下采样，得到小图 RGB 与灰度引导图
2. 小图上用可分离 min 滤波求 dark channel（窗口 `patch`）
3. 用小图 dark channel 直方图取 airlight 候选阈值，再按 16×16 tile 挑选
   `r+g+b` 最大的候选像素，逐级归约到 1×1
4. 以 airlight 归一化后重新求 dark channel，得到 transmission 初值 `t = 1 - omega * dark`
5. 小图上做引导滤波（打包均值 2 次 + `a/b` 平滑 2 次，共 4 次可分离盒滤波），得到 `a/b`
6. 全分辨率双线性上采样 `a/b`，`t = clamp(a * guide + b, t0, 1)`，按
   `out = (I - A) / t + A` 重建并夹到 `[0, 1]`。

实现要点：

- FILTER 插件，**自管模式**（`process = NULL`）：插件工作线程用 `MEDIA_PLUGIN_RecvFrame` 取帧
  GPU 提交后 `MEDIA_PLUGIN_EmitFrame` 输出；GLES 上下文在该工作线程创建、使用和销毁。
- 每帧只做一次 `MEDIA_GPU_Finish`，随后投递输出并归还输入；中间结果全部在 GPU 纹理内
  不做 CPU readback。输入经 `MEDIA_GPU_ImportBuffer` 导入为 `GL_TEXTURE_EXTERNAL_OES`
  输出导入为 `GL_TEXTURE_2D` 渲染目标。
  `MEDIA_GPU_LoadProgramBinary` 加载；生成物不进入发布包。
- 中间纹理统一为 `GL_RGBA16F`，复用一个 FBO；不创建逐帧 shader、不整帧 copy。
- 统计：`MEDIA_PLUGIN_RecordProcess` 记录每帧耗时，`MEDIA_PLUGIN_ReportStats` 上报 GPU
  耗时与 `gles <W>x<H> small=<w>x<h> frames= fail= imp=` 明细，`MEDIA_SYS_debug` 可见。

## type 名、端口与格式

| 项目 | 值 |
| --- | --- |
| 插件 type | `DCP_FAST_DEHAZE`（`MEDIA_DCP_FAST_DEHAZE_TYPE`） |
| 实例名 | `DCP_FAST_DEHAZE_<grp>` |
| 输入端口 | `input0`，1 路，`MEDIA_PORT_DIR_INPUT` |
| 输出端口 | `output0`，1 路，`MEDIA_PORT_DIR_OUTPUT` |
| 支持格式 | `MEDIA_FORMAT_RGB888`、`MEDIA_FORMAT_BGR888`，输入输出格式相同 |
| 压缩 | 仅 `MEDIA_VIDEO_COMPRESS_NONE`（LINEAR） |
| 尺寸 | 创建时固定为 `attr.width × attr.height`，运行中不可改 |

`RGB888` 在 GLES 内部映射为 `DRM_FORMAT_BGR888`（内存字节序 R,G,B），`BGR888` 映射为
`DRM_FORMAT_RGB888`（内存字节序 B,G,R）；着色器统一按逻辑 RGB 采样，不做通道交换。

## 配置字段

`MEDIA_DCP_FAST_DEHAZE_ATTR` 的字段名、类型与顺序与旧 `master:include/media_api.h` 完全一致。
`width`/`height`/`format`/`output_pool_id` 为必填项，其余可选参数在 `<= 0`（浮点为 `<= 0`）
时使用下表中的默认值。

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `width` | int | 必填 | 图像宽度（像素），必须 `> 0` |
| `height` | int | 必填 | 图像高度（像素），必须 `> 0` |
| `format` | int | 必填 | `MEDIA_FORMAT_RGB888` 或 `MEDIA_FORMAT_BGR888` |
| `input_depth` | int | 4 | `input0` 队列深度 |
| `output_pool_id` | int | 必填 | 输出 Buffer 池 ID，必须 `>= 0`，输出帧从该池取 |
| `input_stride` | int | 规范 pitch | 输入 plane0 行字节数；`<= 0` 时用 `MEDIA_VIDEO_BuildLinearLayout` 计算（pitch 64 字节对齐），显式值不得小于 `width * 3` |
| `output_stride` | int | 规范 pitch | 输出 plane0 行字节数，规则同上；同时决定输出 Buffer 的最小容量 |
| `patch` | int | 15 | dark channel 窗口尺寸（小图上按 `refine_scale` 缩放）；偶数自动 `+1` |
| `omega` | float | 0.95 | 雾保留系数，`t = 1 - omega * dark` |
| `t0` | float | 0.40 | transmission 下限，抑制噪声放大 |
| `airlight_percent` | float | 0.0005 | airlight 候选像素占小图比例，至少取 1 个像素 |
| `guided_radius` | int | 24 | 引导滤波半径（全分辨率像素），小图上按 `refine_scale` 缩放后夹在 `[1, 256]` |
| `guided_eps` | float | 1e-3 | 引导滤波 epsilon，抑制除零 |
| `refine_scale` | float | 0.5 | 小图缩放比例；小图尺寸 = `floor(W * scale)`，夹在 `[64, W]` / `[64, H]` |
| `passthrough` | int | 0 | 初始直通模式；非 0 时输入帧零拷贝直接输出，运行期可用 `SetPassthrough` 切换 |

## Pool 要求

- 输出池必须在实例处理帧之前创建，`output_pool_id` 由调用方管理，模块不创建、不销毁 Pool。
- 输出池每个 buffer 的容量必须 `>=` 按 `(width, height, format, output_stride)` 计算出的
  `output_size`（`MEDIA_VIDEO_BuildLinearLayout` 的 size；显式 stride 时为
  `output_stride * storage_height`）。处理时用 `MEDIA_POOL_GetSize` 检查，不足则本帧失败并归还输入。
- 输出池建议至少 4 个 slot，避免下游持有帧时上游取不到 buffer。buffer 必须是单 plane、
  可由 EGL 导入的 LINEAR DMA-BUF。
- 输入帧由调用方或上游模块提供，`metadata` 必须为 `MEDIA_BUFFER_META_VIDEO_FRAME`
  `media_format`、`visible_width/height`、`pitch_bytes[0]` 必须与配置一致，否则整帧被丢弃并归还。
- 所有权：`SendFrame` 成功后 buffer 归模块所有；`GetFrame` 得到的帧必须 `ReleaseFrame`
  直通模式下输出就是输入 buffer（同 `pool_id/index`），`ReleaseFrame` 后归还原输入池。

## 使用示例

以下示例假设进程已调用 `MEDIA_SYS_Init`；输出 Pool 的创建、销毁和 ID 都由调用方负责。

```c
#include <string.h>

#include "media_api.h"
#include "media_dcp_fast_dehaze.h"

#define MY_OUT_POOL 12

int setup_dehaze(int grp, int width, int height)
{
    MEDIA_DCP_FAST_DEHAZE_ATTR attr
    MEDIA_VIDEO_LAYOUT layout
    size_t size = 0
    if (MEDIA_VIDEO_BuildLinearLayout((uint32_t)width, (uint32_t)height
                                      MEDIA_FORMAT_RGB888, &layout, &size) != MEDIA_STATUS_OK) {
        return -1
    }
    if (MEDIA_POOL_CreateWithMode(MY_OUT_POOL, size, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) !=
        MEDIA_STATUS_OK) {
        return -1
    }

    memset(&attr, 0, sizeof(attr))
    attr.width = width
    attr.height = height
    attr.format = MEDIA_FORMAT_RGB888
    attr.output_pool_id = MY_OUT_POOL
    /* 其余字段留 0：patch=15、omega=0.95、t0=0.4、refine_scale=0.5 */

    if (MEDIA_DCP_FAST_DEHAZE_CreateGrp(grp, &attr) != MEDIA_STATUS_OK) {
        return -1
    }
    return MEDIA_DCP_FAST_DEHAZE_Start(grp)
}

void run_once(int grp, MEDIA_BUFFER input)
{
    MEDIA_BUFFER output
    /* 成功即转移所有权；失败时 buffer 仍归调用方 */
    if (MEDIA_DCP_FAST_DEHAZE_SendFrame(grp, input, 200) != MEDIA_STATUS_OK) {
        return
    }
    if (MEDIA_DCP_FAST_DEHAZE_GetFrame(grp, &output, 200) == MEDIA_STATUS_OK) {
        /* 此处使用 output */
        (void)MEDIA_DCP_FAST_DEHAZE_ReleaseFrame(grp, output)
    }
}

/* 也可以直接 Bind：上游 -> DCP_FAST_DEHAZE_<grp> -> 下游 */
int bind_pipeline(void)
{
    int ret
    ret = MEDIA_SYS_Bind("TPG", 0, "output0", "DCP_FAST_DEHAZE", 0, "input0")
    if (ret != MEDIA_STATUS_OK) {
        return ret
    }
    return MEDIA_SYS_Bind("DCP_FAST_DEHAZE", 0, "output0", "VO", 0, "input0")
}

void teardown_dehaze(int grp)
{
    (void)MEDIA_DCP_FAST_DEHAZE_Stop(grp)
    (void)MEDIA_DCP_FAST_DEHAZE_DestroyGrp(grp)
    (void)MEDIA_POOL_Destroy(MY_OUT_POOL)
}
```

运行时切换直通：`MEDIA_DCP_FAST_DEHAZE_SetPassthrough(grp, 1)`，不需要重启实例。
`Enable/Disable` 分别是 `Start/Stop` 的等价接口。

## 限制

- 只支持 packed 8-bit RGB888/BGR888，不支持 YUV、RAW、10-bit、alpha 或 compressed 输入输出。
- 只支持 LINEAR，不支持 AFBC；输出必须来自调用方指定的外部 Pool。
- 分辨率和格式在 `CreateGrp` 时固定，运行中不可改变；需要重新创建实例。
- 小图尺寸会向下取整，且最小 64；暗矩形边缘与 `refine_scale` 相关，边界附近会有 1~2 像素的过渡差异。
- `guided_radius` 在小图上的有效半径被夹在 `[1, 256]`；`patch` 在小图上最小 3。
- 没有 CPU fallback。`MEDIA_GPU_Finish` 失败时实例进入失败状态并停止取帧（输入 buffer 归还）
  需 `Stop` 后重新 `Start` 恢复。
- 每实例固定一个输入端口和一个输出端口，不支持多路或动态端口。

## 在发布包中使用

```c
#include "media_dcp_fast_dehaze.h"
```

- 头文件：`include/media_dcp_fast_dehaze.h`
- 模块映像：`plugins/libmedia_dcp_fast_dehaze.so`（与 `libmedia_dcp_fast_dehaze.so.sig` 必须同目录）
- 链接：`-lmedia_dcp_fast_dehaze -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_dcp_fast_dehaze.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("DCP_FAST_DEHAZE", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

