# venc 教程：NV12 → H.264

程序构造 4 帧 320×240 NV12，送入 VENC 并把连续 H.264 packets 写成一个码流文件，便于直接交给 `vdec` 教程。需要 RK3588 MPP 和 `/dev/mpp_service`。

```sh
cmake -S examples/venc -B build/venc && cmake --build build/venc
LD_LIBRARY_PATH=lib:plugins ./build/venc/venc_tutorial [out.h264]
```

修改 `video_format` 为 H.265、码率或 GOP 可对比编码参数。输出 pool 必须足够容纳码流；设备不可用时程序会明确报告。
