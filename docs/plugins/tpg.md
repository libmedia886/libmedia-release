# tpg

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `TPG` | SOURCE |

测试图案源，产出 `libmedia_tpg.so` 与 [`include/media_tpg.h`](../../include/media_tpg.h)。只依赖 libmedia 公开 SDK
也是写外部模块的参考实现（SOURCE，托管模式，按截止时间节拍）。

- 格式：NV12、NV15/NV20/NV30（compact 10bit）、RGB888/BGR888、RGBA/BGRA/ARGB/ABGR8888、GRAY8，LINEAR。
- 图案：色条、棋盘格、渐变、纯色、移动方块。
- 用法：`MEDIA_TPG_CreateChn` → `MEDIA_SYS_Bind("TPG", chn, "output0", ...)` → `MEDIA_TPG_Start`
  或 `MEDIA_PLUGIN_Load("libmedia_tpg.so")` + `MEDIA_PLUGIN_CreateModule("TPG", chn, &attr, sizeof(attr))`。
- 1080p 下 CPU 逐像素填充约 32ms/帧（约 30fps），1080p60 压测请降低分辨率或另做预生成/GPU 填充。

## 在发布包中使用

```c
#include "media_tpg.h"
```

- 头文件：`include/media_tpg.h`
- 模块映像：`plugins/libmedia_tpg.so`（与 `libmedia_tpg.so.sig` 必须同目录）
- 链接：`-lmedia_tpg -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_tpg.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("TPG", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

