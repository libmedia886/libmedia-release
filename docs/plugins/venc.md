# venc

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `VENC` | FILTER |

VENC 是 libmedia 的硬件视频编码外部模块，用 RK3588 的 MPP（rockchip_mpp）对
NV12/YUV420 等输入做 H.264 / H.265 编码（格式表还保留 MJPEG / JPEG / VP8，能否
出码流取决于板端 MPP 实际能力）。它原本是 libmedia core 的内置模块，现已迁出为
独立仓库/独立 `.so`，type 名仍为 `VENC`，`MEDIA_VENC_*` 接口与旧内置版本源码兼容。

- 公共头文件：`include/media_venc.h`
- 产物：`libmedia_venc.so`（只导出 `media_plugin_entry` 与 `MEDIA_VENC_*`）
- 依赖：`libmedia.so`（API 0.7.0 及以上）、`librockchip_mpp.so.1`、板端 `/dev/mpp_service`

## 使用方式

### 1. 直接调用公共 API（与旧内置版本一致）

```c
#include "media_api.h"
#include "media_venc.h"

MEDIA_SYS_Init
MEDIA_VENC_CreateChn(0, 320, 240, 320, 30, 4, 4
                     out_pool_id, 4000000, 30, MEDIA_FORMAT_H264, 0)
MEDIA_VENC_Start(0)
MEDIA_PACKET pkt
if (MEDIA_VENC_GetPacket(0, &pkt, 1000) == 0) {
    /* pkt.data 为码流起始地址，pkt.len 为本包字节数 */
    MEDIA_VENC_ReleasePacket(0, &pkt)
}

MEDIA_VENC_Stop(0)
MEDIA_VENC_DestroyChn(0)
MEDIA_SYS_Exit
```

首次调用任一 `MEDIA_VENC_*` 时，本库会自行调用 `MEDIA_PLUGIN_LoadSelf` 向
libmedia 注册 `VENC` 类，不需要应用手动 `MEDIA_PLUGIN_Load`。

`MEDIA_SYS_Exit` 会注销全部插件，下次调用时会自动重新注册。

### 2. 作为插件创建

```c
MEDIA_VENC_ATTR attr = { /* 见下表 */ }
MEDIA_PLUGIN_CreateModule("VENC", 0, &attr, sizeof(attr))
int pool_id = out_pool_id
MEDIA_PLUGIN_SetParam("VENC", 0, "output_pool_id", &pool_id, sizeof(pool_id))
MEDIA_PLUGIN_Start("VENC", 0)
```

`MEDIA_VENC_ATTR` 没有 `output_pool_id` 字段（与旧声明保持字段顺序不变）
插件路径必须在 create 之后、start 之前用 SetParam 设置；直接 API
`MEDIA_VENC_CreateChn` 的参数里已经包含它。

### 3. 端口与实例名

- 实例名：`VENC_<chn>`
- 输入端口 `input0`：视频帧，格式为 `MEDIA_VENC_ATTR.input_format`
  （默认 NV12），可 Bind 上游，或 `MEDIA_SYS_SendFrame("VENC", chn, "input0", ...)`
- 输出端口 `output0`：编码码流（`MEDIA_FORMAT_H264` / `MEDIA_FORMAT_H265` 等）
  可由 `MEDIA_SYS_Bind(..., "VENC", chn, "output0")` 接下游，未连接时用
  `MEDIA_VENC_GetPacket/ReleasePacket` 或 `MEDIA_SYS_GetFrame` 取包
- 模块类型由 create 声明的端口推导（有输入有输出 → FILTER）。

Bind 方向示例：

```c
MEDIA_SYS_Bind("TPG", 0, "output0", "VENC", 0, "input0")
MEDIA_SYS_Bind("VENC", 0, "output0", "MY_SINK", 0, "input0")
```

### 4. 输出 buffer

编码输出使用外部 Pool：创建通道时传入 `output_pool_id`。Pool 必须由应用创建
槽大小要能容纳一帧码流（测试用 320x240 时取 64 KiB 足够）；模块通过
`MEDIA_POOL_GetBuffer` 申请，`memcpy` 码流后写 meta（`type = VIDEO_FRAME`、
`media_format` 为编码格式、`len` 与 `pitch_bytes[0]` 均为本包字节数、`visible_width`
为字节数、`time_stamp` 取 MPP 包 PTS），再交给下游或 GetPacket。

