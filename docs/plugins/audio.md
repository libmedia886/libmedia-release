# audio

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `AUDIO_IN` | SOURCE / SINK / FILTER |
| `AUDIO_OUT` | SOURCE / SINK / FILTER |
| `AUDIO_ENC` | SOURCE / SINK / FILTER |
| `AUDIO_DEC` | SOURCE / SINK / FILTER |

产物：`libmedia_audio.so` 与公共头 `include/media_audio.h`。只依赖 libmedia 公开 SDK
（`media_api.h` / `media_plugin.h`）；ALSA（libasound）与 FFmpeg（avcodec / avutil / swresample，仅 AAC）
只在本模块内链接，core 不再依赖它们。

## 类型与实例名

| type | 作用 | 端口 | 创建 API |
| --- | --- | --- | --- |
| `AUDIO_IN` | ALSA 采集，输出 planar PCM（S16LE） | `output0` | `MEDIA_AUDIO_IN_CreateChn(dev, chn, attr)` |
| `AUDIO_OUT` | ALSA 播放 | `input0` | `MEDIA_AUDIO_OUT_CreateChn(dev, chn, attr)` |
| `AUDIO_ENC` | PCM -> G711A / G711U / AAC-LC(ADTS) | `input0` / `output0` | `MEDIA_AUDIO_ENC_CreateChn(chn, attr)` |
| `AUDIO_DEC` | 码流 -> PCM | `input0` / `output0` | `MEDIA_AUDIO_DEC_CreateChn(chn, attr)` |

实例名为 `<TYPE>_<id>`。`AUDIO_IN` / `AUDIO_OUT` 的 `id = MEDIA_AUDIO_ID(dev, chn) = dev * 256 + chn`
（`dev`、`chn` 取 0..255），dev0 时 `id == chn`，所以 `MEDIA_SYS_Bind("AUDIO_IN", chn, "output0", ...)`
与原内置版本写法一致；非零 dev 传 `MEDIA_AUDIO_ID(dev, chn)`（原来的 `AUDIO_IN_<dev>_<chn>` 完整模块名写法不再支持）。
`AUDIO_ENC` / `AUDIO_DEC` 的 id 就是 chn。端口名固定、必须显式写出。

## 属性

`MEDIA_AUDIO_IN_ATTR` / `MEDIA_AUDIO_OUT_ATTR` / `MEDIA_AUDIO_ENC_ATTR` / `MEDIA_AUDIO_DEC_ATTR` 定义见
`media_audio.h`，字段、默认值与原内置版本一致：采样率 8000/16000/44100/48000（默认 48000）、位宽 16、
声道 mono(1)/stereo(2)（默认 stereo）、`frame_num` 默认 8、`point_num_per_frame` 默认 1024（G711 默认 160
G711 取 80/160/240/320/480，AAC 固定 1024）、`device` 默认 `plughw:0,0`、AAC `bit_rate` 默认 128000、
`transport_type` 目前只有 `MEDIA_AUDIO_AAC_TRANSPORT_ADTS`。

## Pool 要求

`AUDIO_IN`、`AUDIO_ENC`、`AUDIO_DEC` 的**输出 Pool 由应用创建**，并通过 `output_pool_id` 传入：

- 必须是 `MEDIA_POOL_ALLOC_MODE_HOST`（模块用 `MEDIA_BUFFER_IsHostMemory` 逐槽校验）
- 槽大小用 `MEDIA_AUDIO_CalcFramePoolSize`（IN / DEC 的 PCM 帧）或 `MEDIA_AUDIO_CalcStreamPoolSize`（ENC 码流）
- 槽数不少于 `frame_num`，并覆盖下游队列深度
- Pool 必须先于模块创建，并晚于模块销毁。`AUDIO_OUT` 不产生 buffer，无 Pool 要求。

不满足时 `CreateChn` 失败。

## 两种工作方式

- **Bind**：`MEDIA_SYS_Bind` 后由模块内部线程收发。Bind 前 core 比较两端端口的 caps blob
  （采样率、位宽、声道、点数，码流端口另含 codec），不一致返回 `MEDIA_ERR_PORT_INCOMPATIBLE`。
  Bind 不会隐式 Start；各模块各自 `Start`。Bind 上游送来的 buffer 的 metadata 会完整校验（plane 偏移/长度、
  参数范围），非法帧被丢弃并归还。
- **手动**：端口未 Bind 时用 `MEDIA_AUDIO_*` 的 Send / Get / Release。输出端口已 Bind 时 Get 返回
  `MEDIA_ERR_PIPELINE_STATE`；输入端口已 Bind 时 Send 同理。`UnBind` 后立即回到手动模式。
  也可用 `MEDIA_SYS_SendFrame` 向未 Bind 的输入送 buffer，模块会处理，结果进入手动输出队列。
  `AUDIO_IN` 手动模式下不后台采集，`GetFrame` 现取（`timeout_ms` < 0 一直等，0 不等）。

编码 / 解码结果在手动模式下进入模块内最多 32 项的输出队列；队列满时新结果被丢弃（buffer 归还），应及时 Get。

## 示例

```c
#include "media_api.h"
#include "media_audio.h"

size_t fsz = MEDIA_AUDIO_CalcFramePoolSize(48000, 16, MEDIA_AUDIO_SOUND_MODE_STEREO, 1024)
MEDIA_POOL_CreateWithMode(1, fsz, 8, MEDIA_POOL_ALLOC_MODE_HOST)
MEDIA_AUDIO_IN_ATTR in = {.sample_rate = 48000, .snd_mode = MEDIA_AUDIO_SOUND_MODE_STEREO
                          .point_num_per_frame = 1024, .output_pool_id = 1}
MEDIA_AUDIO_IN_CreateChn(0, 0, &in)
MEDIA_AUDIO_IN_Start(0, 0)
MEDIA_AUDIO_FRAME f
if (MEDIA_AUDIO_IN_GetFrame(0, 0, &f, 1000) == 0) {
    /* f.virt_addr[0/1] 是左/右声道 planar 数据 */
    MEDIA_AUDIO_IN_ReleaseFrame(0, 0, &f)
}
```

完整链路 `AUDIO_IN -> AUDIO_ENC -> AUDIO_DEC -> AUDIO_OUT` 的 Bind 写法见 `tests/test_audio.c` 的 `hw_bind_chain`。

## 限制

- 仅 16bit PCM；AAC 仅 AAC-LC + ADTS，每帧 1024 点；FFmpeg 编码器有延迟，前一两帧可能不产出码流。
- `AUDIO_OUT` 的输入帧点数不得超过创建时的 `point_num_per_frame`，参数需与创建时一致。
- `Enable` / `Disable` 等价于 `Start` / `Stop`。
- 每个 `AUDIO_*` 实例的手动 API 与内部线程用同一把锁串行化，同一实例不要在多个线程里并发调用手动 API。
- 音频设备在 `Start` 时才打开；设备不可用时 `Start` 返回 `MEDIA_ERR_RESOURCE`。

## 在发布包中使用

```c
#include "media_audio.h"
```

- 头文件：`include/media_audio.h`
- 模块映像：`plugins/libmedia_audio.so`（与 `libmedia_audio.so.sig` 必须同目录）
- 链接：`-lmedia_audio -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_audio.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("AUDIO_IN", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

