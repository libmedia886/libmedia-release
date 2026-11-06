# vo

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `VO` | SINK |

RK3588 DRM/KMS atomic 显示输出：把 Pipeline 里的 DMA-BUF 帧（LINEAR 或 AFBC）**零拷贝直接 scanout**
多个 plane（channel）共用一个 output（connector + CRTC），支持热插拔、运行时改 mode、HDMI SDR 10bpc、
外部 master（X11 等）管分辨率的 EXTERNAL 显示所有权。

本模块由 libmedia core 的内置模块 VO 原样迁出，只依赖公开 SDK（`media_api.h` / `media_plugin.h`）与 libdrm。
应用只需 `#include "media_vo.h"` 并链接 `libmedia_vo.so`，`MEDIA_VO_*` 函数名、参数、结构体和
`MEDIA_SYS_Bind("VO", ...)` 的用法与迁出前完全一致。

## API 概览（`media_vo.h`）

输出 / 显示：

```c
int MEDIA_VO_SetAttr(int output_id, const MEDIA_VO_ATTR *attr);   /* 绑定 connector，创建 VO_<id>；已存在时改 mode */
int MEDIA_VO_GetOutput(...);  int MEDIA_VO_GetOutputInfo(...);  int MEDIA_VO_ProbeConnectors(...)
int MEDIA_VO_GetActualMode(...);  int MEDIA_VO_GetSinkCaps(...);  int MEDIA_VO_RegisterEventCb(...)
int MEDIA_VO_SetDisplayOwner(...); int MEDIA_VO_GetDisplayOwner(...); int MEDIA_VO_NotifyExternalMode(...)
int MEDIA_VO_SetColorAttr(...);  int MEDIA_VO_GetColorInfo(...);       /* HDMI SDR 8/10bpc、RGB/YCbCr */
```

通道（plane）：

```c
int MEDIA_VO_QueryChnCaps(output_id, plane_type, force_plane, caps, max_caps, &count);  /* 当前拓扑下可直接 scanout 的 format×compress */
int MEDIA_VO_CreateChn(output_id, chn, x, y, width, height, stride, depth, plane_type, format, compress, force_plane)
int MEDIA_VO_DestroyChn(output_id, chn)
int MEDIA_VO_Start(output_id, chn);  int MEDIA_VO_Stop(output_id, chn)
int MEDIA_VO_FreezePlane(...);  int MEDIA_VO_HidePlane(...)
int MEDIA_VO_GetFrame(output_id, chn, &buf, timeout_ms);  int MEDIA_VO_ReleaseFrame(output_id, chn, buf);   /* 少用 */
```

## 使用流程

```c
#include "media_api.h"
#include "media_vo.h"

MEDIA_SYS_Init
MEDIA_VO_ATTR attr = {
    .intf = MEDIA_VO_INTF_HDMI
    .connector = "HDMI-A-1",          /* 必填：DRM 标准名，可用 MEDIA_VO_ProbeConnectors 枚举 */
    .width = 1920, .height = 1080, .fps = 60
    .plane_count = 1
    .policy = MEDIA_VO_MODE_NEGOTIATE, /* sink 支持才出图；FORCE 则强制 modeset，未插线也允许 */
}
MEDIA_VO_SetAttr(0, &attr)
MEDIA_VO_CreateChn(0, 0, /*x*/0, /*y*/0, 1920, 1080, /*stride*/1920, /*depth*/4
                   MEDIA_VO_PLANE_TYPE_AUTO, MEDIA_FORMAT_NV12, MEDIA_VIDEO_COMPRESS_NONE, /*force_plane*/0)
MEDIA_SYS_Bind("VPSS", 0, "output0", "VO", 0, "input0")
MEDIA_VO_Start(0, 0)
/* ... */
MEDIA_VO_Stop(0, 0)
MEDIA_SYS_UnBind("VPSS", 0, "output0", "VO", 0, "input0")
MEDIA_VO_DestroyChn(0, 0)
MEDIA_SYS_Exit;                    /* 销毁 VO_0，释放 plane / FB / mode blob，并关闭 DRM fd */
```

也可以不链接、只让框架动态加载：`MEDIA_PLUGIN_PATH=<dir>` 或 `MEDIA_PLUGIN_Load("libmedia_vo.so")` 后
`MEDIA_PLUGIN_CreateModule("VO", ...)` 的 config 是内部结构，**应用请始终通过 `MEDIA_VO_SetAttr` 创建输出**。

### 规则

