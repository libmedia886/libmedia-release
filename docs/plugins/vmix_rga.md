# vmix_rga

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `VMIX_RGA` | FILTER |

`VMIX_RGA`（type `VMIX_RGA`）模块：用 RGA 硬件把最多 16 路输入按通道位置/尺寸合成到固定输出 Pool。
由 libmedia core 原有实现独立而成，现在是独立 git 仓库，只依赖公开 SDK 与 librga。

## 使用

实例名为 `VMIX_RGA_<grp>`，端口 `input0..input15` / `output0`
`MEDIA_SYS_Bind("VMIX_RGA", grp, "inputN", ...)` 与迁移前一致。

```c
#include "media_vmix_rga.h"

MEDIA_VMIX_RGA_ATTR attr = {
    .input_count = 2
    .output_width = 1920
    .output_height = 1080
    .format = MEDIA_FORMAT_NV12
    .output_pool_id = out_pool
    .primary_index = 0
    .use_rga_compose = 1
}
attr.channels[0] = (MEDIA_VMIX_RGA_CHANNEL){ .enabled = 1, .width = 1920, .height = 1080 }
attr.channels[1] = (MEDIA_VMIX_RGA_CHANNEL){ .enabled = 1, .x = 1280, .y = 720
                                            .width = 640, .height = 360 }
MEDIA_VMIX_RGA_CreateGrp(0, &attr)
MEDIA_VMIX_RGA_Start(0)
```

也支持作为插件 `MEDIA_PLUGIN_Load("libmedia_vmix_rga.so")` 后
`MEDIA_PLUGIN_CreateModule("VMIX_RGA", grp, &attr, sizeof(attr))`。

## 约束与行为（与迁移前一致）

- 输出格式只支持 LINEAR（`output_compress` 必须为 0）；通道输入按
  `MEDIA_VMIX_RGA_CHANNEL.compress` 的精确契约，NV12 支持 `NONE` / `0x001` / `0x041`。
- 支持格式：NV12（含上面三种 compress）、RGB888/BGR888、RGBA8888/BGRA8888/ARGB8888/ABGR8888。
- `primary_index` 决定刷新节奏；其它输入复用最后一帧（last frame reuse）。
- `use_rga_compose` 非 0 时优先 `imcomposite`，失败自动回退为逐个 blit。
- 主输入完全覆盖输出时跳过背景清除；`MEDIA_VMIX_RGA_FORCE_CLEAR=1` 环境变量可强制清除
  （与原实现相同的调试开关）。
- `set_param("input_attr")` 仍返回 `MEDIA_ERR_UNSUPPORTED`：运行期不支持修改通道属性。
- 统计 detail 仍为 `rga_vmix_host inputs=<N>`。

## 在发布包中使用

```c
#include "media_vmix_rga.h"
```

- 头文件：`include/media_vmix_rga.h`
- 模块映像：`plugins/libmedia_vmix_rga.so`（与 `libmedia_vmix_rga.so.sig` 必须同目录）
- 链接：`-lmedia_vmix_rga -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vmix_rga.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VMIX_RGA", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

