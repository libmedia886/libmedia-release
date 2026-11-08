# Examples / 模块教程

这组例子是给第一次接触 libmedia 的人准备的。建议先读本页，再打开目标模块目录中的 `README.md`，最后阅读同目录的 `main.c`。每个目录都能单独用 CMake 编译；README 会解释它为什么这样配置、每个参数改变什么，以及失败时先检查哪里。`common/` 是教程共用代码，不是独立模块。

## 目录索引

| 目录 | 你会学到什么 |
| --- | --- |
| [`minimal/`](minimal/) | 初始化核心、验证签名插件 |
| [`load_all/`](load_all/) | 批量检查插件、依赖和加载上限 |
| [`tpg/`](tpg/) | 生成彩条、棋盘、渐变和移动方块 |
| [`vpss_16out/`](vpss_16out/) | 一个输入连接 16 个独立输出 |
| [`vi_mipi/`](vi_mipi/) | 从 RK3588 MIPI/ISP 采集 NV12 |
| [`clahe/`](clahe/)、[`conv/`](conv/)、[`resize_rga/`](resize_rga/)、[`csc_rga/`](csc_rga/)、[`rga/`](rga/) | 单路图像增强、卷积、缩放和颜色转换 |
| [`cap_dehaze/`](cap_dehaze/)、[`dcp_fast_dehaze/`](dcp_fast_dehaze/)、[`highlight_suppress/`](highlight_suppress/)、[`retinex/`](retinex/)、[`tnr/`](tnr/)、[`wavelet_nr/`](wavelet_nr/)、[`thermal/`](thermal/) | 去雾、降噪、高光抑制和伪彩 |
| [`blend_pyr/`](blend_pyr/)、[`dualview/`](dualview/)、[`edof/`](edof/)、[`exposure_fusion/`](exposure_fusion/)、[`fluo_visible_fusion/`](fluo_visible_fusion/)、[`mcf_fusion/`](mcf_fusion/)、[`stereo_3d/`](stereo_3d/)、[`thermal_lowlight_fusion/`](thermal_lowlight_fusion/)、[`vmix/`](vmix/)、[`vmix_rga/`](vmix_rga/) | 两路输入同步、融合、拼接和混合 |
| [`panorama/`](panorama/)、[`svm_3d/`](svm_3d/) | 使用 PTO/mesh 标定资产做环视投影 |
| [`demosaic/`](demosaic/)、[`raw_wb/`](raw_wb/) | RAW16 Bayer 去马赛克和白平衡 |
| [`eis/`](eis/)、[`osd/`](osd/)、[`transform/`](transform/) | 电子防抖、区域叠加和 LUT 变换 |
| [`audio/`](audio/) | PCM → G.711A 软件编码 |
| [`venc/`](venc/)、[`vdec/`](vdec/) | NV12 ↔ H.264 硬件编解码 |
| [`vo/`](vo/)、[`vo_wbc/`](vo_wbc/) | DRM 显示 connector 和 writeback 探测 |

## 1. 先建立三个概念

### 插件、模块和通道

发布包中的 `plugins/libmedia_xxx.so` 是一个插件，旁边必须有同名的 `.so.sig`。插件提供一个或多个模块类型，例如 `libmedia_tpg.so` 提供 `TPG`，`libmedia_venc.so` 提供 `VENC`。模块创建后还要指定通道号（通常从 `0` 开始）；同一个类型可以创建多个通道。

模块通过端口交换数据。常见输入端口叫 `input0`、输出端口叫 `output0`。`MEDIA_SYS_Bind("TPG", 0, "output0", "CLAHE", 0, "input0")` 的意思是把 TPG 通道 0 的输出接到 CLAHE 通道 0 的输入。没有 Bind 时，程序可以用 `MEDIA_SYS_SendFrame`/模块专用 `SendFrame` 手动送入，再用 `GetFrame` 取出。

### Pool、Buffer 和 metadata

