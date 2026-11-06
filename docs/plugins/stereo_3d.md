# stereo_3d

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `STEREO_3D` | FILTER |

GLES 双目合成：把两路 NV12 图像合成一路 NV12，支持"左右并排"与"隔行交替"两种排列
其中一路可按任意角度绕画面中心旋转（旋转后源图以外的区域填 0）。产出 `libmedia_stereo_3d.so`
与 [`include/media_stereo_3d.h`](../../include/media_stereo_3d.h)；`MEDIA_STEREO_3D_*` 的函数名、参数与
`MEDIA_STEREO_3D_ATTR` 字段顺序与原内置 STEREO_3D 完全一致（源码级兼容，不保证二进制兼容）。

- 插件 type 名（Bind / CreateModule 用）：`STEREO_3D`，实例名 `STEREO_3D_<grp>`
- 输入端口：`input0`、`input1`；输出端口：`output0`
- 后端：OpenGL ES 3（RK3588 Mali-G610 实测），只用公开 SDK 的 `MEDIA_GPU_*`、`MEDIA_POOL_*`、
  `MEDIA_PLUGIN_*`；不含 OpenCL / Vulkan / NPU / CPU fallback

## 设计

- FILTER 插件、**自管模式**（`process = NULL`）：插件工作线程用 `MEDIA_PLUGIN_RecvFrame` 取两路输入
  `MEDIA_GPU_CreateContext` 建本实例上下文（共享进程级 GBM EGLDisplay），一次绘制 Y、UV 两张平面
  `MEDIA_GPU_Finish` 后 `MEDIA_PLUGIN_EmitFrame` 输出、`MEDIA_PLUGIN_ReturnFrame` 归还输入。
- **零拷贝**：DMA-BUF 直接 `eglCreateImageKHR`（DRM_FORMAT_R8）→ `glEGLImageTargetTexture2DOES`
  → texture / FBO。NV12 的 UV 平面用 `pitch_bytes[0] * height` 作为 DMA-BUF 内偏移单独导入
  因此 Y / UV 各是一张 R8 纹理，不依赖 libdrm，也不需要 CPU 拆平面。
  shader 源码不进入发布包，构建期生成的 `.c` 文件只在 `build/` 中生成、不进入发布包。
- **缺帧复用**：每个输入端口扣留最近一帧（`MEDIA_PLUGIN_RetainFrame`）。某回合只收到一路时
  另一路用扣留的历史帧；两路都缺则丢弃该回合。Stop/Destroy 在这些历史帧归还后才退出。
- 统计：每帧 `MEDIA_PLUGIN_RecordProcess`；`MEDIA_PLUGIN_ReportStats` 上报 `has_gpu=1`、
  `gpu_kernel_ms`（首绘 → `MEDIA_GPU_Finish`）、`gpu_queue_ms=-1`，detail 形如
  `gles mode=0 rotate=1 frames=42 err=0`。

## 合成语义

两路输入中"直通"的一路记作 direct，"被旋转"的一路记作 rotated，由 `rotate_input` 决定：

| rotate_input | direct | rotated |
| --- | --- | --- |
| 0 | input1 | input0 |
| 1 | input0 | input1 |

`MEDIA_STEREO_3D_MODE_SIDE_BY_SIDE`（0）：输出左半幅取 direct、右半幅取 rotated，两半都按 2 倍
横向拉伸回整幅宽度（`sample_x = (x + 0.5) * W / (W / 2) - 0.5`，最近邻采样）。

`MEDIA_STEREO_3D_MODE_LINE_BY_LINE`（1）：偶数行取 direct 原位置，奇数行取 rotated；UV 平面在每个
2×2 块上取两路 UV 的平均值（与原 OpenCL kernel 一致）。

旋转以画面中心 `((W-1)/2, (H-1)/2)` 为轴，反变换 `src = R(-θ)·(dst - center) + center`，正角度使该路
画面顺时针旋转（图像坐标 y 向下）。Y 平面反变换后越界直接填 0；UV 平面越界按边缘钳制后再配对采样。
输出 UV 的偶数列写 U、奇数列写 V。

## 配置字段（MEDIA_STEREO_3D_ATTR）

| 字段 | 说明 |
| --- | --- |
| `width` / `height` | 图像宽高（像素）。必须为偶数，范围 [2, 8192]；两路输入与输出同尺寸 |
| `format` | 输入/输出格式，目前只接受 `MEDIA_FORMAT_NV12`，其它格式在 create 时被拒绝 |
| `input_depth` | 两个输入端口的队列深度；`<= 0` 取默认值 4 |
| `output_pool_id` | 输出 buffer 所在 Pool ID，必须 `>= 0`；插件不创建 Pool |
| `input_stride` | 输入 Y/UV 行跨度；`<= 0` 取 `align64(width)`，否则须 `>= width`、64 字节对齐、`<= 32768` |
| `output_stride` | 输出 Y/UV 行跨度；规则同上，同时决定输出 metadata 的 pitch 与 UV 偏移 |
| `mode` | `MEDIA_STEREO_3D_MODE_SIDE_BY_SIDE`(0) 或 `MEDIA_STEREO_3D_MODE_LINE_BY_LINE`(1) |
| `rotate_input` | 0 = 旋转 input0，1 = 旋转 input1 |
| `rotation_degrees` | 旋转角度（度，float），可运行期用 `MEDIA_STEREO_3D_SetRotation` 修改，下一帧生效 |

