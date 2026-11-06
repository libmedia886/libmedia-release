# tnr

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `TNR_CL` | FILTER |

本插件把旧 `TNR_CL` 的 NV12 时域降噪功能移植为独立 `libmedia_tnr.so`。插件 type 名保持为 `TNR_CL`，实例名为 `TNR_CL_<grp>`，输入端口为 `input0`，输出端口为 `output0`。

输入和输出均为相同宽高的 `MEDIA_FORMAT_NV12`。输入的真实布局从逐帧 metadata 读取；输出使用 `output_pool_id` 指定的 Pool，设置为负数时复用输入 Pool。Pool 必须能容纳 `MEDIA_VIDEO_QueryLayout(width,height,NV12,NONE)` 的大小，且数量覆盖在途帧和历史帧。压缩 NV12、其它格式、缩放和多路端口均不支持。

配置字段与旧 API 一致：`width`、`height`、`format`、`input_depth`、`output_pool_id`、`input_stride`、`output_stride`、`block_size`、`threshold`、`static_alpha`、`motion_alpha`、`passthrough`。阈值大于 1 时按 8 位码值除以 255；历史帧通过公开 Buffer retain 保留，只读使用。

```c
MEDIA_TNR_CL_ATTR attr = {
    .width = 1920, .height = 1080, .format = MEDIA_FORMAT_NV12
    .input_depth = 4, .output_pool_id = 2
    .static_alpha = 0.35f, .motion_alpha = 0.88f
}
MEDIA_TNR_CL_CreateGrp(0, &attr)
MEDIA_SYS_Bind("VI", 0, "output0", "TNR_CL", 0, "input0")
MEDIA_TNR_CL_Start(0)
```

## 在发布包中使用

```c
#include "media_tnr.h"
```

- 头文件：`include/media_tnr.h`
- 模块映像：`plugins/libmedia_tnr.so`（与 `libmedia_tnr.so.sig` 必须同目录）
- 链接：`-lmedia_tnr -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_tnr.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("TNR_CL", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

