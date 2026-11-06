# vdec

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `VDEC` | SOURCE / FILTER |

VDEC 是 libmedia 的硬件视频解码外部模块，用 RK3588 的 MPP（rockchip_mpp）把
H.264 / H.265 / MJPEG 码流解码为 NV12（或 H.265 Main10 的 compact NV15 / 8bit
AFBC 16x16），输出到应用提供的外部 Pool。它原本是 libmedia core 的内置模块
现已迁出为独立仓库/独立 `.so`，type 名仍为 `VDEC`，`MEDIA_VDEC_*` 接口与旧内置
版本源码兼容。

- 公共头文件：`include/media_vdec.h`
- 产物：`libmedia_vdec.so`（只导出 `media_plugin_entry` 与 `MEDIA_VDEC_*`）
- 依赖：`libmedia.so`（API 0.7.0 及以上）、`librockchip_mpp.so.1`、板端 `/dev/mpp_service`

## 使用方式

### 关键点：外部输出 Pool

VDEC 自己不解码到内部 buffer，而是解码到应用提供的 Pool 槽位。因此必须：

1. `MEDIA_VDEC_QueryOutputPoolRequirements` 查询槽大小与槽数
2. 用该尺寸创建 Pool
3. 把 `pool_id` 填进 `MEDIA_VDEC_ATTR.pool_id` 再 `MEDIA_VDEC_CreateChn`。

```c
MEDIA_VDEC_ATTR attr = {0}
MEDIA_VDEC_OUTPUT_POOL_REQUIREMENTS req
attr.width = 320
attr.height = 240
attr.stride = 320
attr.buf_cnt = 8
attr.video_type = MEDIA_VIDEO_H264
attr.pool_id = pool_id;            /* 应用创建的输出 Pool */
attr.has_input_port = 0;           /* 手动 SendPacket 模式 */
attr.input_depth = 0
attr.output_format = 0;            /* 0 = NV12 */
attr.output_compress = MEDIA_VIDEO_COMPRESS_NONE
MEDIA_VDEC_QueryOutputPoolRequirements(&attr, &req)
/* 用 req.slot_size / req.slot_count 创建 Pool，id 填回 attr.pool_id */

MEDIA_VDEC_CreateChn(0, &attr)
MEDIA_VDEC_Start(0)
MEDIA_VDEC_SendPacket(0, data, size, pts)
MEDIA_VDEC_GetFrame(0, &frame, 1000)
/* frame.buffer 的 meta 里有真实 layout 与像素格式 */
MEDIA_VDEC_ReleaseFrame(0, frame)
MEDIA_VDEC_Stop(0)
MEDIA_VDEC_DestroyChn(0)
```

`MEDIA_VDEC_FRAME` 携带 `lease_id`，`ReleaseFrame` 必须原样传回，同一 token
只能成功释放一次。

### 两种输入模式

| 模式 | `has_input_port` | 用法 |
| --- | --- | --- |
| 手动送包 | `0` | 应用调用 `MEDIA_VDEC_SendPacket(chn, data, size, pts)` |
| Bind 接收 | `1` | 声明 `input0`，`MEDIA_SYS_Bind("VENC", 0, "output0", "VDEC", 0, "input0")` |

两种模式共用 `MEDIA_VDEC_SendPacket` 的提交路径；Bind 模式下插件自管输入线程
用 `MEDIA_PLUGIN_RecvFrame` 从 `input0` 取包解引用后送入 MPP。由于同一 type 既
可有输入端口也可没有，插件类的 `kind` 使用 0.7.0 新增的
`MEDIA_PLUGIN_KIND_AUTO`，由 create 实际声明的端口推导模块类型（无输入 → SOURCE
语义、有输入 → FILTER 语义），否则手动模式的实例无法通过 `pipeline_validate_connections`。

### 作为插件创建

```c
MEDIA_VDEC_ATTR attr = { /* 同上，含 pool_id 与 has_input_port */ }
MEDIA_PLUGIN_CreateModule("VDEC", 0, &attr, sizeof(attr))
MEDIA_PLUGIN_Start("VDEC", 0)
```

`MEDIA_VDEC_ATTR` 本身就包含 `pool_id`，插件路径不需要额外 SetParam。

- 实例名：`VDEC_<chn>`
- `GetParam("mpp")` 返回 `{ void *mpp_ctx; void *mpp_mpi; }`。

### 输出格式与容量契约

- `output_format = 0` → NV12
- H.265 Main10 可用 compact NV15（需显式给 MPP 要求的 stride）
- 8bit 输出可选 AFBC 16x16（`MEDIA_AFBC_BLOCK_16X16`，0x001）
- NV20 / NV30 / P010 不支持
- Pool 槽容量按下述规则计算（`vdec_required_output_capacity_values`）：
  AFBC 为 `align64(w) * align16(align8(h)+16) * 25/16`、H.264 为 2x、
  H.265 为 9/5、MJPEG 为 2x、NV15 按 CTU 计算。用
  `MEDIA_VDEC_QueryOutputPoolRequirements` 拿到的值即可，不要自行估算。

### 尺寸与 stride 校验

- 宽高 16..8192，宽必须为偶数
- `stride` 在 LINEAR 输出时必填且不得小于 MPP 最小 stride、必须为偶数
- 压缩输出时端口声明的 stride 为 0（pitch 由 Pool 决定）。

## API 一览

| 函数 | 说明 |
| --- | --- |
| `MEDIA_VDEC_QueryOutputPoolRequirements` | 查询输出 Pool 的槽大小与槽数 |
| `MEDIA_VDEC_CreateChn` | 创建解码通道 |
| `MEDIA_VDEC_SetAttr` | 保留旧行为：打印 TODO 并返回 -1 |
| `MEDIA_VDEC_DestroyChn` | 销毁通道 |
| `MEDIA_VDEC_Enable` / `Disable` | 启停解码 |
| `MEDIA_VDEC_Start` / `Stop` | 启动/停止解码线程（Bind 模式另含输入线程） |
| `MEDIA_VDEC_SendPacket` | 提交一个码流包 |
| `MEDIA_VDEC_GetFrame` / `ReleaseFrame` | 取/还解码帧（带 lease_id） |

## 在发布包中使用

```c
#include "media_vdec.h"
```

- 头文件：`include/media_vdec.h`
- 模块映像：`plugins/libmedia_vdec.so`（与 `libmedia_vdec.so.sig` 必须同目录）
- 链接：`-lmedia_vdec -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vdec.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VDEC", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

