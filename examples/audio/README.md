# AUDIO 音频编码入门：PCM → G.711 A-law

本例是一个**不需要声卡**的最小音频处理教程。程序在内存中生成一小段 8 kHz、16 bit、单声道 PCM 三角波，把 PCM 送给 `AUDIO_ENC`，取出 G.711 A-law 码流写入文件。掌握这个例子后，可以把“生成 PCM”替换成麦克风采集（`AUDIO_IN`），把编码结果替换成网络发送或文件封装。

## 1. 先理解四个概念

- **采样率（sample rate）**：每秒采多少个样本。`8000` 表示电话语音常用的 8 kHz。
- **位宽（bit width）**：一个样本占多少位。本例是 `16`，因此一个单声道样本占 2 字节。
- **声道模式（sound mode）**：`MONO` 是 1 声道，`STEREO` 是左右 2 声道。立体声一帧的字节数约为单声道的两倍。
- **每帧采样点数（point_num_per_frame）**：一次送入编码器的样本数。本例 `160` 点，在 8 kHz 下约等于 20 ms：`160 / 8000 = 0.02` 秒。

本例的端口是 `AUDIO_ENC` 的 `input0`（PCM）和 `output0`（压缩码流）。程序使用手动 `SendFrame/GetStream`，因此不用写 `MEDIA_SYS_Bind`。

## 2. 编译和运行

在仓库根目录执行：

```sh
cmake -S examples/audio -B build/audio
cmake --build build/audio -j2
LD_LIBRARY_PATH=lib:plugins ./build/audio/audio_tutorial audio_g711a.bin
ls -l audio_g711a.bin
```

默认输出文件是 `audio_g711a.bin`。G.711 A-law 每个 PCM 样本编码成 1 字节，所以本例通常会得到约 160 字节的有效码流（还可能受编码器帧头或实现细节影响）。它是裸码流，没有 WAV 文件头。

可以用 FFmpeg 播放裸 G.711 A-law：

```sh
ffplay -f alaw -ar 8000 -ac 1 audio_g711a.bin
```

如果只想确认插件加载，不运行编码，可执行 `ldd build/audio/audio_tutorial`，确认 `libmedia.so` 与 `libmedia_audio.so` 能找到。

## 3. 代码中的参数和 Pool

`main.c` 顶部的常量是入门时最值得修改的地方：

| 参数 | 本例值 | 作用 |
|---|---:|---|
| `RATE` | 8000 | PCM 采样率，必须和 `MEDIA_AUDIO_ENC_ATTR.sample_rate` 一致 |
| `SAMPLES` | 160 | 每次送入的采样点数；G.711 常用 80/160/240/320/480 |
| `bit_width` | 16 | PCM 位宽；本例 little-endian S16LE |
| `snd_mode` | `MONO` | 声道数；改成 `STEREO` 时要同时生成左右样本 |
| `codec` | `G711A` | 编码器类型，可查看 `include/media_audio.h` 中的 `MEDIA_AUDIO_CODEC_*` |
| `output_pool_id` | 1 | 编码器输出码流所在的外部 Pool ID |

Pool 是 libmedia 管理的一组可复用 Buffer。编码器不会偷偷申请任意大小的内存，应用必须先创建输出 Pool：

```c
size_t pcm_bytes = MEDIA_AUDIO_CalcFramePoolSize(RATE, 16, MONO, SAMPLES);
size_t stream_bytes = MEDIA_AUDIO_CalcStreamPoolSize(CODEC_G711A, RATE, 16,
                                                      MONO, SAMPLES, 0);
MEDIA_POOL_CreateWithMode(1, pcm_bytes > stream_bytes ? pcm_bytes : stream_bytes,
                          8, MEDIA_POOL_ALLOC_MODE_HOST);
```

音频编码是 CPU 软件路径，使用 `HOST` Pool 就够了；视频模块常见的 `NATIVE` DMA-BUF Pool 不必套用到这里。Pool 必须在 `MEDIA_AUDIO_ENC_CreateChn` 前创建，在 Destroy 后销毁。

## 4. 从 PCM 换成真实麦克风

真实设备的流程是 `AUDIO_IN → AUDIO_ENC`：

1. 用 `aplay -l` / `arecord -l` 找到 ALSA 设备。
2. 用 `MEDIA_AUDIO_IN_ATTR` 设置 `dev`、`chn`、采样率、位宽、声道和 `output_pool_id`。
3. 创建并启动 `AUDIO_IN`，循环 `MEDIA_AUDIO_IN_GetFrame`。
4. 把得到的 `MEDIA_AUDIO_FRAME` 送入 `MEDIA_AUDIO_ENC_SendFrame`。
5. 每个 frame 用 `MEDIA_AUDIO_IN_ReleaseFrame` 归还；每个 stream 用 `MEDIA_AUDIO_ENC_ReleaseStream` 归还。

也可以用 `MEDIA_SYS_Bind("AUDIO_IN", id, "output0", "AUDIO_ENC", 0, "input0")` 让框架在线程中搬运帧。`AUDIO_IN` 和 `AUDIO_ENC` 的采样率、声道、每帧点数必须匹配。

## 5. 常见错误

- `audio pool setup failed`：Pool ID 已被其他模块占用，或槽大小小于 `Calc*PoolSize` 的结果。
- `AUDIO_ENC create failed`：检查 codec、采样率和每帧点数是否在插件支持范围内。
- `AUDIO_ENC stream timeout`：输入帧的 `sample_rate/bit_width/snd_mode/point_num_per_frame` 与创建属性不一致，或插件未正确加载。
- `ffplay` 播放速度不对：裸文件没有自描述信息，必须显式给出 `-f alaw -ar 8000 -ac 1`。

更完整的字段和 Bind 规则见 [`docs/plugins/audio.md`](../../docs/plugins/audio.md) 与 [`include/media_audio.h`](../../include/media_audio.h)。
