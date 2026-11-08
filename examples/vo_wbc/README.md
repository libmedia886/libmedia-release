# vo_wbc 教程：writeback 能力探测

程序调用 `MEDIA_VO_WBC_Probe` 查询 writeback connector 和 active CRTC。抓帧前必须确认这个能力；没有 writeback 的板卡会明确返回错误。

```sh
cmake -S examples/vo_wbc -B build/vo_wbc && cmake --build build/vo_wbc
LD_LIBRARY_PATH=lib:plugins ./build/vo_wbc/vo_wbc_tutorial
```

探测成功后按 `docs/plugins/vo_wbc.md` 创建 DMA-BUF pool、设置 NV12 尺寸并调用 `Start/GetFrame`；桌面占用 DRM master 时抓帧会失败。
