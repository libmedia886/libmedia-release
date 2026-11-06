# vi

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `VI` | SOURCE |

V4L2 / HDMI-RX 视频采集，产出 `libmedia_vi.so` 与 [`include/media_vi.h`](../../include/media_vi.h)。只依赖 libmedia 公开 SDK。
`MEDIA_VI_*` API 与原内置 VI 源码兼容（新增 `MEDIA_VI_Destroy`），实例名 `VI_<dev>`，输出端口 `output0`。

## 设计

- SOURCE 插件、托管模式：框架线程循环调用 `process`，`process` 用 `select` 等待一帧并 `DQBUF`。
- 零拷贝：`Enable` 时从 `pool_id` 取 `buf_cnt` 个 Buffer 以 `V4L2_MEMORY_DMABUF` 入队；每采到一帧先从 Pool
  补一个新 Buffer 入同一驱动槽位，再把采到的 Buffer 交给框架发送（一出多入时由框架加引用分发）。
  Pool 暂时耗尽时丢弃本帧并把原 Buffer 重新入队。Pool count 至少为 `buf_cnt` + 下游在途数量。
- **Pool 必须是 CMA/连续内存**：HDMI-RX 驱动导入 DMABUF 时只接受 CMA 的 dma-buf。默认的
  `MEDIA_POOL_Create` 使用 `MEDIA_POOL_ALLOC_MODE_COMPAT`（system-dma32），`QBUF` 会被驱动以
  `EFAULT` 拒绝，`Enable` 失败。创建 Pool 必须显式指定 NATIVE：
  ```c
  MEDIA_POOL_CreateWithMode(pool_id, size, count, MEDIA_POOL_ALLOC_MODE_NATIVE)
  ```
- 输出格式的 canonical LINEAR 布局用 `MEDIA_VIDEO_QueryLayout(w, h, fmt, 0, ...)` 查询即可，包括 HDMI-RX
  常见的 `NV24`。注意 `NV24` 目前只有 CPU / DRM 扫描路径：Mali-G610 的 `eglCreateImage`
  不接受 `DRM_FORMAT_NV24`（`EGL_BAD_MATCH`），因此 GPU 插件不能直接导入 NV24 帧。
- 逐帧 metadata：真实 layout（驱动返回的 bytesperline）、`len = bytesused`、单调时钟时间戳、递增 `seq`
  发布前用 `MEDIA_VIDEO_ValidateLayout` 校验，坏帧重新入队不外发。
- `MEDIA_VI_QuerySource` / `MEDIA_VI_WaitSourceChange` 不依赖实例，供上层做 HDMI 插拔与分辨率变化管理。

### HDMI-RX 8-bit LINEAR 采集格式

`SW-SRS-ACQ-003` 在不扩展 10bit 和压缩布局的前提下，将 HDMI-RX
8-bit NV16 纳入 VI output0 能力。

1. VI 当前的 LINEAR 白名单是 `NV12`、`NV24`、`YUV422SP/NV16`、
   `RGB888` 和 `BGR888`；`output_compress` 仍只允许
   `MEDIA_VIDEO_COMPRESS_NONE`，不得由此外推任何 10bit 或 AFBC 输入。
2. `MEDIA_FORMAT_YUV422SP` 在 V4L2 边界映射为 `V4L2_PIX_FMT_NV16`
   在逐帧布局中发布 `DRM_FORMAT_NV16 + DRM_FORMAT_MOD_LINEAR`。
   Y 和 UV 的 byte pitch 都使用驱动 `VIDIOC_S_FMT` 返回的 plane0
   `bytesperline`，UV offset 为 `pitch * visible_height`，单个 buffer 容量
   至少为 `pitch * visible_height * 2`。
3. V4L2 枚举 NV16 不代表当前 HDMI 时序/色彩格式一定接受
   `VIDIOC_S_FMT(NV16)`。当 HDMI-RX 当前收到 YUV420 时，驱动可以只允许
   NV12；真实 NV16 验证必须先确认上游已输出 8-bit YUV422，不得把
   当前信号约束误判为 VI 格式白名单拒绝。
4. `VIDIOC_S_FMT` 后必须核对驱动返回的实际 width/height/FourCC、单个
   V4L2 memory plane、plane0 `bytesperline` 和 `sizeimage`。每个外部 pool
   slot 在 `QBUF` 前必须覆盖 `sizeimage`；每次 `DQBUF` 后以 plane0
   `bytesused` 校验当前 LINEAR layout，并一次性发布非零 `metadata len`、
   真实 layout、timestamp 和 seq。驱动改写 tuple、buffer 太小、`bytesused`
   越界或 layout 不完整时不得把帧交给下游。

## 在发布包中使用

```c
#include "media_vi.h"
```

- 头文件：`include/media_vi.h`
- 模块映像：`plugins/libmedia_vi.so`（与 `libmedia_vi.so.sig` 必须同目录）
- 链接：`-lmedia_vi -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vi.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VI", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

