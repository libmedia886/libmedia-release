# audio 教程：PCM → G711A

程序在 HOST pool 中生成一帧 8 kHz、16 bit、单声道 PCM 三角波，调用 `AUDIO_ENC` 编成 G.711 A-law 并写出码流。无需 ALSA 声卡；`AUDIO_IN/AUDIO_OUT` 需要真实设备。

```sh
cmake -S examples/audio -B build/audio && cmake --build build/audio
LD_LIBRARY_PATH=lib:plugins ./build/audio/audio_tutorial [out.g711a]
```

修改 `RATE`/`SAMPLES` 可观察采样率和每帧点数对输出长度的影响；G711 支持 80/160/240/320/480 点。AAC 需要 1024 点和 FFmpeg，可按 `include/media_audio.h` 的 `MEDIA_AUDIO_ENC_ATTR` 改为 AAC。
