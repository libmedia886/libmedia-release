# cap_dehaze

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `CAP_DEHAZE` | FILTER |

基于 CAP 深度先验与引导滤波的单帧去雾模块。产出 `libmedia_cap_dehaze.so` 和
[`include/media_cap_dehaze.h`](../../include/media_cap_dehaze.h)，只依赖公开 libmedia SDK、EGL/GLESv2、
pthread、libm 与 libc。GPU 路径只使用 OpenGL ES。

## 模块标识

| 项目 | 值 |
| --- | --- |
| 插件 type | `CAP_DEHAZE`（`MEDIA_CAP_DEHAZE_TYPE`） |
| 实例名 | `CAP_DEHAZE_<grp>` |
| 插件类别 | FILTER，自管模式 |
| 输入端口 | `input0`（`MEDIA_CAP_DEHAZE_INPUT_PORT`） |
| 输出端口 | `output0`（`MEDIA_CAP_DEHAZE_OUTPUT_PORT`） |
| 输入/输出格式 | `MEDIA_FORMAT_RGB888`、`MEDIA_FORMAT_BGR888` |
| 压缩 | 仅 `MEDIA_VIDEO_COMPRESS_NONE`（LINEAR） |
| 输出数量 | 1 |

应用可直接 `MEDIA_SYS_Bind("CAP_DEHAZE", grp, "output0", ...)`，也可通过
`MEDIA_CAP_DEHAZE_SendFrame` / `GetFrame` / `ReleaseFrame` 手动收发。公开 API 的函数名、参数、
`MEDIA_CAP_DEHAZE_ATTR` 字段名和字段顺序沿用旧 `media_api.h`，源码级兼容。

## 功能

- 由 RGB 计算 CAP 深度先验与透射率，在小分辨率上做两遍可分离引导滤波，再双线性上采样
  按大气散射模型恢复无雾图像并量化到 8 bit。
- 大气光估计、输入亮度统计和直通拷贝在主机侧完成；其余图像处理是 GPU 全屏三角形 pass。
- `passthrough` 创建时直通，也可由 `MEDIA_CAP_DEHAZE_SetPassthrough` 运行期切换。直通只做
  格式/stride 对应的逐行拷贝，不做去雾。
- 透射率细化比例可设为 0.25（默认）到 1.0。全分辨率（`refine_scale >= 0.999`）默认开启
  亮度补偿；可用环境变量 `MEDIA_CAP_DEHAZE_LUMA_COMPENSATION=0` 或 `=1` 在创建时覆盖。
- 工作线程创建、使用并销毁专用 GLES 上下文；每帧 `MEDIA_GPU_Finish` 确认 GPU 完成后才
  归还输入、发送输出。

## 配置字段

`MEDIA_CAP_DEHAZE_ATTR` 的完整字段如下。字段顺序不可改变；零初始化后按表中的默认语义工作。

| 字段 | 含义与取值 |
| --- | --- |
| `width` | 图像宽度，输入输出相同；允许 16..8192 |
| `height` | 图像高度，输入输出相同；允许 16..8192 |
| `format` | 仅 `MEDIA_FORMAT_RGB888` / `MEDIA_FORMAT_BGR888` |
| `input_depth` | 输入端口队列深度；`0` 表示默认 4 |
| `output_pool_id` | 输出 buffer 池 ID，必须 `>= 0` |
| `input_stride` | 输入 plane0 行字节数；`0` 表示 canonical pitch。非 0 时必须 `>= width * 3` 且为 64 的倍数 |
| `output_stride` | 输出 plane0 行字节数；`0` 表示 canonical pitch。非 0 时必须 `>= width * 3` 且为 64 的倍数 |
| `guided_radius` | 引导滤波半径；`<= 0` 表示默认 24，最终夹到不超过图像宽高 |
| `guided_eps` | 引导滤波 epsilon；`<= 0` 表示默认 `1e-3` |
| `t0` | 透射率下限；`<= 0` 表示默认 `0.12`，大于 1 时夹到 1 |
| `beta0` | CAP 深度常数项；严格等于 0 表示默认 `0.121779` |
| `beta1` | CAP 最大通道系数；严格等于 0 表示默认 `0.959710` |
| `beta2` | CAP 饱和度系数；严格等于 0 表示默认 `-0.780245` |
| `depth_scale` | 深度整体缩放；`<= 0` 表示默认 1.0 |
| `refine_scale` | 透射率细化缩放；`<= 0` 表示默认 0.25，大于 1 时夹到 1.0 |
| `passthrough` | 创建时非 0 表示直通；运行期用 `MEDIA_CAP_DEHAZE_SetPassthrough` 修改 |

`refine_scale` 先换算为小图宽高，小图每边最小 64 像素、最大不超过原图。例如 96x96 配
`refine_scale=0.5` 时仍使用 64x64 的引导滤波图。

## 输入、输出与 Pool 要求

- 输入和输出都是单 plane、紧凑 RGB/BGR 8 bit，visible 尺寸相同，不支持 AFBC、YUV、RAW 或
  其它压缩修饰符。
- 输入 Pool 和输出 Pool 都应使用 `MEDIA_POOL_ALLOC_MODE_NATIVE` 创建，使 core 能提供可被
   `MEDIA_GPU_ImportBuffer` 导入的 DMA-BUF。输出 Pool 的单个 buffer 容量必须不小于
   `output_stride * height`；`output_stride=0` 时即
   `MEDIA_VIDEO_BuildLinearLayout(width, height, format, ...)` 返回的 canonical size。