## API 一览

| 函数 | 说明 |
| --- | --- |
| `MEDIA_VENC_CreateChn` | 创建通道（尺寸、stride、帧率、buf 数、输入深度、输出 Pool、码率、GOP、格式、输入压缩） |
| `MEDIA_VENC_SetAttr` | 整份属性更新（校验 + 在线应用 + 失败回滚）。只能修改 fps/bitrate/gop/rc_mode/输入格式×压缩/stride/编码格式；width/height/buf_cnt/input_depth 只能在 CreateChn 时确定，改动会返回 `MEDIA_ERR_UNSUPPORTED` |
| `MEDIA_VENC_DestroyChn` | 销毁通道 |
| `MEDIA_VENC_Enable` / `Disable` | 启停编码，等价于 `Start` / `Stop`（会销毁 MPP context 并丢弃在途输入） |
| `MEDIA_VENC_Start` / `Stop` | 启动/停止编码线程 |
| `MEDIA_VENC_GetFrame` / `ReleaseFrame` | 以 `MEDIA_BUFFER` 形式取/还码流包 |
| `MEDIA_VENC_GetPacket` / `ReleasePacket` | 以 `MEDIA_PACKET`（data/len/handle）形式取/还码流包；`ReleasePacket` 失败时 handle 保持有效可重试 |
| `MEDIA_VENC_GetMpp` | 取底层 `MppCtx` / `MppApi*` 句柄 |
| `MEDIA_VENC_SetRcMode` | 设置 RC 模式与码率 |
| `MEDIA_VENC_SetFormat` / `SetVideoType` | 设置输入格式 / 编码格式 |
| `MEDIA_VENC_SetQpConfig` / `SetBpsConfig` | QP 与码率上下限 |
| `MEDIA_VENC_SetH265*` | H.265 profile/level/tier/CU/SAO/Deblock |
| `MEDIA_VENC_SetH264*` | H.264 profile/level/CABAC |

枚举 `MEDIA_VENC_RC_MODE`：`CBR / VBR / FIXQP / AVBR / CBR_SHARE / VBR_SHARE`。

## 支持范围与限制

- 编码格式：H.264、H.265（Main）；MJPEG/JPEG/VP8 在当前 MPP 上未验证
- 输入格式：NV12、YUV420SP、YUV422SP、YUV420P、YCrCb 420SP、YUV422P、
  YCrCb 422SP、YUYV/YVYU/UYVY/VYUY、GRAY8、NV24、YUV444P 等 8bit 布局
- 输入压缩：H.264/H.265 接受 `MEDIA_VIDEO_COMPRESS_NONE`、AFBC 16x16（0x001）、
  AFBC 16x16 + SPARSE（0x041），压缩时 `stride` 必须为 0
- 10bit / compact / RGB 输入不支持，创建或 SetAttr 时直接拒绝
- `stride` 必须 ≥ width 且为偶数；宽高需为正；`stride=0` 表示取 `width`（AFBC 压缩输入例外，必须为 0）
- 4:2:0 输入（NV12/YUV420SP/YCrCb420SP/YUV420P）高度必须 2 对齐，宽度一律 2 对齐
- 输出 Pool 满时丢弃当前编码包（可能包含 IDR）并计数，首次、每 100 包以及恢复出包时各打一条
  stderr；不会阻塞编码线程
- 码率过小（例如 1000 bps）会被 MPP 拒绝：接口返回 `MEDIA_ERR_RESOURCE`
  stderr 打印 MPP 原始返回码（`mpp_ret=-6`），测试中使用 4 Mbps
- QP / BPS 参数为三态：`-1` 表示不修改，`0` 是合法取值（QP 范围 0~51），越界返回
  `MEDIA_ERR_INVALID_PARAM`；布尔参数 `0`/`1` 分别表示关闭/打开
- `Stop` 有 2 秒 join 上限，超时返回 `MEDIA_ERR_RESOURCE` 并保留资源供重试。

## 在发布包中使用

```c
#include "media_venc.h"
```

- 头文件：`include/media_venc.h`
- 模块映像：`plugins/libmedia_venc.so`（与 `libmedia_venc.so.sig` 必须同目录）
- 链接：`-lmedia_venc -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_venc.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VENC", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

