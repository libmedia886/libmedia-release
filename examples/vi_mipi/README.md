# MIPI Camera / VI 采集教程

这个示例演示如何用 `VI` 模块从 RK3588 的 MIPI camera 采集 NV12 帧，并把 DMA-BUF 内容保存成原始 `.nv12` 文件。程序直接调用 `MEDIA_VI_*` API；不经过 DRM/VO，也不会占用显示器。

## 先确认设备

MIPI camera 在 RK3588 上通常经过两类节点：

| 节点 | 用途 | 建议格式 |
| --- | --- | --- |
| `/dev/video11` | RKISP main path，已经过 ISP，适合直接得到可查看的 YUV | NV12 |
| `/dev/video12` | RKISP self path，尺寸和裁剪由当前 ISP 配置决定 | NV12 |
| `/dev/video0` … `/dev/video9` | RKCIF 直出节点，通常是 Bayer RAW10，供需要原始数据的程序使用 | 由驱动和传感器决定 |

先查看本机节点与格式：

```sh
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video11 --list-formats-ext
v4l2-ctl -d /dev/video0 --list-formats-ext
```

本板默认程序使用 `/dev/video11` 和 `3840x2160 NV12`。如果你的传感器没有配置到这个 ISP 输出，先用 `media-ctl`/ISP 配置工具设置 MIPI pipeline，再运行示例。

## 参数

```text
./vi_mipi [device] [width] [height] [frames] [output_dir] [fps]
```

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `device` | `/dev/video11` | V4L2 视频节点；MIPI ISP 输出优先使用 `/dev/video11` 或 `/dev/video12` |
| `width` | `3840` | 请求宽度；驱动可能返回实际尺寸，实际值写入 `info.txt` |
| `height` | `2160` | 请求高度 |
| `frames` | `1` | 保存帧数 |
| `output_dir` | `frames_vi` | 输出目录 |
| `fps` | `30` | 请求帧率；驱动不支持时以驱动实际值为准 |

程序固定请求 `MEDIA_FORMAT_NV12` 和 `MEDIA_VIDEO_COMPRESS_NONE`。VI 的 DMA-BUF Pool 使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，因为 RK3588 V4L2 camera 驱动要求 CMA/连续内存。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/vi_mipi -B build/vi_mipi
cmake --build build/vi_mipi -j2
LD_LIBRARY_PATH=plugins:lib ./build/vi_mipi/vi_mipi
```

例如采集 ISP 的 1280x720 画面 10 帧：

```sh
LD_LIBRARY_PATH=plugins:lib ./build/vi_mipi/vi_mipi \
  /dev/video11 3840 2160 10 frames_vi 30
```

输出：

- `frames_vi/frame_000.nv12` …：驱动实际返回的 NV12 payload；
- `frames_vi/info.txt`：实际 `visible/storage/pitch/offset/len/timestamp/seq` 等 metadata。

NV12 文件可以用 ffplay 检查（尺寸按 `info.txt` 中实际值填写）：

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 3840x2160 frames_vi/frame_000.nv12
```

## 代码流程

1. `MEDIA_SYS_Init()` 初始化核心并通过直接链接的 `libmedia_vi.so` 注册 VI 插件。
2. `MEDIA_VIDEO_BuildLinearLayout()` 计算 NV12 Pool 的 canonical 容量。
3. `MEDIA_POOL_CreateWithMode(..., MEDIA_POOL_ALLOC_MODE_NATIVE)` 创建 camera 可导入的 CMA Pool。
4. `MEDIA_VI_SetAttr()` 设置 V4L2 节点、尺寸、帧率和格式。
5. `MEDIA_VI_Enable()` 完成 `S_FMT`、DMABUF 入队和 `STREAMON`。
6. `MEDIA_VI_GetFrame()` 取帧，读取 metadata，CPU 读同步后写文件。
7. `MEDIA_VI_ReleaseFrame()` 归还 buffer，最后 Disable、Destroy 和销毁 Pool。

## 常见问题

- `MEDIA_VI_SetAttr` 打开失败：检查设备节点、权限和 MIPI/ISP pipeline 是否已启用。
- `MEDIA_VI_Enable` 返回错误：检查请求的尺寸/格式是否在 `--list-formats-ext` 中，Pool 是否使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，以及 Pool 容量是否足够。
- 采集超时：通常表示传感器没有出流、MIPI lane/时钟配置不匹配，或 ISP 尚未配置；先用 `v4l2-ctl` 和 `media-ctl` 检查链路。
- `/dev/video0` 的 RAW10 不能直接用本示例的 NV12 请求。若要抓 Bayer 原始帧，需要使用驱动支持的 RAW 格式并扩展示例的 `MEDIA_FORMAT`、layout 和保存逻辑。
