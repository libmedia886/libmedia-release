# csc_rga

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `CSC_RGA` | FILTER |

`libmedia_csc_rga.so`：用 RGA 硬件把输入帧转换到另一种颜色空间 / 像素格式，并输出到固定
Buffer Pool。模块由 libmedia core 的内置 CSC_RGA 迁出，只依赖公开 SDK
（`media_api.h` / `media_plugin.h`）与 librga。

## 使用

```c
MEDIA_SYS_Init
MEDIA_CSC_RGA_ATTR attr = {0}
attr.input_width = 1920
attr.input_height = 1080
attr.input_format = MEDIA_FORMAT_NV12
attr.output_format = MEDIA_FORMAT_RGB888
attr.output_pool_id = out_pool;      /* 输出固定 Pool，必须已创建 */
attr.csc_mode = CSC_RGA_CSC_BT601_LIMIT
MEDIA_CSC_RGA_CreateGrp(0, &attr)
MEDIA_CSC_RGA_Start(0)
/* 手动送帧 */
MEDIA_SYS_SendFrame("CSC_RGA", 0, "input0", in_buf, 1000)
MEDIA_BUFFER out
MEDIA_CSC_RGA_GetFrame(0, &out, 1000)
MEDIA_CSC_RGA_ReleaseFrame(0, &out)
MEDIA_CSC_RGA_Stop(0)
MEDIA_CSC_RGA_DestroyGrp(0)
```

或与上游模块 Bind（实例名仍是 `CSC_RGA_<grp>`，端口仍是 `input0` / `output0`）：

```c
MEDIA_SYS_Bind("VPSS", 0, "output0", "CSC_RGA", 0, "input0")
```

输出尺寸恒等于输入尺寸；`output_pool_id` 是唯一输出目标，不会回退到输入 Pool。
`input_depth` 为 0 时输入端口队列深度取默认 4。

### 颜色空间模式

- `CSC_RGA_CSC_DEFAULT`（0）
- `CSC_RGA_CSC_BT601_LIMIT`（1）
- `CSC_RGA_CSC_BT601_FULL`（2）
- `CSC_RGA_CSC_BT709_LIMIT`（3）
- `CSC_RGA_CSC_BT709_FULL`（4）

`BT709_FULL` 在 librga 1.10.1 上没有对应选择器：LINEAR 路径回落到 DEFAULT 继续转换
压缩（AFBC）路径在 CreateGrp 时返回错误。

### 修改 csc_mode（内部参数）

旧内置模块提供 `csc_rga_set_csc_mode`；插件化后改用框架的 SetParam 接口，不新增公开
API。只允许在 `CreateGrp` 之后、`Start` 之前调用：

```c
int mode = CSC_RGA_CSC_BT709_LIMIT
MEDIA_PLUGIN_SetParam("CSC_RGA", 0, "csc_mode", &mode, sizeof(mode))
```

运行中调用返回 `MEDIA_ERR_PIPELINE_STATE`，需要先 `Stop` 再设置。

## 支持格式与存储契约

输入/输出格式：`NV12`、`RGB888`、`BGR888`、`RGBA8888`、`BGRA8888`、`ARGB8888`、
`ABGR8888`。线性存储时 stride 为 0 会按格式和输入宽度推导（输出 stride 沿用旧代码用
`input_width` 计算的行为）。

AFBC 只允许 NV12 + `MEDIA_AFBC_BLOCK_16X16`（0x001）或 `MEDIA_AFBC_BLOCK_16X16 |
MEDIA_AFBC_SPARSE`（0x041），且配置 stride 必须为 0；方向矩阵只允许以下两种：

- 输入 AFBC NV12 → 输出线性 RGB（输出不能是 RGBA8888/BGRA8888）
- 输入线性 RGB → 输出 AFBC NV12。

其它组合在 `CreateGrp` 时被拒绝。

## 在发布包中使用

```c
#include "media_csc_rga.h"
```

- 头文件：`include/media_csc_rga.h`
- 模块映像：`plugins/libmedia_csc_rga.so`（与 `libmedia_csc_rga.so.sig` 必须同目录）
- 链接：`-lmedia_csc_rga -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_csc_rga.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("CSC_RGA", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

