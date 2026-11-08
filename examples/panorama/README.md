# panorama：新手实帧教程

`PANO`（全景拼接）把 2~8 路相机投影到一张全景画布。它和普通左右拼接最大的区别是：每个输出像素要通过标定文件决定“来自哪一个相机的哪个坐标”。因此没有 PTO/LUT 时不能凭空得到正确环视几何；本教程允许编译，但只有提供真实标定资产才应期待成功。


## 先理解这段教程在做什么

把一个视频模块想成“有输入插口和输出插口的黑盒子”。本教程为了不依赖摄像头，程序在内存中生成两张 **640×480** 的测试图：`input0` 是从左到右变亮，`input1` 是从上到下变亮。两张图通过 DMA-BUF 放进模块，模块处理一次后从 `output0` 取回一帧，写成没有文件头的 raw 文件。

这里会反复出现四个词：

* **Pool**：一组可重复使用的 DMA-BUF。输入 Pool 的 buffer 交给模块后，模块处理完才归还；输出 Pool 提供模块写结果的空 buffer。Pool 不够大或没有空闲 slot，通常会在 `CreateModule`、`SendFrame` 或 `GetFrame` 时报错。
* **端口（port）**：模块的插口。`input0`、`input1` 是两路输入，`output0` 是输出。手动送帧时端口名必须完全匹配；把 `input1` 写成 `input01` 会失败。
* **VIDEO_FRAME metadata**：跟在 buffer 上的描述信息，包括格式、可见宽高、每行 pitch、plane offset、字节数、序号和时间戳。模块依靠它判断 DMA-BUF 怎样解释，不能只填一块裸内存。
* **LINEAR/NV12**：本教程使用线性 NV12。NV12 的 Y 平面先放 `width × height` 个亮度字节，后面紧跟交错 UV 平面；每行可能为了硬件对齐而比 width 更长，所以查看文件必须使用程序打印的尺寸和格式。

程序中的处理顺序是：`MEDIA_SYS_Init` → 加载插件（校验 `.so.sig`）→ `MEDIA_VIDEO_BuildLinearLayout` → 创建输入/输出 Pool → 创建并启动模块 → 向两路输入送帧 → 从 `output0` 取帧 → 读取 metadata/像素 → Release → Stop/Destroy/Exit。多数插件通过 `MEDIA_PLUGIN_CreateModule`、`MEDIA_SYS_SendFrame`、`MEDIA_SYS_GetFrame` 完成；MCF、PANO、SVM3D 使用各自头文件中的 `Create/SendFrame/GetFrame` 专用 API，但生命周期和 buffer 所有权相同。阅读 `main.c` 时可以按这个顺序对照。

## 环境和第一次运行

请在发布包根目录执行命令。目标板应有 RK3588 的 Mali/EGL 驱动；没有 GPU 的普通 x86 机器只能完成编译，运行时会在 `Start` 或导入 DMA-BUF 时失败。插件文件和同名签名文件必须同时存在：例如 `plugins/libmedia_panorama.so` 与 `plugins/libmedia_panorama.so.sig`。

```sh
export LD_LIBRARY_PATH="$PWD/plugins:$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake -S examples/panorama -B build/panorama
cmake --build build/panorama -j2
(cd build/panorama && ./panorama_tutorial ...)
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

模块的完整 ABI、字段边界和限制请继续阅读对应的 [`docs/plugins/panorama.md`](../../docs/plugins/panorama.md) 与 `include/media_panorama.h`；本 README 只解释“怎样第一次跑通以及怎样观察参数差异”。

## 这个模块的参数（先看懂再修改）

| 参数/字段 | 含义 | 初学者要点 |
|---|---|---|
| `input_count` | 相机数量 | 本程序为 2；真实环视通常 4 或 6。 |
| `in_width/in_height/in_stride` | 每路原图布局 | 所有输入必须与标定时尺寸一致；不能只改程序里的数字。 |
| `out_width/out_height/out_stride` | 全景画布 | 必须与输出 Pool 和 LUT 设计一致。 |
| `pto_path`（命令行第 3 项） | PTO/PTS 标定文件 | 包含镜头内参、姿态和拼接关系；没有它通常 `CreateModule` 失败。 |
| `lut_width/lut_height` | 预计算映射尺寸 | 0 表示使用输出尺寸；若 PTO 生成 LUT，必须匹配。 |
| `crop_*` | 全景域裁剪 | 只在标定文件有效域内裁剪，越界会得到空白。 |
| `sync_timeout_ms/sync_wait_inputs` | 多路同步策略 | 本例等待两路；实时系统需按相机帧率调节。 |


## 本程序的输入、端口和 Pool

端口是 `input0..inputN-1` 与 `output0`，格式为 NV12 LINEAR。程序虽然生成两路渐变并正确填写 Pool/layout/metadata，但 `pto_path` 默认 NULL；这只用于检查 API 和错误处理。提供 PTO 后，模块按 PTO 把两路送入同一画布。输出 Pool 29 的容量必须覆盖全景输出尺寸。

## 输出和结果对比

不带资产的试跑：

```sh
(cd build/panorama && ./panorama_tutorial test.nv12)
```

带真实 PTO：

```sh
(cd build/panorama && ./panorama_tutorial pano.nv12 /绝对路径/plugins/libmedia_panorama.so /data/calib/car_2cam.pto)
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 pano.nv12
```

如果程序在 `CreateModule` 返回错误，优先检查 PTO 路径、文件版本、输入分辨率和 LUT 尺寸；不要用空文件“骗过”标定检查。

## 从教程走向真实相机

本例使用合成图只是为了让每个人都能重复实验。接入真实设备时，把 `make_frame` 替换成 VI/解码器给出的 DMA-BUF，保持相同的 `MEDIA_VIDEO_LAYOUT` 和 `MEDIA_BUFFER_META_VIDEO_FRAME`；两路相机必须在同一时刻、同一分辨率、同一格式到达。推荐先用 `examples/vi_mipi` 验证摄像头，再通过 `MEDIA_SYS_Bind("VI", ..., "<模块 type>", ..., "input0")` 或本模块专用 `SendFrame` API 连接模块（具体 type 和端口以本模块文档为准）。不要在应用侧把 DMA-BUF 映射后复制成普通 malloc 指针，GPU/RGA 模块需要真正的 dma-buf fd。

当前仓库没有通用 PTO 样例，因为标定文件与镜头安装位置一一对应。请用相机厂商/标定工具生成后再测试。

## 下一步阅读

* 程序：[`main.c`](main.c)；构建文件：[`CMakeLists.txt`](CMakeLists.txt)。
* 公共 API：[`include/media_panorama.h`](../../include/media_panorama.h)。
* 完整模块约束：[`docs/plugins/panorama.md`](../../docs/plugins/panorama.md)。
