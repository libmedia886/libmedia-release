# VPSS 入门：一路输入生成 16 路输出

VPSS 是 GLES 视频处理模块，可对一路输入做裁剪、缩放、旋转、翻转、保宽高比留边和格式转换。本教程把 1920×1080 NV12 输入同时变成 16 路 480×270 NV12，每路设置不同变换，最后在 CPU 端拼成 4×4 RGB24 画布。它展示了“一个 input0、多个 outputN、每路独立 Pool”的完整结构。

## 1. 编译和运行

```sh
cmake -S examples/vpss_16out -B build/vpss_16out
cmake --build build/vpss_16out -j2
LD_LIBRARY_PATH=plugins:lib ./build/vpss_16out/vpss_16out frames 1 0
cat frames/info.txt
```

命令行：`./vpss_16out <输出目录> [帧数] [间隔毫秒] [源]`

- 输出目录：保存 `frame_000.rgb` 等文件和 `info.txt`。
- 帧数：例如 `10` 可观察 moving source 的连续结果。
- 间隔毫秒：每帧取完后的等待时间，`0` 表示不等待。
- 源：省略或写 `tpg` 使用色条；写 `img:file.rgb` 使用 1920×1080 裸 RGB24 文件。

查看默认画布：

```sh
ffplay -f rawvideo -pixel_format rgb24 -video_size 1920x1080 frames/frame_000.rgb
```

## 2. 16 路分别做什么

代码中的 `k_tiles[16]` 是教程参数表：前 4 路分别旋转 0/90/180/270 度；接着演示水平翻转、垂直翻转、双翻转和 keep-aspect；后几路组合旋转/翻转。每路都设置：

- `output_id`：端口编号，对应 `output0..output15`。
- `out_width/out_height`：输出可见尺寸，本例 480×270。
- `crop_x/y/w/h`：从输入取的区域；全图是 0,0,1920,1080。
- `rotate`：0/90/180/270。
- `flip_h/flip_v`：水平/垂直翻转开关。
- `keep_aspect_ratio`：1 时保持源宽高比，空白区域留边；0 时直接拉伸。
- `in_fps/out_fps`：帧率控制，-1 表示关闭节流。
- `output_format/compress`：本例线性 NV12/NONE。
- `color_space/dynamic_range`：本例 BT.601 limited、SDR8；错误设置会导致颜色偏差。

## 3. Pool 和端口

输入 Pool 0 有 6 个 1920×1080 NV12 槽，供 TPG 或手动 `img:` 发送。每个输出端口有自己的 Pool（Pool 1..16），每个 4 个 480×270 槽。输出 Pool 不能共用同一个 ID，否则不同端口的生命周期会互相干扰。所有视频 Pool 用 `NATIVE`，以便 GLES 导入 DMA-BUF。

TPG 源通过：

```c
MEDIA_SYS_Bind("TPG", 0, "output0", "VPSS", 0, "input0");
```

`img:` 模式不创建 TPG，而是把 RGB24 转换成 NV12 后用 `MEDIA_SYS_SendFrame("VPSS",0,"input0",...)` 手动送入。

## 4. 输出和 metadata

程序每路调用 `MEDIA_SYS_GetFrame("VPSS",0,"outputN",...)`，读取 `MEDIA_BUFFER_META` 中真实的 visible size、pitch、offset，再把 NV12 转 RGB24 放到画布。取到的每路 Buffer 都必须用对应的 `MEDIA_SYS_ReleaseFrame` 归还。

## 5. 常见问题

- 只有部分输出：某一路 Pool 创建失败、硬件能力不支持该旋转/格式，或 GetFrame 超时；查看终端的 `N/16 outputs`。
- 颜色偏差：输入是 BT.601 limited，本例按同一矩阵转换；实际摄像头可能是 BT.709/full range。
- `img:` 文件错误：必须是无文件头、恰好 `1920*1080*3` 字节的 RGB24。
- GLES 创建失败：检查 `/dev/dri/renderD*`、GPU 驱动、DMA-BUF Pool 和 `LD_LIBRARY_PATH`。

详见 [`docs/plugins/vpss.md`](../../docs/plugins/vpss.md)、[`include/media_vpss.h`](../../include/media_vpss.h)。