- `MEDIA_VO_CreateChn` 必须在 `MEDIA_VO_SetAttr` 之后；`chn` 必须小于 `plane_count`。
- `inputN` 端口在创建 VO 实例时就按 `plane_count` 全部声明，初始是 NV12 + 输出分辨率的占位属性
  `CreateChn` 把端口改写成该通道的精确 `format × compress × 尺寸 × 队列深度`（`depth<=0` 取 4），之后 `MEDIA_SYS_Bind`
  才按这个 tuple 检查上游。`CreateChn` 之前 Bind 只有恰好是占位属性才可能成功，**务必先 CreateChn 再 Bind**。
- 已 Bind 的通道不能再 `CreateChn`（返回 `MEDIA_ERR_PIPELINE_STATE`）；已 `CreateChn` 的通道再 Create 返回 `MEDIA_ERR_ALREADY_EXISTS`
  需要先 `DestroyChn`。运行中（别的通道正在显示）可以动态创建新通道。
- `MEDIA_VO_Start` 启用通道并在实例未运行时启动收帧线程；最后一个通道 `Stop` 后实例停止。
- 逻辑格式（`MEDIA_FORMAT_*`）：NV12、YUV420SP、YUV422_YUYV、NV24、YUV420SP/422SP/444SP_10BIT（NV15/NV20/NV30）、
  RGB888、RGBA8888、BGR888、BGR565、BGRA8888、ARGB8888、ABGR8888、RGB101010、BGR101010。
  是否真能创建取决于当前 CRTC / plane 对 FourCC + modifier 的支持，先用 `MEDIA_VO_QueryChnCaps` 查询。
- LINEAR 通道必须显式给 stride（plane0 byte pitch，不小于有效行字节且 64 字节对齐）；AFBC 通道 stride 必须为 0
  逐帧以 buffer 的真实 `VIDEO_FRAME` metadata 为准。
- 尺寸范围 16x16 .. 3840x2160（输出与通道）。

### Pool / Buffer 要求

- 上游帧必须是 DMA-BUF（`MEDIA_POOL_ALLOC_MODE_NATIVE` / `COMPAT` 的 Pool）；VO 用 `MEDIA_POOL_GetFd` 取 fd 直接 AddFB2
  不 map、不拷贝、不做 CPU 访问。HOST 内存 Pool 的帧会因没有 DMA-BUF fd 而被拒绝。
- 每个 plane 同时最多占用 3 个 slot：当前 scanout（`current`）、等待提交（`queued`）、刚被替换待归还（`retired`）。
  上游 Pool 的 count 应不少于 `队列深度 + 3`，多路 Bind 时按通道相加。
- 帧所有权：收线程取到的帧由 VO 自己持有（`MEDIA_BUFFER_Ref` 后立即 `MEDIA_PLUGIN_ReturnFrame`，不占框架 `MAX_HELD` 名额）
  Stop / Destroy / Exit 时全部归还 Pool，归还失败会保留重试，不会提前复用 slot。
- VO 是 latest-frame sink：同一通道积压多帧时只显示最新一帧，较旧的帧直接归还。

## 事件、热插拔、显示所有权

行为与迁出前一致，详 与
docs/VO实现约束.md：

- `NEGOTIATE`：sink 支持配置的 mode 才输出，不自动改分辨率；没有 sink 时输出路径不可用，所有 plane 在第一次 commit 时进入停止状态
  应用的 Start 意图保留，sink 接入后自动恢复。
- `FORCE`：强制 modeset，未插线也允许输出路径。
- `EXTERNAL` owner：VO 不 modeset，由外部（X11）管分辨率；`MEDIA_VO_NotifyExternalMode` 通知跟随，plane commit 仍可上屏。
- HPD 线程监听 netlink uevent（500ms 兜底轮询），最后一个输出销毁时 join 并关闭 DRM fd。

## 限制

- DRM 设备固定 `/dev/dri/card0`。
- 单进程内同一 `output_id` 只能有一个实例；`output_id` 最多 4 个、每个最多 8 个 plane。
- 当前 RK3588 VOP2 对 `YUV420_8BIT` AFBC、LINEAR XR30/XB30、`BGR888/ABGR8888 + AFBC 0x071` 等 tuple 的限制
- `MEDIA_VO_GetFrame` 只在通道已 `Start` 时有效，用于旁路取最新一帧，不是主显示路径。

## 在发布包中使用

```c
#include "media_vo.h"
```

- 头文件：`include/media_vo.h`
- 模块映像：`plugins/libmedia_vo.so`（与 `libmedia_vo.so.sig` 必须同目录）
- 链接：`-lmedia_vo -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vo.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VO", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