- 非 canonical pitch 必须是 64 的倍数：`MEDIA_GPU_ImportBuffer` 对线性 RGB 的 pitch 有该
  对齐要求（实测 328 被驱动拒绝、384 可导入）。该限制同时适用于输入和输出。
- 输出 Pool 由应用创建，`output_pool_id` 在创建模块时传入；Pool count 要覆盖下游队列深度、
  当前处理和尚未释放的输出。自测使用 3 个 slot。
- 输入 Queue 要覆盖 `input_depth` 与在途帧；上游 Pool count 还应覆盖所有下游引用。
- 输入帧最好带有效的 `MEDIA_BUFFER_META_VIDEO_FRAME`。若未设置 metadata，插件按配置的
  布局补齐；自定义 pitch 的帧必须提供 metadata，且 plane0 pitch 不小于 `width * 3`。
- 输出 metadata 会填好格式、visible 尺寸、按 `output_stride` 的 layout、`len`，并复制输入的
  `time_stamp` 与 `seq`。
- `MEDIA_CAP_DEHAZE_SendFrame` 成功即转移一个 buffer 引用；从模块取出的输出必须用
  `MEDIA_CAP_DEHAZE_ReleaseFrame` 归还。停止并销毁实例后，输入/输出 Pool 的 buffer 应全部回到
  Pool。

## 使用示例

```c
#include "media_api.h"
#include "media_cap_dehaze.h"

#define IMG_W 1920
#define IMG_H 1080

static int start_dehaze(int output_pool_id) {
    MEDIA_VIDEO_LAYOUT layout
    size_t size = 0
    MEDIA_CAP_DEHAZE_ATTR attr = { 0 }
    if (MEDIA_VIDEO_BuildLinearLayout(IMG_W, IMG_H, MEDIA_FORMAT_RGB888, &layout, &size) !=
        MEDIA_STATUS_OK) {
        return -1
    }
    if (MEDIA_POOL_CreateWithMode(output_pool_id, size, 4, MEDIA_POOL_ALLOC_MODE_NATIVE) != 0) {
        return -1
    }

    attr.width = IMG_W
    attr.height = IMG_H
    attr.format = MEDIA_FORMAT_RGB888
    attr.output_pool_id = output_pool_id
    /* 其余字段为 0：radius=24、eps=1e-3、t0=0.12、refine=0.25。 */
    if (MEDIA_CAP_DEHAZE_CreateGrp(0, &attr) != MEDIA_STATUS_OK) {
        return -1
    }
    if (MEDIA_SYS_Bind("VI", 0, "output0", MEDIA_CAP_DEHAZE_TYPE, 0
                       MEDIA_CAP_DEHAZE_INPUT_PORT) != MEDIA_STATUS_OK) {
        return -1
    }
    return MEDIA_CAP_DEHAZE_Start(0)
}

static int get_result(void) {
    MEDIA_BUFFER out
    int ret
    ret = MEDIA_SYS_GetFrame(MEDIA_CAP_DEHAZE_TYPE, 0, MEDIA_CAP_DEHAZE_OUTPUT_PORT
                             &out, 1000)
    if (ret != MEDIA_STATUS_OK) {
        return ret
    }
    /* 在 out 的有效期内读取或交给下游，然后归还。 */
    return MEDIA_SYS_ReleaseFrame(MEDIA_CAP_DEHAZE_TYPE, 0
                                  MEDIA_CAP_DEHAZE_OUTPUT_PORT, out)
}

static int stop_dehaze(void) {
    MEDIA_CAP_DEHAZE_Stop(0)
    MEDIA_SYS_UnBind("VI", 0, "output0", MEDIA_CAP_DEHAZE_TYPE, 0
                     MEDIA_CAP_DEHAZE_INPUT_PORT)
    return MEDIA_CAP_DEHAZE_DestroyGrp(0)
}
```

手动送帧时把上述 `MEDIA_SYS_Bind` 换成 `MEDIA_SYS_SendFrame(MEDIA_CAP_DEHAZE_TYPE, 0
MEDIA_CAP_DEHAZE_INPUT_PORT, buf, timeout_ms)` 即可。

## 限制

- 只支持 RGB888/BGR888 单路输入、单路输出；没有 YUV、AFBC、10 bit 或多路处理。
- GLES program binary 与构建机驱动指纹绑定，不能把一块板生成的 `.so` 当作跨驱动通用包。
  驱动指纹不匹配时 `Start` 失败，不使用 OpenCL/CPU 算法回退。
- 图像宽高限制为 16..8192。输入/输出都支持非 canonical pitch，但必须是 64 的倍数且不小于
  `width * 3`；非 canonical 输入帧必须由逐帧 metadata 描述实际 pitch。
- 引导滤波代价随 `guided_radius` 增长；全分辨率 `refine_scale=1.0` 还会增加中间纹理内存和
  每帧 pass 开销。
- 亮度补偿只在创建时读取环境变量；运行期没有亮度补偿开关。`MEDIA_CAP_DEHAZE_FAST_SUBMIT`
  等旧内部调优变量不再生效。
- 模块不做帧率控制、裁剪、缩放或色彩空间转换，输入输出 visible 尺寸与格式必须一致。

## 在发布包中使用

```c
#include "media_cap_dehaze.h"
```

- 头文件：`include/media_cap_dehaze.h`
- 模块映像：`plugins/libmedia_cap_dehaze.so`（与 `libmedia_cap_dehaze.so.sig` 必须同目录）
- 链接：`-lmedia_cap_dehaze -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_cap_dehaze.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("CAP_DEHAZE", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

