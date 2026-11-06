# vpss

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `VPSS` | FILTER |

GLES 视频处理：裁剪、缩放、0/90/180/270 旋转、翻转、keep-aspect、色彩空间（BT.601/709 × Limited/Full、
packed RGB Full/Limited）、SDR8/SDR10、LINEAR 与 AFBC 之间转换，最多 16 路输出。产出 `libmedia_vpss.so`
与 [`include/media_vpss.h`](../../include/media_vpss.h)；`MEDIA_VPSS_*` API 与原内置 VPSS 源码兼容，实例名 `VPSS_<grp>`。

## 设计

- FILTER 插件、**自管模式**：插件工作线程 `RecvFrame` 取帧，一次 GPU 提交绘制全部输出，只等一次 fence
  再 `EmitFrame` 各路输出、`ReturnFrame` 输入。GPU 上下文（`MEDIA_GPU_CreateContext`，共享进程级 GBM
  EGLDisplay）在该线程创建、使用和销毁。
- **隔离（quarantine）**：fence 无法确认完成时，本帧输入与输出都不归还，停止处理新帧；Stop 时在工作线程
  内重试等待 / `glFinish` 后再归还，确认 GPU 不再访问前绝不把 buffer 交回 Pool。
- **零拷贝 + 缓存**：DMA-BUF → EGLImage → texture/FBO，按 `(pool, index, Pool 代次, fd, size, layout
  color space)` 缓存；Start 时预导入每个输出 Pool 的全部 slot，稳态零导入、零 CPU copy/map/readback。
- **能力**：GLES 方向能力表（format × compress）来自 core 格式事实源 `MEDIA_GPU_FormatSupported /
  CompressSupported`；VPSS 自身约束（16..8192、LINEAR pitch 64 字节对齐、色度对齐、高位深有向 pair）由插件自身维护。
- 统计：`MEDIA_PLUGIN_RecordProcess` 记录每帧处理耗时，`MEDIA_PLUGIN_ReportStats` 上报 GPU 耗时与
  cache/import/draw/wait/strong 计数（`MEDIA_SYS_debug` 可见）。
- `MEDIA_VPSS_SetOutAttr / SetOutCrop` 只允许在 Disable 后调用；已 Bind 的输出不能改 format/尺寸/compress
  （`MEDIA_PLUGIN_SetPortAttr` 检查）。

## 在发布包中使用

```c
#include "media_vpss.h"
```

- 头文件：`include/media_vpss.h`
- 模块映像：`plugins/libmedia_vpss.so`（与 `libmedia_vpss.so.sig` 必须同目录）
- 链接：`-lmedia_vpss -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vpss.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VPSS", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

