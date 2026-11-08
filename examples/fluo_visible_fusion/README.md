# fluo_visible_fusion：新手实帧教程

可见光+荧光融合把 `input0` 当作普通可见光，把 `input1` 的亮度当作荧光信号，按阈值生成 mask，再以伪彩或灰度方式叠加。荧光输入的 UV 不参与计算，因此用灰度荧光相机也可以。


## 先理解这段教程在做什么

把一个视频模块想成“有输入插口和输出插口的黑盒子”。本教程为了不依赖摄像头，程序在内存中生成两张 **640×480** 的测试图：`input0` 是从左到右变亮，`input1` 是从上到下变亮。两张图通过 DMA-BUF 放进模块，模块处理一次后从 `output0` 取回一帧，写成没有文件头的 raw 文件。

这里会反复出现四个词：

* **Pool**：一组可重复使用的 DMA-BUF。输入 Pool 的 buffer 交给模块后，模块处理完才归还；输出 Pool 提供模块写结果的空 buffer。Pool 不够大或没有空闲 slot，通常会在 `CreateModule`、`SendFrame` 或 `GetFrame` 时报错。
* **端口（port）**：模块的插口。`input0`、`input1` 是两路输入，`output0` 是输出。手动送帧时端口名必须完全匹配；把 `input1` 写成 `input01` 会失败。
* **VIDEO_FRAME metadata**：跟在 buffer 上的描述信息，包括格式、可见宽高、每行 pitch、plane offset、字节数、序号和时间戳。模块依靠它判断 DMA-BUF 怎样解释，不能只填一块裸内存。
* **LINEAR/NV12**：本教程使用线性 NV12。NV12 的 Y 平面先放 `width × height` 个亮度字节，后面紧跟交错 UV 平面；每行可能为了硬件对齐而比 width 更长，所以查看文件必须使用程序打印的尺寸和格式。

程序中的处理顺序是：`MEDIA_SYS_Init` → 加载插件（校验 `.so.sig`）→ `MEDIA_VIDEO_BuildLinearLayout` → 创建输入/输出 Pool → 创建并启动模块 → 向两路输入送帧 → 从 `output0` 取帧 → 读取 metadata/像素 → Release → Stop/Destroy/Exit。多数插件通过 `MEDIA_PLUGIN_CreateModule`、`MEDIA_SYS_SendFrame`、`MEDIA_SYS_GetFrame` 完成；MCF、PANO、SVM3D 使用各自头文件中的 `Create/SendFrame/GetFrame` 专用 API，但生命周期和 buffer 所有权相同。阅读 `main.c` 时可以按这个顺序对照。

## 环境和第一次运行

请在发布包根目录执行命令。目标板应有 RK3588 的 Mali/EGL 驱动；没有 GPU 的普通 x86 机器只能完成编译，运行时会在 `Start` 或导入 DMA-BUF 时失败。插件文件和同名签名文件必须同时存在：例如 `plugins/libmedia_fluo_visible_fusion.so` 与 `plugins/libmedia_fluo_visible_fusion.so.sig`。

```sh
export LD_LIBRARY_PATH="$PWD/plugins:$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake -S examples/fluo_visible_fusion -B build/fluo_visible_fusion
cmake --build build/fluo_visible_fusion -j2
(cd build/fluo_visible_fusion && ./fluo_visible_fusion_tutorial ...)
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

模块的完整 ABI、字段边界和限制请继续阅读对应的 [`docs/plugins/fluo_visible_fusion.md`](../../docs/plugins/fluo_visible_fusion.md) 与 `include/media_fluo_visible_fusion.h`；本 README 只解释“怎样第一次跑通以及怎样观察参数差异”。

## 这个模块的参数（先看懂再修改）

| 参数 | 含义 | mode/建议 |
|---|---|---|
| `method` | 伪彩策略 | 0 绿色、1 青色、2 热红、3 亮度平均灰、4 拉普拉斯细节灰。mode 0/1 的命令行值分别测试绿色与青色。 |
| `thr_lo/thr_hi` | 荧光归一化亮度的平滑阈值 | 默认 0.20/0.65；增大 `thr_lo` 可抑制背景荧光，减小 `thr_hi` 会更早显示颜色。 |
| `overlay_alpha` | 最大荧光处的覆盖透明度 | 0.72；1.0 完全覆盖，0.3 更保留可见光。 |
| `y_boost` | 荧光 Y 增益 | 1.15；过大容易饱和。 |


## 本程序的输入、端口和 Pool

端口固定为 `input0=visible`、`input1=fluorescence`、`output0=result`，全部 NV12 LINEAR。两路必须相同尺寸/stride；输出 Pool 29 由属性 `output_pool_id` 指定。程序用横向/纵向渐变模拟两种传感器，送帧时先 input1 再 input0。

## 输出和结果对比

```sh
(cd build/fluo_visible_fusion && ./fluo_visible_fusion_tutorial 0 green.nv12)
(cd build/fluo_visible_fusion && ./fluo_visible_fusion_tutorial 1 cyan.nv12)
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 green.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 cyan.nv12
```

真实荧光图通常需要先做暗电流/背景校正；阈值是归一化 Y（0~1），不是 0~255 的整数。

## 从教程走向真实相机

本例使用合成图只是为了让每个人都能重复实验。接入真实设备时，把 `make_frame` 替换成 VI/解码器给出的 DMA-BUF，保持相同的 `MEDIA_VIDEO_LAYOUT` 和 `MEDIA_BUFFER_META_VIDEO_FRAME`；两路相机必须在同一时刻、同一分辨率、同一格式到达。推荐先用 `examples/vi_mipi` 验证摄像头，再通过 `MEDIA_SYS_Bind("VI", ..., "<模块 type>", ..., "input0")` 或本模块专用 `SendFrame` API 连接模块（具体 type 和端口以本模块文档为准）。不要在应用侧把 DMA-BUF 映射后复制成普通 malloc 指针，GPU/RGA 模块需要真正的 dma-buf fd。



## 下一步阅读

* 程序：[`main.c`](main.c)；构建文件：[`CMakeLists.txt`](CMakeLists.txt)。
* 公共 API：[`include/media_fluo_visible_fusion.h`](../../include/media_fluo_visible_fusion.h)。
* 完整模块约束：[`docs/plugins/fluo_visible_fusion.md`](../../docs/plugins/fluo_visible_fusion.md)。
