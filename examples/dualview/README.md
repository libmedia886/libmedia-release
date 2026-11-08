# dualview 实帧教程

两幅程序生成的测试图经过 `DUALVIEW` 模块处理，保存原始输出帧。第一幅是横向亮度渐变，第二幅是纵向亮度渐变；输入是真实 DMA-BUF 帧，带 VIDEO_FRAME metadata。程序完成创建 Pool、加载签名插件、创建模块、启停、送帧、取帧和归还。处理依赖 RK3588 的 GPU 或 RGA 驱动。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/dualview -B build/dualview
cmake --build build/dualview -j2
(cd build/dualview && ./dualview_tutorial 0)
(cd build/dualview && ./dualview_tutorial 1 second.rgb)
```

参数一是模式整数；参数二是输出路径，参数三可选指定插件绝对路径。输入尺寸固定 640×480，格式 `RGB888`；输出尺寸由该模块的布局决定。输出是无文件头原始像素，实际尺寸、字节数由程序打印。`ffplay -f rawvideo -pixel_format rgb24 -video_size 1280x480 <file>` 可查看默认模式结果。

## 参数差异

mode 0 side by side; mode 1 alternates source on each row。两组输出可以逐字节比较，观察参数改变的实际效果。第二组模式为隔行，输出尺寸 640×480，查看时改用该尺寸。

公共配置在 [media_dualview.h](../../include/media_dualview.h)，完整限制见 [模块文档](../../docs/plugins/dualview.md)。示例中的测试图只用于说明处理管线；真实场景应替换为同步采集的两路传感器帧。
