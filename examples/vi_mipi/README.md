# VI / MIPI Camera 入门：从摄像头采集 NV12

VI（Video Input）把 V4L2 摄像头节点变成 libmedia 的视频源。这个教程以 RK3588 MIPI camera 的 RKISP main path 为例，从 `/dev/video11` 采集 NV12 帧并保存为裸文件。程序不经过 VO，不会自动显示画面，便于先确认“摄像头确实在出流”。

## 1. 先确认硬件和节点

MIPI 摄像头必须已经接好排线、供电、设备树和 ISP 配置。先查看节点：

```sh
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video11 --list-formats-ext
v4l2-ctl -d /dev/video0 --list-formats-ext
media-ctl -p
```

常见节点：

| 节点 | 通常用途 |
|---|---|
| `/dev/video11` | RKISP main path，已经过 ISP，常用 NV12 |
| `/dev/video12` | RKISP self path，尺寸/裁剪依 ISP 配置 |
| `/dev/video0..9` | RKCIF 直出，常见为 Bayer RAW10/RAW12 |

不要看到 `/dev/video0` 就直接当作 NV12；最终格式以 `--list-formats-ext` 和驱动 `S_FMT` 返回值为准。

## 2. 编译、采集和查看

```sh
cmake -S examples/vi_mipi -B build/vi_mipi
cmake --build build/vi_mipi -j2
LD_LIBRARY_PATH=plugins:lib ./build/vi_mipi/vi_mipi
```

命令行格式：

```text
./vi_mipi [device] [width] [height] [frames] [output_dir] [fps]
```

默认值是 `/dev/video11 3840 2160 1 frames_vi 30`。例如采 10 帧：

```sh
LD_LIBRARY_PATH=plugins:lib ./build/vi_mipi/vi_mipi \
  /dev/video11 3840 2160 10 frames_vi 30
cat frames_vi/info.txt
ffplay -f rawvideo -pixel_format nv12 -video_size 3840x2160 frames_vi/frame_000.nv12
```

如果驱动协商出的实际尺寸与请求不同，以 `info.txt` 的 `visible/storage/pitch/offset/len` 为准。`frame_000.nv12` 等文件是原始 payload；`info.txt` 还记录时间戳、序号、DRM fourcc 和 modifier。

## 3. 每个参数为什么存在

- `device`：V4L2 节点路径。换相机或 ISP path 时修改。
- `width/height`：向驱动请求的可见尺寸；驱动可能选择最接近的支持 mode。
- `frames`：要保存多少帧。每帧都要及时 Release，避免卡住驱动。
- `output_dir`：保存帧和 `info.txt` 的目录，会自动创建。
- `fps`：请求帧率；传感器不支持时驱动会回退到实际值。

程序固定请求 `MEDIA_FORMAT_NV12`、`MEDIA_VIDEO_COMPRESS_NONE`。NV12 是线性 Y+UV 两 plane，适合直接给 ffplay、RGA、VPSS 或 VENC。

## 4. Pool、零拷贝和生命周期

VI 使用 V4L2 `V4L2_MEMORY_DMABUF`：Enable 时从应用创建的 Pool 取 `buf_cnt` 个 DMA-BUF 入队，驱动填满后返回给 `GetFrame`。因此：

1. 先 `MEDIA_VIDEO_BuildLinearLayout` 计算大小。
2. 用 `MEDIA_POOL_CreateWithMode(..., MEDIA_POOL_ALLOC_MODE_NATIVE)` 创建 CMA/连续内存 Pool；本例 6 个槽，入队 4 个。
3. `MEDIA_VI_SetAttr` 设置 device、format、fps、pool_id。
4. `MEDIA_VI_Enable` 完成 S_FMT、入队和 STREAMON。
5. 循环 `MEDIA_VI_GetFrame`，CPU 读同步、保存、`MEDIA_VI_ReleaseFrame`。
6. 退出时 Disable、Destroy、Pool_Destroy。

Pool 槽数太少会导致驱动无法维持流水线；槽大小太小会在 Enable 时失败。不要在 Send/Get 期间销毁 Pool。

## 5. 常见问题

- `SetAttr` 打不开：设备路径错误、权限不足或节点被其他程序占用。
- `Enable` 失败：请求的宽高/NV12 不在该节点支持列表，或 Pool 不是 NATIVE、容量不足。
- `GetFrame timeout`：传感器没有出流、MIPI lane/时钟错误、ISP pipeline 未配置；先用 v4l2-ctl 验证。
- RAW 节点不能采 NV12：RAW10/RAW12 需要专门的 unpack + demosaic 流程，不能套用本示例。
- 画面花屏：按 metadata 的 pitch/offset 保存和查看，不能把 stride 当作 width。

详见 [`docs/plugins/vi.md`](../../docs/plugins/vi.md)、[`include/media_vi.h`](../../include/media_vi.h)。
