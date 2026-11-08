# stereo_3d 实帧教程

两幅程序生成的测试图经过 `STEREO_3D` 模块处理，保存原始输出帧。第一幅是横向亮度渐变，第二幅是纵向亮度渐变；输入是真实 DMA-BUF 帧，带 VIDEO_FRAME metadata。程序完成创建 Pool、加载签名插件、创建模块、启停、送帧、取帧和归还。处理依赖 RK3588 的 GPU 或 RGA 驱动。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/stereo_3d -B build/stereo_3d
cmake --build build/stereo_3d -j2
(cd build/stereo_3d && ./stereo_3d_tutorial 0)
(cd build/stereo_3d && ./stereo_3d_tutorial 1 second.nv12)
```

参数一是模式整数；参数二是输出路径，参数三可选指定插件绝对路径。输入尺寸固定 640×480，格式 `NV12`；输出尺寸由该模块的布局决定。输出是无文件头原始像素，实际尺寸、字节数由程序打印。`ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 <file>` 可查看默认模式结果。

## 参数差异

mode 0 side by side; mode 1 alternates rows; input1 rotates 3.5 degrees。两组输出可以逐字节比较，观察参数改变的实际效果。

公共配置在 [media_stereo_3d.h](../../include/media_stereo_3d.h)，完整限制见 [模块文档](../../docs/plugins/stereo_3d.md)。示例中的测试图只用于说明处理管线；真实场景应替换为同步采集的两路传感器帧。
