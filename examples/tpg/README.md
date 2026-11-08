# TPG 入门：生成第一个视频帧

TPG（Test Pattern Generator）是最适合学习 libmedia 的视频源。它不需要摄像头，直接生成颜色条、棋盘格、渐变、纯色或移动方块。你可以先用 TPG 验证 Pool、格式和下游模块，再把 TPG 换成真实 VI 摄像头。

## 1. 编译和最简单的运行

```sh
cmake -S examples/tpg -B build/tpg
cmake --build build/tpg -j2
(cd build/tpg && ./tpg_tutorial)
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 build/tpg/tpg_frame.nv12
```

命令行格式：

```text
./tpg_tutorial [输出文件] [宽] [高] [图案]
```

| 参数 | 默认值 | 说明 |
|---|---:|---|
| 输出文件 | `tpg_frame.nv12` | 裸 NV12 文件，Y plane 后接 UV plane |
| 宽 | 640 | 可见宽度，范围 1..8192 |
| 高 | 480 | 可见高度，范围 1..8192 |
| 图案 | 0 | 0 色条、1 棋盘、2 渐变、3 纯色、4 移动方块 |

例：

```sh
(cd build/tpg && ./tpg_tutorial gradient.nv12 1280 720 2)
ffplay -f rawvideo -pixel_format nv12 -video_size 1280x720 build/tpg/gradient.nv12
(cd build/tpg && ./tpg_tutorial box.nv12 640 480 4)
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 build/tpg/box.nv12
```

## 2. 为什么要 Pool 和 layout

NV12 是两个 plane：Y（每像素一个亮度值）和交错 UV（每 2×2 像素一组色度）。实际 DMA 内存常有 pitch/offset 对齐，不能只按 `width*height*3/2` 猜大小。程序调用 `MEDIA_VIDEO_BuildLinearLayout` 得到 canonical layout，并创建 4 个 `NATIVE` Pool 槽；TPG 每生成一帧就从池中取一个可复用 Buffer。

- `NATIVE`：可导出 DMA-BUF，适合连接 VPSS/RGA/VO 等硬件模块。
- `HOST`：CPU 内存，不能假设所有视频插件都能导入。
- `pool_id`：TPG 属性中的池编号，必须在 `CreateChn` 前已创建。

## 3. 代码生命周期

`MEDIA_SYS_Init → BuildLinearLayout → Pool_Create → TPG_CreateChn → TPG_Enable → GetFrame → CPU 读同步/写文件 → ReleaseFrame → Disable → Destroy → Pool_Destroy → MEDIA_SYS_Exit`。`GetFrame` 取得的 Buffer 必须 Release；否则连续取帧会把 4 个槽全部占住。

## 4. 把 TPG 接到别的模块

TPG 实例是 `TPG_0`，输出端口是 `output0`。例如连接 RGA：

```c
MEDIA_SYS_Bind("TPG", 0, "output0", "RGA", 0, "input0");
```

绑定后不要同时手动从 TPG 取同一帧；由下游模块 Get/Release。下游的输入格式、尺寸和 Pool 必须匹配 TPG 的 NV12 layout。

## 5. 常见错误

- `NV12 pool setup failed`：宽高过大、格式不支持或 Pool ID 已被占用。
- ffplay 画面倾斜：播放尺寸与实际参数不一致，或使用了错误像素格式。
- 下游导入失败：下游需要 NATIVE DMA-BUF；检查 `output_compress=NONE` 和 stride。

更多 pattern、格式和属性见 [`docs/plugins/tpg.md`](../../docs/plugins/tpg.md)、[`include/media_tpg.h`](../../include/media_tpg.h)。
