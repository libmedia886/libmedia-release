# blend_pyr：新手实帧教程

`BLEND_PYR` 用第三路 mask 决定两张图的比例：mask 的 Y=255 表示完全取 `input0`，Y=0 表示完全取 `input1`，中间值是线性混合。教程额外创建一个 mask Pool，并调用 `MEDIA_BLEND_PYR_SetMask`；因此你能看到“同样两路输入，仅改变 mask 就得到不同结果”。这正是做曝光、透明叠加或多传感器加权时最重要的概念。


## 先理解这段教程在做什么

把一个视频模块想成“有输入插口和输出插口的黑盒子”。本教程为了不依赖摄像头，程序在内存中生成两张 **640×480** 的测试图：`input0` 是从左到右变亮，`input1` 是从上到下变亮。两张图通过 DMA-BUF 放进模块，模块处理一次后从 `output0` 取回一帧，写成没有文件头的 raw 文件。

这里会反复出现四个词：

* **Pool**：一组可重复使用的 DMA-BUF。输入 Pool 的 buffer 交给模块后，模块处理完才归还；输出 Pool 提供模块写结果的空 buffer。Pool 不够大或没有空闲 slot，通常会在 `CreateModule`、`SendFrame` 或 `GetFrame` 时报错。
* **端口（port）**：模块的插口。`input0`、`input1` 是两路输入，`output0` 是输出。手动送帧时端口名必须完全匹配；把 `input1` 写成 `input01` 会失败。
* **VIDEO_FRAME metadata**：跟在 buffer 上的描述信息，包括格式、可见宽高、每行 pitch、plane offset、字节数、序号和时间戳。模块依靠它判断 DMA-BUF 怎样解释，不能只填一块裸内存。
* **LINEAR/NV12**：本教程使用线性 NV12。NV12 的 Y 平面先放 `width × height` 个亮度字节，后面紧跟交错 UV 平面；每行可能为了硬件对齐而比 width 更长，所以查看文件必须使用程序打印的尺寸和格式。

程序中的处理顺序是：`MEDIA_SYS_Init` → 加载插件（校验 `.so.sig`）→ `MEDIA_VIDEO_BuildLinearLayout` → 创建输入/输出 Pool → 创建并启动模块 → 向两路输入送帧 → 从 `output0` 取帧 → 读取 metadata/像素 → Release → Stop/Destroy/Exit。多数插件通过 `MEDIA_PLUGIN_CreateModule`、`MEDIA_SYS_SendFrame`、`MEDIA_SYS_GetFrame` 完成；MCF、PANO、SVM3D 使用各自头文件中的 `Create/SendFrame/GetFrame` 专用 API，但生命周期和 buffer 所有权相同。阅读 `main.c` 时可以按这个顺序对照。

## 环境和第一次运行

请在发布包根目录执行命令。目标板应有 RK3588 的 Mali/EGL 驱动；没有 GPU 的普通 x86 机器只能完成编译，运行时会在 `Start` 或导入 DMA-BUF 时失败。插件文件和同名签名文件必须同时存在：例如 `plugins/libmedia_blend_pyr.so` 与 `plugins/libmedia_blend_pyr.so.sig`。

```sh
export LD_LIBRARY_PATH="$PWD/plugins:$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake -S examples/blend_pyr -B build/blend_pyr
cmake --build build/blend_pyr -j2
(cd build/blend_pyr && ./blend_pyr_tutorial ...)
```

`CMakeLists.txt` 用的是发布目录里的 `include/`、`lib/libmedia.so` 和 `plugins/`，因此换目录后请用 `-DLIBMEDIA_ROOT=/绝对路径/libmedia-release` 指向实际位置。看到 `load ... failed` 时先检查 `.so`/`.sig` 是否配对、`LD_LIBRARY_PATH` 是否包含 `lib`；看到 `start failed` 时通常是 EGL/DMA-BUF 驱动或 Pool layout 不满足要求。

## 输出 raw 文件怎样查看

