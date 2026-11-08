# VO_WBC 入门：从正在显示的画面抓帧

VO_WBC（WriteBack Capture）利用 DRM writeback connector，把某个 CRTC 当前合成的画面回写成 DMA-BUF。它适合录制屏幕、截图或把显示内容再送给编码器。这个教程程序只执行能力探测，因为不同板卡的 writeback connector、CRTC 和桌面占用状态差异很大；没有这些信息时强行抓帧只会产生误导。

## 1. 编译和探测

```sh
cmake -S examples/vo_wbc -B build/vo_wbc
cmake --build build/vo_wbc -j2
LD_LIBRARY_PATH=lib:plugins ./build/vo_wbc/vo_wbc_tutorial
```

成功时会打印 `connector_id` 和 `crtc_id`。失败时先检查：

```sh
modetest -M rockchip -c
ls -l /dev/dri/card*
```

需要活动的 CRTC 和支持 writeback 的 DRM 驱动；桌面 compositor 独占 DRM master 时，抓帧也可能失败。

## 2. 真正抓帧需要哪些参数

`MEDIA_VO_WBC_ATTR` 的关键字段：

| 字段 | 含义 |
|---|---|
| `device` | DRM 设备，通常留空让插件自动选择 |
| `connector_id/crtc_id` | 探测到的 writeback connector/活动 CRTC |
| `width/height` | 抓取尺寸，通常等于 CRTC mode |
| `fps` | 回写频率；过高可能超过带宽 |
| `stride` | 每行字节数，0 表示自动；显式值必须匹配 format |
| `format` | `NV12`、`BGR888` 或 `ARGB8888` |
| `pool_id` | 应用创建的 DMA-BUF Pool，至少一个完整帧大小 |
| `output_depth` | 兼容字段；当前插件输出端没有额外队列 |

典型流程是：Probe → 创建 `NATIVE/COMPAT` Pool → `MEDIA_VO_WBC_CreateChn` → Start → `GetFrame` → CPU 读同步/写文件 → Release → Stop/Destroy。也可以 Bind `VO_WBC:output0` 到 VENC 直接录制。

## 3. 文件查看和录制

如果选择 NV12，抓到的文件可用：

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 1920x1080 frame.nv12
```

实际尺寸、pitch 和 modifier 必须以 frame metadata 为准。RGB 输出用 `-pixel_format bgr24` 或 `argb`，并按 stride 处理每行。

## 4. 常见错误

- `writeback probe failed`：硬件没有 writeback，或当前 CRTC 未连接显示器。
- `CreateChn` 失败：Pool 不是 DMA-BUF、大小不足、format 不被目标 CRTC 支持。
- `GetFrame` 超时：DRM master 被桌面占用、commit fence 未完成，或刷新率/带宽过高。
- NV12 花屏：VO_WBC 可能返回对齐 pitch，不能用 `width*height*3/2` 生搬硬套。

详见 [`docs/plugins/vo_wbc.md`](../../docs/plugins/vo_wbc.md) 和 [`include/media_vo_wbc.h`](../../include/media_vo_wbc.h)。
