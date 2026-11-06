# rga

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `RGA` | FILTER |

RGA 硬件图形模块，提供拷贝、缩放、颜色空间转换、旋转、翻转、平移、裁剪与多路合成。
本模块是从 `libmedia` core 迁出的外部模块，只依赖公开 SDK（`libmedia.so` + `media_plugin.h`）
与 `librga`（im2d），不依赖 core 内部头或其它插件。

## 类型与端口

- 插件类型名：`RGA`；实例名：`RGA_<id>`（`id` 即 API 的 `chn`/`grp`）。
- 端口：`input0..3`、`output0..3`（实际个数由 `MEDIA_RGA_GRP_ATTR.input_count/output_count` 决定）。
- `MEDIA_SYS_Bind("RGA", id, "output0", ...)` 等规则与迁移前一致。

## 算法

`MEDIA_RGA_ALG` 与能力：

| 算法 | 输入/输出 | 说明 |
| --- | --- | --- |
| `RESIZE` | 1→1 | `crop_*` + 目标宽高缩放；同尺寸同格式退化为 copy |
| `CSC` | 1→1 | NV12 ↔ 打包 RGB，`IM_COLOR_SPACE_DEFAULT` |
| `ROTATE` | 1→1 | `rotate` = 0/90/180/270 |
| `COPY` | 1→1 | 同格式同尺寸拷贝 |
| `TRANSLATE` | 1→1 | `dst_x/dst_y` 平移 |
| `FLIP` | 1→1 | `flip_h` / `flip_v` |
| `CROP` | 1→1 | 按 `crop_*` 裁剪 |
| `COMPOSE` | 1..4→1 | 1 路 copy、2 路 `imcomposite`（SRC_OVER）、多路逐层 blit |
| `OSD` | 2→2 | 前景（input0）叠到背景（input1），`imcomposite(..., SRC_OVER)` |
| `MOSAIC` / `PYRAMID` | — | 本平台不支持，Create 阶段拒绝 |

## 格式与压缩契约

- 格式：`NV12`、`RGB888`、`BGR888`、`RGBA8888`、`BGRA8888`、`ARGB8888`、`ABGR8888`。
- 尺寸 16..8192；`w_align = h_align = 1`；LINEAR `stride` 按 byte 配置，`stride_align = 1`。
- 压缩只识别 `NONE`、AFBC16x16 `0x001`、AFBC16x16+SPARSE `0x041`；非零 compression 只允许 NV12
  且端口 `stride` 必须为 0。AFBC 几何门槛：宽高为偶数、visible width ≥ 68、height ≥ 16。
- 逐方向矩阵：
  - copy / resize / rotate / flip / crop：`LINEAR→AFBC`、`AFBC→LINEAR`、同 modifier 的 `AFBC→AFBC`
    `0x001↔0x041` 拒绝。
  - translate：只开放 `AFBC→LINEAR`，且目标 `dst_x/dst_y` 为偶数。
  - 压缩 CSC：NV12 AFBC → LINEAR `RGB888/BGR888/RGBA8888/BGRA8888`；六种 LINEAR RGB family →
    NV12 AFBC。NV12 AFBC → `ARGB8888/ABGR8888` 不开放。
  - compose / OSD：任一压缩端在 Create 阶段拒绝；mosaic / pyramid 平台不支持。
- AFBC 输入必须发布完整 `MEDIA_BUFFER_META_VIDEO_FRAME`，`meta.len` 与真实 DMA-BUF capacity
  精确相等；AFBC 输出池每个 slot 必须是 `MEDIA_VIDEO_QueryLayout` 返回的精确 capacity。
  LINEAR 兼容手动帧 `len=0`（按 capacity 做边界校验），该兼容不适用于 AFBC。

## 属性更新

- `MEDIA_RGA_SetGrpAttr` / `SetInAttr` / `SetOutAttr` 只能在模块 Stop 后调用（否则
  `MEDIA_ERR_PIPELINE_STATE`）；已 Bind 的端口不能改变 format/宽高/compress tuple。
- 输出端口属性还要求模块未运行；输入端口 queue_depth 变更要求未 Bind。
- 端口 stride 为 0 时按格式 × 宽度推导；LINEAR stride 必须按格式 bpp 对齐。

## 在发布包中使用

```c
#include "media_rga.h"
```

- 头文件：`include/media_rga.h`
- 模块映像：`plugins/libmedia_rga.so`（与 `libmedia_rga.so.sig` 必须同目录）
- 链接：`-lmedia_rga -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_rga.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("RGA", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

