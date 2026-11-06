# vo_wbc

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `VO_WBC` | SOURCE |

DRM writeback 回写抓屏：通过 writeback connector 把某个 CRTC（也就是显示器上正在显示的画面）周期性回写到应用 Pool 里
作为 `SOURCE` 输出 `output0`，可以 `MEDIA_SYS_Bind` 给 VENC / 文件 / 网络等下游，也可以 `MEDIA_VO_WBC_GetFrame` 手动取帧。

本模块是旧 `master` 里内置模块 `VO_WBC` 的恢复版（core 精简时被删除）。type 名、`MEDIA_VO_WBC_*` 函数名与参数、
`MEDIA_VO_WBC_ATTR` 结构体名和字段顺序与旧 `master:include/media_api.h` 完全一致，声明在 `include/media_vo_wbc.h`
只依赖公开 SDK（`media_api.h` / `media_plugin.h`）与 libdrm，**不依赖 VO 模块**（自己独立打开 DRM fd）。
详

## API（`media_vo_wbc.h`）

```c
int MEDIA_VO_WBC_Probe(const char *device, int *connector_id, int *crtc_id);   /* 找 writeback connector 与 active CRTC */
int MEDIA_VO_WBC_CreateChn(int chn, const MEDIA_VO_WBC_ATTR *attr)
int MEDIA_VO_WBC_DestroyChn(int chn)
int MEDIA_VO_WBC_Start(int chn)
int MEDIA_VO_WBC_Stop(int chn)
int MEDIA_VO_WBC_GetFrame(int chn, MEDIA_BUFFER *buf, int timeout_ms)
int MEDIA_VO_WBC_ReleaseFrame(int chn, MEDIA_BUFFER buf)
```

`MEDIA_VO_WBC_ATTR`：

| 字段 | 含义 |
| --- | --- |
| `device` | DRM card，`NULL` = `/dev/dri/card0`（最长 63 字符） |
| `target` | 显示 connector 名，如 `DSI-1` / `HDMI-A-1`；`NULL` = 自动找当前 active 的 CRTC（最长 31 字符） |
| `width` / `height` | 抓取尺寸；NV12/BGR888 宽度必须 64 对齐，ARGB8888 必须 16 对齐，不满足时创建失败并提示建议值 |
| `stride` | bytesperline，<=0 自动（`width * 每像素字节数`，NV12 为 `width`）；显式值必须等于自动值 |
| `fps` | <=0 不限速 |
| `format` | `MEDIA_FORMAT_NV12`（默认，值 <=0 时也取 NV12）/ `BGR888` / `ARGB8888` |
| `pool_id` | 输出 Pool，**应用先创建**；必须是 DMA-BUF Pool（`NATIVE` / `COMPAT`），单个 Buffer 至少一帧大小 |
| `output_depth` | 迁出前用于输出端口队列深度；插件框架里输出端口没有队列，**现在只做兼容保留，不再生效** |

## 使用示例

```c
#include "media_api.h"
#include "media_vo_wbc.h"

int connector = 0, crtc = 0
if (MEDIA_VO_WBC_Probe(NULL, &connector, &crtc) != 0) {
    /* 板上没有 writeback connector，或没有已激活的显示 CRTC */
}

MEDIA_SYS_Init
size_t size; MEDIA_VIDEO_LAYOUT layout
MEDIA_VIDEO_BuildLinearLayout(1920, 1080, MEDIA_FORMAT_NV12, &layout, &size)
MEDIA_POOL_Create(1, size, 4);                          /* 输出 Pool，count 覆盖下游深度 + 手动取帧在途数 */

MEDIA_VO_WBC_ATTR attr = {
    .target = "HDMI-A-1", .width = 1920, .height = 1080
    .fps = 30, .format = MEDIA_FORMAT_NV12, .pool_id = 1
}
MEDIA_VO_WBC_CreateChn(0, &attr)
MEDIA_VO_WBC_Start(0)
MEDIA_BUFFER buf
if (MEDIA_VO_WBC_GetFrame(0, &buf, 1000) == 0) {        /* 或 MEDIA_SYS_Bind("VO_WBC", 0, "output0", "VENC", 0, "input0") */
    MEDIA_BUFFER_META meta
    MEDIA_BUFFER_GetMeta(buf, &meta);                   /* VIDEO_FRAME：真实布局 + 单调时钟微秒时间戳 + 序号 */
    /* 读像素前后用 MEDIA_POOL_BeginCpuAccess / EndCpuAccess(DMA_BUF_SYNC_READ) */
    MEDIA_VO_WBC_ReleaseFrame(0, buf)
}
MEDIA_VO_WBC_Stop(0)
MEDIA_VO_WBC_DestroyChn(0)
MEDIA_SYS_Exit
```

把旧 demo（`demo_vo_wbc_capture_image` / `demo_vo_wbc_dump_raw` / `demo_vo_wbc_record`）里有价值的用法归纳为：

- **抓单张图**：`CreateChn` 后 `Start`，`GetFrame` 取一帧、写文件、`ReleaseFrame`、`Stop/Destroy`。
- **dump 原始帧**：循环 `GetFrame` → 按 `meta.u.video` 的 `pitch_bytes/offset_bytes` 逐 plane 写出（NV12 两个 plane，packed RGB 一个）。
- **录制**：`MEDIA_SYS_Bind("VO_WBC", chn, "output0", "VENC", id, "input0")`，VENC 的输入属性取 `width/height/stride/format` 与本通道一致。
  （RTSP 推流已从项目移除，不再提供。）

## 行为要点

- 取帧节拍：抓取线程每次 `MEDIA_POOL_GetBuffer` → AddFB2（按 slot 缓存 FB）→ atomic commit（`WRITEBACK_FB_ID` + out-fence）→ 等 fence（1s 超时）
  → 写 metadata → `MEDIA_PLUGIN_EmitFrame`。`fps>0` 时按帧周期节流，抓取失败时退让 1ms 再试。
- 没有下游 Bind 且应用不取帧时，输出端口只保留最新一帧（旧帧自动归还 Pool），不会耗尽 Pool。
- `Stop` 会把 writeback 与 CRTC 解除绑定（`CRTC_ID=0`）；`Destroy` 释放 FB / GEM handle / DRM fd。
- 输出 Pool 被销毁重建（代次变化）后会丢弃旧 FB 缓存。
- RK3588 VOP2 writeback 不支持 YUV→RGB 转换：目标 CRTC 所连 connector 的 `color_format` 为 YCbCr 时
  BGR888 / ARGB8888 在创建阶段直接失败（不做 CPU 回退），需要把显示链路配成 RGB，或改用 NV12。
- 环境变量：`VO_WBC_PROFILE=1`（打印 commit/fence/total 耗时）、`VO_WBC_PROFILE_INTERVAL=N`、`VO_WBC_NONBLOCK=1`（nonblock commit）。
- 回写只读 CRTC 输出，不修改显示；但 atomic commit 需要 DRM master，桌面（Xorg 等）占用 master 时创建/抓取会失败。

## 在发布包中使用

```c
#include "media_vo_wbc.h"
```

- 头文件：`include/media_vo_wbc.h`
- 模块映像：`plugins/libmedia_vo_wbc.so`（与 `libmedia_vo_wbc.so.sig` 必须同目录）
- 链接：`-lmedia_vo_wbc -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_vo_wbc.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("VO_WBC", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

