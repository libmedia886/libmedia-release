# tnr 模块入门教程

> 目标：即使第一次接触 libmedia，也能照着本页编译、运行、看懂输出，并知道下一步该改哪个参数。

## 1. 这个例子解决什么问题

TNR_CL（Temporal Noise Reduction）比较相邻帧：静止区域多使用历史帧来降噪，运动区域更多使用当前帧以避免拖影。程序特意让 TPG 生成移动方块，并取两帧；第一帧建立历史，第二帧才有可比较的结果。

**本教程实际管线：** `TPG (测试图案源) → TNR_CL → GetFrame → raw 文件`。
TPG 不需要摄像头或视频文件，便于先学习模块 API；它生成彩条（TNR 例外：生成移动方块以显示时域变化）。算法只处理一帧或指定帧数，不代表生产环境吞吐量。

- 输入/输出：NV12、640×360，每帧理论 345,600 bytes。
- 程序会生成两个文件：`tnr.nv12.00` 和 `tnr.nv12.01`；两帧 metadata 序号不同。
- 端口仍为 `input0` → `output0`；历史帧由模块内部 retain，不需要应用复制。



## 2. 参数速查

| 位置 | 字段 | 范围/默认 | 作用 |
|---|---|---|---|
| 第 1 个 | 输出前缀 | `tnr.nv12` | 程序自动追加 `.00`、`.01` |
| 第 2 个 | `threshold` | 默认 0.12；建议 0..1 | 判断运动的差异阈值。小值更敏感、降噪弱但拖影少；大值把更多区域视为运动，拖影少但静止区噪声保留更多 |

程序固定 `block_size=16`、`static_alpha=0.35`（静止区历史权重）、`motion_alpha=0.88`（运动区当前帧权重）。阈值大于 1 时插件按 8 位码值除以 255 解释；初学者先使用 0.05/0.12/0.30。

```sh
LD_LIBRARY_PATH=lib:plugins ./build/tnr/tnr_tutorial tnr005.nv12 0.05
LD_LIBRARY_PATH=lib:plugins ./build/tnr/tnr_tutorial tnr012.nv12 0.12
LD_LIBRARY_PATH=lib:plugins ./build/tnr/tnr_tutorial tnr030.nv12 0.30
ffplay -f rawvideo -pixel_format nv12 -video_size 640x360 tnr012.nv12.01
```
比较 `.00` 与 `.01`，再比较不同 threshold 的 `.01`；只看第一帧无法判断时域效果。

## 先建立三个概念

1. **帧（frame）**：一张图像。`width`、`height` 是可见像素数；本教程固定为 640×360，程序每次运行处理 1 帧（时域降噪 TNR 处理 2 帧）。
2. **Pool**：一组可重复使用的 DMA-BUF 图像内存。程序创建输入 Pool **20**、输出 Pool **21**，每个 8 个 slot，分配模式是 `MEDIA_POOL_ALLOC_MODE_NATIVE`。native Pool 才能被板上的 GLES/RGA 导入；不要把普通 `malloc` 指针当作视频帧。
3. **端口和 metadata**：TPG 的 `output0` 绑定到算法的 `input0`，算法结果从 `output0` 取出。每个 Buffer 都带 `MEDIA_BUFFER_META_VIDEO_FRAME`：格式、可见宽高、每个 plane 的 pitch/offset、有效字节数 `len`、时间戳和序号。算法会先检查 metadata，再处理，因此尺寸或格式写错会直接报错。

本例使用 `examples/common/filter_runner.h` 完成上述生命周期：初始化系统 → 建 Pool → 加载 `.so` 和相邻 `.so.sig` → 创建 TPG 和算法 → `MEDIA_SYS_Bind` → Start → `GetFrame` → 保存文件 → Release/Stop/Destroy。你以后把 TPG 换成 VI、解码器或自己的发送端时，仍要提供同样的 metadata 和可导入的 Pool。

## 从零开始编译

在仓库根目录执行（设备上应已有 `lib/`、`plugins/`）：

```sh
cmake -S examples/tnr -B build/tnr
cmake --build build/tnr -j2
LD_LIBRARY_PATH=lib:plugins ./build/tnr/tnr_tutorial
```

`cmake -S` 读取本目录的 `CMakeLists.txt`；`cmake --build` 只编译一个很小的教程程序，不会重新编译插件。`LD_LIBRARY_PATH` 告诉动态链接器去哪里找 `libmedia.so` 和插件依赖。若库不在仓库根目录，请把两个路径改成实际绝对路径。

逐行理解：

1. `cmake -S ... -B ...` 读取教程的 CMake 文件并生成构建目录；第一次运行会检查头文件、`libmedia.so` 和目标插件。
2. `cmake --build ... -j2` 编译 `main.c`，`-j2` 表示最多同时使用两个编译任务。
3. `LD_LIBRARY_PATH=... ./...` 先设置运行时库搜索路径，再启动程序；程序退出码为 0 才表示拿到并保存了帧。

程序的第一个参数是输出文件，第二个（以及 resize 的第三个）是算法参数。输出是**无文件头 raw**，文件名可以任意，但扩展名建议按格式命名。

## 如何查看 raw 输出

raw 文件没有宽高信息，查看时必须手动给出正确的像素格式和尺寸。NV12 是 Y 平面后接交错 UV 平面，理论字节数 `width × height × 3 / 2`；RGB888 是每像素 R、G、B 三字节，字节数 `width × height × 3`。如果 SDK 为 pitch 做了对齐，实际 `len` 可能略大于理论值，请以程序打印的 `saved N bytes` 为准。

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 640x360 tnr.nv12.01
```

按 `Ctrl-C` 退出 ffplay。若看到斜条、花屏或颜色错乱，优先检查 `-pixel_format`、`-video_size` 和输出文件是否对应本次参数。

## 常见错误排查

- `MEDIA_PLUGIN_Load failed`、找不到 `.so`：确认 `plugins/libmedia_tnr.so` 与同名 `.so.sig` 都存在，且 `LD_LIBRARY_PATH=lib:plugins` 写在**同一条运行命令前**。
- `signature`/验签错误：插件或 `.sig` 被改过；从同一版本发布包恢复两者，不能只复制 `.so`。
- `CreateModule/Start failed` 或 `MEDIA_ERR_UNSUPPORTED`：当前板子的 GLES/RGA 扩展或像素格式不支持。先运行 `examples/tpg` 验证 TPG，再换 NV12/640×360 等保守参数。
- `GetFrame timeout`：模块没有成功 Start、Bind 失败，或 GPU 驱动不可用。查看终端中紧邻的错误码；确认输入输出 Pool 都是 native DMA-BUF。
- 输出文件大小为 0：程序没有拿到帧就退出；不要用失败运行留下的旧文件判断结果。
- ffplay 无法打开：raw 文件不是损坏容器，补全正确的像素格式和尺寸；必要时用 `stat -c %s 文件` 对照上面的字节数。

更完整的字段约束、动态调参 API 和硬件限制，请继续阅读对应的 [`docs/plugins/tnr.md`](../../docs/plugins/tnr.md) 以及头文件。
