# TPG 测试图案源教程

这个示例创建 `TPG` SOURCE，生成一帧 NV12 测试图案并保存成原始文件。可以改变尺寸和图案，直接比较输出差异。

## 编译和运行

```sh
cmake -S examples/tpg -B build/tpg
cmake --build build/tpg -j2
(cd build/tpg && ./tpg_tutorial [输出文件] [宽] [高] [图案])
```

默认输出 `build/tpg/tpg_frame.nv12`。可以用 ffplay 查看：

```sh
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 build/tpg/tpg_frame.nv12
```

图案参数：`0` 色条、`1` 棋盘格、`2` 渐变、`3` 纯色、`4` 移动方块。例如：

```sh
(cd build/tpg && ./tpg_tutorial gradient.nv12 1280 720 2)
ffplay -f rawvideo -pixel_format nv12 -video_size 1280x720 build/tpg/gradient.nv12
```

## 参数和流程

程序使用 `MEDIA_TPG_ATTR`：`width=640`、`height=480`、`format=MEDIA_FORMAT_NV12`、`fps=30`、`pattern=MEDIA_TPG_PATTERN_COLOR_BARS`，并为 4 个 native/CMA Pool slot 创建足够的 NV12 buffer。

流程是 `MEDIA_SYS_Init` → `MEDIA_VIDEO_BuildLinearLayout` → `MEDIA_POOL_CreateWithMode` → `MEDIA_TPG_CreateChn` → `MEDIA_TPG_Enable` → `MEDIA_TPG_GetFrame` → CPU 读同步并写文件 → Release/Disable/Destroy。

更多图案、格式和属性见 [`include/media_tpg.h`](../../include/media_tpg.h) 与 [`docs/plugins/tpg.md`](../../docs/plugins/tpg.md)。