Pool 是一组可重复使用的内存槽位，Buffer 是其中一个槽位。输入和输出模块通常不会替应用自动分配合适的 DMA-BUF，所以教程先用 `MEDIA_POOL_CreateWithMode` 创建 Pool，再把 `pool_id` 写入模块参数。图像处理和相机路径使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`；音频软件模块使用 `MEDIA_POOL_ALLOC_MODE_HOST`。模块销毁后才能销毁 Pool。

Buffer 只是一块内存，模块还需要知道这块内存代表什么。因此每个视频 Buffer 都附带 `MEDIA_BUFFER_META`：`media_format` 表示像素格式，`len` 表示有效字节数，`u.video` 中的 `visible_width/height`、`pitch_bytes`、`offset_bytes` 描述每个平面。教程手工造帧时会先填像素，再调用 `MEDIA_BUFFER_SetMeta`；忘记 metadata 是最常见的 `invalid frame` 原因。

### 格式、stride 和压缩

`NV12` 是 Y 平面后接交错 UV 平面，理论大小是 `width * height * 3 / 2`；`RGB888` 是每像素 3 字节；RAW16 是每像素 2 字节；编码码流没有固定的像素大小。实际 DMA-BUF 每行常会向上对齐，应该以 `MEDIA_VIDEO_BuildLinearLayout` 返回的 `pitch_bytes`、`offset_bytes` 和 Pool size 为准，不能只用可见宽度猜容量。README 中的 raw 文件没有文件头，播放时必须同时告诉 `ffplay` 正确的格式和尺寸。

## 2. 第一次运行：从 TPG 开始

TPG 不需要摄像头，它在 native Pool 中生成测试图，是所有图像教程的输入源。先运行：

```sh
cmake -S examples/tpg -B build/tpg
cmake --build build/tpg -j2
export LD_LIBRARY_PATH="$PWD/lib:$PWD/plugins"
./build/tpg/tpg_tutorial /tmp/color.nv12 640 480 0
./build/tpg/tpg_tutorial /tmp/checker.nv12 640 480 1
```

最后一个参数是图案：`0` 彩条、`1` 棋盘、`2` 渐变、`3` 纯色、`4` 移动方块。两个文件可以用下面命令查看：

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 /tmp/color.nv12
```

如果这里失败，先不要调算法模块：检查 `libmedia.so`、`plugins/libmedia_tpg.so`、`.sig` 是否在发布包中，并确认 `LD_LIBRARY_PATH` 同时包含 `lib` 和 `plugins`。

## 3. 每个处理教程都在做什么

大多数图像教程遵循同一条最小管线：

```text
TPG 或程序生成的帧
        │  VIDEO_FRAME metadata
        ▼
输入 Pool → 模块 input0 → 模块 output0 → 输出 Pool
                                      │
                                      ▼
                               GetFrame + 写 raw 文件
```

先编译，再从构建目录运行是最不容易出错的方式：

```sh
cmake -S examples/clahe -B build/clahe
cmake --build build/clahe -j2
(cd build/clahe && ./clahe_tutorial)
```

每个 README 会把实际命令简化为可复制版本。运行输出中的 `saved ... bytes`、尺寸和格式就是检查管线是否真的产出帧的证据。改变命令行参数后，建议保存到两个不同文件并比较：

```sh
cmp result-a.raw result-b.raw || echo '参数确实改变了输出'
sha256sum result-a.raw result-b.raw
```

## 4. raw 文件如何查看

README 会给出对应的 `ffplay` 命令。通用规则如下（尺寸要换成程序实际打印的值）：

| 输出格式 | ffplay 参数示例 | 常见大小 |
| --- | --- | --- |
| NV12 | `-pixel_format nv12` | `w*h*3/2`（还要考虑 stride） |
| RGB888 | `-pixel_format rgb24` | `w*h*3` |
| RGBA8888 | `-pixel_format rgba` | `w*h*4` |
| RAW16 / RGB_U16 | `-f rawvideo -pixel_format gray16le` 或用 Python/专业工具读取 | 每像素 2 或 6 字节 |

如果文件大小和理论值不一致，通常是行对齐或模块输出布局改变；优先使用程序打印的 metadata，不要截断文件来“凑”尺寸。编码输出（H.264/H.265）应交给 `ffprobe` 或 `ffplay`，音频 G.711A 应按 README 中的采样率、位宽和单声道设置播放器。

## 5. 参数应该怎样学习

