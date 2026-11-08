# VI MIPI 示例默认分辨率与板上 ISP 模式不匹配

metadata:
- date: 2026-11-07 22:53:58
- kind: validation
- status: fixed
- repository: /root/libmedia-release
- summary: 当前 RK3588 MIPI/ISP 输出固定为 3840x2160，1920x1080 请求被 V4L2 QBUF 拒绝。

## Symptom

MEDIA_VI_Enable(/dev/video11) failed: -1，插件输出 VI: QBUF: Invalid argument。

## Reproduction

```bash
LD_LIBRARY_PATH=plugins:lib ./build/vi_mipi/vi_mipi /dev/video11 1920 1080 1 frames_vi 30
```

## Evidence

- VI: QBUF: Invalid argument
- VI: capture started /dev/video11 3840x2160 stride=3840
- frame 0: 12441600 bytes

## Affected Files

- examples/vi_mipi/vi_mipi.c
- examples/vi_mipi/README.md

## Root Cause

板上 rkisp_mainpath 当前格式为 3840x2160 NV12，驱动不接受示例请求的 1920x1080 buffer/layout。

## Fix Or Workaround

将 vi_mipi 默认宽高改为 3840x2160，并在 info.txt 写出驱动实际 layout；3840x2160 实测成功保存 12441600 字节 NV12 帧。

## Recurrence Prevention

运行 VI 示例前用 v4l2-ctl -d <node> --all 查看当前 Width/Height、Pixel Format、Size Image；教程默认值必须匹配板上当前 ISP pipeline。

## Related

- None
