# resize_rga

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `RESIZE_RGA` | FILTER |

`libmedia_resize_rga.so`：用 RGA 硬件把输入帧裁剪 / 缩放到固定输出 Pool 的外部
模块，由原 libmedia 内置 `RESIZE_RGA` 模块迁出。

- 公共头：`include/media_resize_rga.h`（与原 `include/media_api.h` 中的
  `MEDIA_RESIZE_RGA_ATTR` / `MEDIA_RESIZE_RGA_*` 源码级兼容）
- 模块类型名：`RESIZE_RGA`，实例名 `RESIZE_RGA_<grp>`
- 端口：输入 `input0`、输出 `output0`（`MEDIA_SYS_Bind` 规则不变）
- 运行模式：自管插件（`process = NULL`），模块内部线程用
  `MEDIA_PLUGIN_RecvFrame` / `EmitFrame` 收发

## 使用

```c
#include "media_resize_rga.h"

MEDIA_SYS_Init
MEDIA_RESIZE_RGA_ATTR attr = {
    .src_x = 0, .src_y = 0
    .src_width = 1920, .src_height = 1080
    .input_width = 1920, .input_height = 1080
    .input_format = MEDIA_FORMAT_NV12
    .out_width = 640, .out_height = 360
    .output_format = MEDIA_FORMAT_NV12
    .output_pool_id = out_pool
    .keep_aspect_ratio = 1
}
MEDIA_RESIZE_RGA_CreateGrp(0, &attr)
MEDIA_RESIZE_RGA_Start(0)
MEDIA_BUFFER out
MEDIA_RESIZE_RGA_GetFrame(0, &out, 1000)
MEDIA_RESIZE_RGA_ReleaseFrame(0, out)
MEDIA_RESIZE_RGA_Stop(0)
MEDIA_RESIZE_RGA_DestroyGrp(0)
MEDIA_SYS_Exit
```

也可以作为插件加载后直接 Bind：

```c
MEDIA_SYS_Bind("TPG", 0, "output0", "RESIZE_RGA", 0, "input0")
```

调用任何 `MEDIA_RESIZE_RGA_*` 前必须先 `MEDIA_SYS_Init`。

## 能力与契约

- 格式：`NV12`、`YUV420SP_10BIT`（compact NV15）、`YUV422SP_10BIT`（compact
  NV20）、`RGB888`、`BGR888`、`RGBA8888`、`BGRA8888`、`ARGB8888`、`ABGR8888`。
- 尺寸 16..8192；输入 / 输出 byte stride 必须是 16 的倍数（0 表示按格式推导）。
- compact NV15/NV20 只允许输入输出同格式（不允许 NV12 ↔ 10bit 互转），宽度按
  16 对齐、高度为偶数，crop / 内容矩形按 64 像素对齐。byte stride 还必须能还原
  成 64 像素对齐的 10bit pixel stride：SDK 线性布局按 64 字节对齐 pitch，因此
  只有部分宽度可用（例如 256 宽 → pitch 320 可用；640 宽 → pitch 832 不可用）。
- 压缩：仅 NV12 AFBC16x16（compress 0x001 / 0x041），输入输出都必须是 NV12
  AFBC 下 stride 必须为 0，布局由 `MEDIA_VIDEO_QueryLayout` 决定。
- `keep_aspect_ratio = 1` 时按比例缩放并居中，四周填黑：LINEAR 用 `imfill`
  compact 10bit 在创建时预填输出池黑底。
- 输出 metadata：发布 `MEDIA_BUFFER_META_VIDEO_FRAME`，复制输入
  `time_stamp` / `seq`，`len` 为本格式所需字节数。
- 输出 Pool 由调用方创建；模块只在其中申请 / 归还 Buffer。

## 在发布包中使用

```c
#include "media_resize_rga.h"
```

- 头文件：`include/media_resize_rga.h`
- 模块映像：`plugins/libmedia_resize_rga.so`（与 `libmedia_resize_rga.so.sig` 必须同目录）
- 链接：`-lmedia_resize_rga -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_resize_rga.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("RESIZE_RGA", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