短配置（例如旧结构体只写到 `rotate_input`）按尾部清零处理；长度不足 `8 * sizeof(int)` 时 create 失败。

## 输出 metadata

每帧输出都重新填写 `MEDIA_BUFFER_META`：`media_format = MEDIA_FORMAT_NV12`、
`len = output_stride * height * 3 / 2`、`u.video` 为 `plane_count = 2` 的 LINEAR 布局
（pitch = `output_stride`，`offset_bytes = {0, output_stride * height}`，`drm_fourcc` 取自
`MEDIA_VIDEO_GetDrmFourcc`，`drm_modifier = 0`），`time_stamp` / `seq` 透传 input0。发布前用
`MEDIA_VIDEO_ValidateLayout` 自检；Pool slot 太小会在运行期报错并丢帧。

## Pool 要求

- 两路输入各需一个 Pool（DMA-BUF，`MEDIA_POOL_ALLOC_MODE_NATIVE`/`COMPAT`；HOST 内存会被拒绝）。
  slot 大小 `>= input_stride * height * 3 / 2`；slot 数建议 `>= input_depth + 3`：队列里最多
  `input_depth` 帧、插件扣留 1 帧历史、外加 1 帧正在处理。
- 输出需要一个 Pool，slot 大小 `>= output_stride * height * 3 / 2`，slot 数建议 `>= 4`（至少 2：
  一帧在管线上、一帧在调用方手里）。多个分组可共用输出 Pool，但必须满足各自的 stride 需求。
- 输入 buffer 的逐帧 metadata 优先：pitch 取 `meta.u.video.pitch_bytes[]`，UV 偏移取
  `offset_bytes[1]`，缺失时才回退到配置的 stride 与 `input_stride * height`。

## 使用示例

```c
#include <string.h>
#include "media_api.h"
#include "media_stereo_3d.h"

MEDIA_STEREO_3D_ATTR attr
memset(&attr, 0, sizeof(attr))
attr.width = 1920
attr.height = 1080
attr.format = MEDIA_FORMAT_NV12
attr.output_pool_id = POOL_OUT;              /* 事先建好的输出 Pool */
attr.mode = MEDIA_STEREO_3D_MODE_SIDE_BY_SIDE
attr.rotate_input = 1;                       /* 旋转 input1 */
attr.rotation_degrees = 3.5f
MEDIA_STEREO_3D_CreateGrp(0, &attr)
MEDIA_SYS_Bind("SRC_LEFT", 0, "output0", "STEREO_3D", 0, "input0")
MEDIA_SYS_Bind("SRC_RIGHT", 0, "output0", "STEREO_3D", 0, "input1")
MEDIA_STEREO_3D_Start(0);                    /* 建工作线程 + GLES 上下文 */

MEDIA_BUFFER out
if (MEDIA_STEREO_3D_GetFrame(0, &out, 1000) == 0) {
    /* 用 MEDIA_BUFFER_GetMeta 取布局，或用 MEDIA_POOL_GetFd 直接送显示/编码 */
    MEDIA_STEREO_3D_ReleaseFrame(0, out)
}

MEDIA_STEREO_3D_SetRotation(0, -3.5f);       /* 运行期改角度，下一帧生效 */
MEDIA_STEREO_3D_Stop(0)
MEDIA_STEREO_3D_DestroyGrp(0)
```

手动送帧的场合下 `MEDIA_SYS_SendFrame("STEREO_3D", grp, "input0" | "input1", buf, timeout)`
发送成功即转移所有权，失败时调用方仍持有 buffer。

## 限制

- 只支持 `MEDIA_FORMAT_NV12` + LINEAR（不支持 AFBC 与其它格式）；两路输入必须与配置同尺寸。
- 宽高必须为偶数且在 [2, 8192]，行跨度必须 64 字节对齐且 `<= 32768`。
- 只支持 DMA-BUF 输入，HOST 内存 buffer 会被拒绝。
- 每个分组一个工作线程与一个 GLES 上下文；未做 EGLImage/FBO 缓存，每帧导入并销毁 6 张
  EGLImage，1080p 下开销可接受但不适合极高帧率的小尺寸场景。
- 一路输入长时间缺失时会持续复用历史帧；两路都无可用帧的回合直接丢帧，上游需要能接受丢帧。
- `mode` / `rotate_input` / 尺寸只能在 create 时确定，运行期只能改 `rotation_degrees`。
- 输出的 UV 平面固定按 canonical NV12 偏移（`output_stride * height`）写入。

## 在发布包中使用

```c
#include "media_stereo_3d.h"
```

- 头文件：`include/media_stereo_3d.h`
- 模块映像：`plugins/libmedia_stereo_3d.so`（与 `libmedia_stereo_3d.so.sig` 必须同目录）
- 链接：`-lmedia_stereo_3d -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_stereo_3d.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("STEREO_3D", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