每个教程把一组能工作的参数写在 `main.c` 中，并把最值得实验的字段暴露为命令行参数。建议一次只改一个参数：先保存默认输出，再改参数保存第二个输出，最后用 `cmp`/`sha256sum` 和 `ffplay` 对比。参数一般分为四类：

1. **尺寸**：宽、高、stride。宽高决定可见图像；stride 是每行实际占用字节，必须满足格式和硬件对齐要求。
2. **质量/算法**：例如 CLAHE 的 tile/grid、TNR 的 history/threshold、融合模块的 alpha 或 mode。数值越大不一定越好，README 会说明方向和限制。
3. **资源**：Pool 的 `pool_id`、slot size、slot count、queue depth。slot size 太小会在创建或处理时失败，slot count 太小会在多帧或 Bind 管线中耗尽。
4. **设备/资产**：MIPI 节点、DRM connector、MPP 设备、PTO/LUT/mesh 标定文件。没有这些外部条件时，程序应明确报错；这不是通过填空字符串可以解决的。

## 6. 出错时按这个顺序排查

1. **找不到插件或签名错误**：确认当前目录、`LD_LIBRARY_PATH`、`.so` 和 `.so.sig` 成对存在；不要手动修改发布包中的 `.so`。
2. **Create 失败**：检查格式是否被模块支持、Pool 是否先创建、Pool mode 是否正确、stride 是否满足对齐和最小值。
3. **Send/Bind 失败**：检查两端的格式、尺寸、端口名和 metadata；输入端已 Bind 后不能再手动 Send，输出端已 Bind 后不能再手动 Get。
4. **GetFrame 超时**：确认模块已 Start、上游已 Enable、上游确实在出帧；MPP 编解码还可能需要多个完整包才能产出第一帧。
5. **能运行但画面不对**：先用 TPG 的彩条/棋盘验证格式，再检查 stride、UV offset、色彩空间、旋转/裁剪和参数值。
6. **相机/显示失败**：用 `v4l2-ctl --list-devices`、`v4l2-ctl -d ... --list-formats-ext`、`ls -l /dev/dri` 检查设备和实际模式；README 会列出所需节点。

## 7. 推荐学习顺序

`minimal` → `load_all` → `tpg` → `clahe`/`resize_rga` → `blend_pyr`/`vmix` → `demosaic`/`raw_wb` → `venc`→`vdec` → `vi_mipi` → `vo`/`vo_wbc`。先学会单路输入和单帧输出，再学习双路同步、编码延迟、真实相机和显示设备。

## 教程类型

除了 `minimal` 和 `load_all` 这两个专门讲插件加载的教程，其余目录都会创建模块并运行一条最小可观察管线。输入优先使用 TPG 或程序生成的 DMA-BUF 测试帧，程序会设置 `VIDEO_FRAME` metadata、创建输入/输出 Pool、送帧、取帧并保存结果。

| 类型 | 目录 |
| --- | --- |
| 真实采集/显示 | `vi_mipi`, `vo`, `vo_wbc` |
| 处理和融合 | `vpss_16out`, `clahe`, `conv`, `cap_dehaze`, `dcp_fast_dehaze`, `highlight_suppress`, `retinex`, `tnr`, `wavelet_nr`, `thermal`, `resize_rga`, `csc_rga`, `blend_pyr`, `dualview`, `stereo_3d`, `edof`, `exposure_fusion`, `fluo_visible_fusion`, `thermal_lowlight_fusion`, `mcf_fusion`, `vmix`, `vmix_rga`, `svm_3d`, `panorama`, `eis`, `osd`, `rga`, `transform`, `raw_wb`, `demosaic` |
| 编解码/音频 | `venc`, `vdec`, `audio` |
| 生成输入 | `tpg` |

`svm_3d` 和 `panorama` 的程序也是真实送帧管线，但必须额外提供项目标定文件；README 说明了参数位置。`venc`/`vdec` 需要 MPP，`vi_mipi`/`vo`/`vo_wbc` 需要对应的摄像头或 DRM 设备，设备不存在时程序会打印原因并退出。

每个示例都从发布包根目录构建，例如：

```sh
cmake -S examples/vi_mipi -B build/vi_mipi
cmake --build build/vi_mipi
```