raw 文件没有 JPEG/PNG 头，播放器不会自动知道宽高。程序成功时会打印 `bytes=...` 和 metadata 中的可见尺寸。NV12 输出可用：

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 output.nv12
```

如果 metadata 打印的尺寸不同，以打印值为准；pitch 对齐字节已经包含在文件中时，不能简单用 `width*height*3/2` 猜大小。RGB888 输出用 `-pixel_format rgb24`，ABGR8888 输出用 `-pixel_format abgr`。按 `q` 退出 ffplay。比较两个模式最简单的方法是分别生成两个文件，再用 `cmp` 或 ffplay 并排观察；raw 文件不同不代表参数错误，参数本来就会改变每个像素。

## 常见错误排查

1. **`MEDIA_PLUGIN_Load` 失败**：路径错、`.sig` 缺失或签名与 `.so` 不匹配。不要手工改动插件二进制。
2. **`CreateModule` 返回错误**：宽高、格式、stride、Pool 容量或属性取值不符合头文件限制。先使用本教程的默认值，再一次只改一个字段。
3. **`SendFrame` 超时**：对应输入端口没有空闲队列，或上一帧仍被应用持有。输入 buffer 送出成功后所有权交给模块，不要再次 `PutBuffer`。
4. **`GetFrame` 超时**：两路没有同时到达、时间戳相差太大，或者 GPU 在运行时不可用。先确认两个 `SendFrame` 都返回 0，再把等待时间设为 5000 ms。
5. **ffplay 花屏**：pixel format/尺寸写错，或把带 pitch 的 buffer 当成紧凑图像。以程序打印的 metadata 为准。
6. **退出时卡住**：仍有输出帧未 `ReleaseFrame`。任何成功的 `GetFrame` 都必须成对调用 Release。

模块的完整 ABI、字段边界和限制请继续阅读对应的 [`docs/plugins/blend_pyr.md`](../../docs/plugins/blend_pyr.md) 与 `include/media_blend_pyr.h`；本 README 只解释“怎样第一次跑通以及怎样观察参数差异”。

## 这个模块的参数（先看懂再修改）

| 参数/字段 | 初学者理解 | 本例值与可尝试的变化 |
|---|---|---|
| `mode`（命令行第 1 项） | 选择 mask 的灰度 | `0` 将 mask 填 255，输出接近 input0；`1` 填 128，输出约为两路平均。修改 `main.c` 中的 `memset` 可试 0（完全 input1）。 |
| `width/height` | 每路图像可见像素 | 640×480；宽必须 16 对齐、高必须 2 对齐。 |
| `input_stride/output_stride` | 一行实际占多少字节 | 由 `MEDIA_VIDEO_BuildLinearLayout` 计算，不能随意写 640。 |
| `input_format` | 像素编码 | 只能 NV12；RGB 文件不能送进来。 |
| `input_depth` | 每个端口能排队几帧 | 0 表示默认 4；实时相机可增大，内存也会增加。 |
| `output_pool_id` | 结果从哪个 Pool 取 | 本例用专用 Pool 29，避免输入 Pool 被输出占满。 |


## 本程序的输入、端口和 Pool

输入端口 `input0`、`input1` 都是 NV12，`input2` 是可选 mask 端口，输出是 `output0`。示例先把 mask 交给 `SetMask`，再按“先 input1、后 input0”的顺序送图；先送副路可避免主路到达时模块立即处理而另一侧还没有帧。mask 只读取 Y 平面，UV 会被忽略。每个输入 Pool 有 4 个 slot，输出 Pool 也有 4 个 slot；Pool slot 必须能容纳完整 NV12 allocation size。

## 输出和结果对比

默认运行会生成 `blend_pyr_frame.nv12`。命令行第 2 项可以改文件名，第 3 项可以传插件绝对路径：

```sh
(cd build/blend_pyr && ./blend_pyr_tutorial 0 mask255.nv12)
(cd build/blend_pyr && ./blend_pyr_tutorial 1 mask128.nv12)
cmp mask255.nv12 mask128.nv12   # 应该不同
```



## 从教程走向真实相机

本例使用合成图只是为了让每个人都能重复实验。接入真实设备时，把 `make_frame` 替换成 VI/解码器给出的 DMA-BUF，保持相同的 `MEDIA_VIDEO_LAYOUT` 和 `MEDIA_BUFFER_META_VIDEO_FRAME`；两路相机必须在同一时刻、同一分辨率、同一格式到达。推荐先用 `examples/vi_mipi` 验证摄像头，再通过 `MEDIA_SYS_Bind("VI", ..., "<模块 type>", ..., "input0")` 或本模块专用 `SendFrame` API 连接模块（具体 type 和端口以本模块文档为准）。不要在应用侧把 DMA-BUF 映射后复制成普通 malloc 指针，GPU/RGA 模块需要真正的 dma-buf fd。

真实产品中 mask 可以来自 UI、分割网络或另一块传感器；用 `MEDIA_SYS_SendFrame("BLEND_PYR",0,"input2",mask,...)` 动态换 mask 时，mask 的尺寸/stride 必须与两路输入完全相同。

## 下一步阅读

* 程序：[`main.c`](main.c)；构建文件：[`CMakeLists.txt`](CMakeLists.txt)。
* 公共 API：[`include/media_blend_pyr.h`](../../include/media_blend_pyr.h)。
* 完整模块约束：[`docs/plugins/blend_pyr.md`](../../docs/plugins/blend_pyr.md)。
