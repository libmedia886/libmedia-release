# mcf_fusion：新手实帧教程

`MCF_FUSION_CL`（多相机融合）接收一条彩色管线和一条单色/灰度管线，按时间戳或到达顺序配对后输出。它适合“彩色相机保留颜色、单色相机提供细节”的场景。与前面直接调用 `MEDIA_PLUGIN_CreateModule` 的 GLES 教程不同，本程序展示 MCF 的分组、通道和算法参数 API。


## 先理解这段教程在做什么

把一个视频模块想成“有输入插口和输出插口的黑盒子”。本教程为了不依赖摄像头，程序在内存中生成两张 **640×480** 的测试图：彩色 pipe 是从左到右变亮，单色 pipe 是从上到下变亮。两张图通过 DMA-BUF 放进 MCF 的两个 pipe，模块处理一次后从 chn=0 取回一帧，写成没有文件头的 raw 文件。

这里会反复出现四个词：

* **Pool**：一组可重复使用的 DMA-BUF。输入 Pool 的 buffer 交给模块后，模块处理完才归还；输出 Pool 提供模块写结果的空 buffer。Pool 不够大或没有空闲 slot，通常会在 `CreateModule`、`SendFrame` 或 `GetFrame` 时报错。
* **端口（port）**：模块的插口。普通插件使用 `input0`、`input1`、`output0` 端口；MCF 使用 `MEDIA_MCF_PIPE_COLOR`、`MEDIA_MCF_PIPE_MONO` 和 chn=0，不能把 pipe 常量误写成字符串端口。
* **VIDEO_FRAME metadata**：跟在 buffer 上的描述信息，包括格式、可见宽高、每行 pitch、plane offset、字节数、序号和时间戳。模块依靠它判断 DMA-BUF 怎样解释，不能只填一块裸内存。
* **LINEAR/NV12**：本教程使用线性 NV12。NV12 的 Y 平面先放 `width × height` 个亮度字节，后面紧跟交错 UV 平面；每行可能为了硬件对齐而比 width 更长，所以查看文件必须使用程序打印的尺寸和格式。

程序中的处理顺序是：`MEDIA_SYS_Init` → 加载插件（校验 `.so.sig`）→ `MEDIA_VIDEO_BuildLinearLayout` → 创建输入/输出 Pool → 创建并启动模块 → 向两路输入送帧 → 从 `output0` 取帧 → 读取 metadata/像素 → Release → Stop/Destroy/Exit。多数插件通过 `MEDIA_PLUGIN_CreateModule`、`MEDIA_SYS_SendFrame`、`MEDIA_SYS_GetFrame` 完成；MCF、PANO、SVM3D 使用各自头文件中的 `Create/SendFrame/GetFrame` 专用 API，但生命周期和 buffer 所有权相同。阅读 `main.c` 时可以按这个顺序对照。

## 环境和第一次运行

请在发布包根目录执行命令。目标板应有 RK3588 的 Mali/EGL 驱动；没有 GPU 的普通 x86 机器只能完成编译，运行时会在 `Start` 或导入 DMA-BUF 时失败。插件文件和同名签名文件必须同时存在：例如 `plugins/libmedia_mcf_fusion.so` 与 `plugins/libmedia_mcf_fusion.so.sig`。

```sh
export LD_LIBRARY_PATH="$PWD/plugins:$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake -S examples/mcf_fusion -B build/mcf_fusion
cmake --build build/mcf_fusion -j2
(cd build/mcf_fusion && ./mcf_fusion_tutorial ...)
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

模块的完整 ABI、字段边界和限制请继续阅读对应的 [`docs/plugins/mcf_fusion.md`](../../docs/plugins/mcf_fusion.md) 与 `include/media_mcf_fusion.h`；本 README 只解释“怎样第一次跑通以及怎样观察参数差异”。

## 这个模块的参数（先看懂再修改）

| 参数 | 含义 | 本例/变化 |
|---|---|---|
| `path`（命令行第 1 项） | 处理路径 | 0=`FUSION`；1=`COLOR` 只直通彩色；2=`MONO` 只直通单色。 |
| `sync_pipe_en` | 是否按时间戳配对 | 本例 0（先到先配）；设 1 后会使用 `timestamp_tolerance_us`。 |
| `timestamp_tolerance_us` | 可接受时间差（微秒） | 本例 2000；实时 30fps 可从 2000 开始，过小会丢帧。 |
| `reuse_last_frame` | 缺一路时是否复用上一帧 | 1；设 0 会等待下一对完整输入。 |
| `local_window` | 局部融合窗口 | 本例 5；算法第一版要求 5。 |
| `local_detail_gain` | 单色细节注入增益 | 0.9；增大更锐利，也更容易放大噪声。 |
| `normalize_mode` | 单色亮度归一化 | 0=不归一化；1=均值/标准差归一化。 |


## 本程序的输入、端口和 Pool

MCF 有两个输入 pipe：`MEDIA_MCF_PIPE_COLOR`（彩色 input0）和 `MEDIA_MCF_PIPE_MONO`（单色 input1），输出通道是 chn=0。程序创建 COLOR/MONO/OUT 三个 Pool，各 4 个 slot，设置通道后 `MEDIA_MCF_StartGrp`，再分别调用 `MEDIA_MCF_SendPipeFrame`。每路输入都必须带 NV12 metadata；本例 mono 也用 NV12，只读取 Y，UV 填中性值。

## 输出和结果对比

```sh
(cd build/mcf_fusion && ./mcf_fusion_tutorial 0 fused.nv12)
(cd build/mcf_fusion && ./mcf_fusion_tutorial 1 color.nv12)
(cd build/mcf_fusion && ./mcf_fusion_tutorial 2 mono.nv12)
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 fused.nv12
```

三种 path 使用相同输入，便于理解“算法融合”和“直通”的差异。真实应用可通过 `MEDIA_MCF_GetLastPerf` 查看配对数、复用次数和 GPU 耗时。

## 从教程走向真实相机

本例使用合成图只是为了让每个人都能重复实验。接入真实设备时，把 `make_frame` 替换成 VI/解码器给出的 DMA-BUF，保持相同的 `MEDIA_VIDEO_LAYOUT` 和 `MEDIA_BUFFER_META_VIDEO_FRAME`；两路相机必须在同一时刻、同一分辨率、同一格式到达。推荐先用 `examples/vi_mipi` 验证摄像头，再通过 `MEDIA_SYS_Bind("VI", ..., "<模块 type>", ..., "input0")` 或本模块专用 `SendFrame` API 连接模块（具体 type 和端口以本模块文档为准）。不要在应用侧把 DMA-BUF 映射后复制成普通 malloc 指针，GPU/RGA 模块需要真正的 dma-buf fd。

如果启用时间戳配对，发送的 `m.time_stamp` 不能一直为 0；两路时钟必须来自同一时基。

## 下一步阅读

* 程序：[`main.c`](main.c)；构建文件：[`CMakeLists.txt`](CMakeLists.txt)。
* 公共 API：[`include/media_mcf_fusion.h`](../../include/media_mcf_fusion.h)。
* 完整模块约束：[`docs/plugins/mcf_fusion.md`](../../docs/plugins/mcf_fusion.md)。
