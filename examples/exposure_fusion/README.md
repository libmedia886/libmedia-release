# exposure_fusion 实帧教程

两幅程序生成的测试图经过 `EXPOSURE_FUSION_CL` 模块处理，保存原始输出帧。第一幅是横向亮度渐变，第二幅是纵向亮度渐变；输入是真实 DMA-BUF 帧，带 VIDEO_FRAME metadata。程序完成创建 Pool、加载签名插件、创建模块、启停、送帧、取帧和归还。处理依赖 RK3588 的 GPU 或 RGA 驱动。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/exposure_fusion -B build/exposure_fusion
cmake --build build/exposure_fusion -j2
(cd build/exposure_fusion && ./exposure_fusion_tutorial 0)
(cd build/exposure_fusion && ./exposure_fusion_tutorial 1 second.nv12)
```

参数一是模式整数；参数二是输出路径，参数三可选指定插件绝对路径。输入尺寸固定 640×480，格式 `NV12`；输出尺寸由该模块的布局决定。输出是无文件头原始像素，实际尺寸、字节数由程序打印。`ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 <file>` 可查看默认模式结果。

## 参数差异

mode 0 contrast_power=1; mode 1 contrast_power=2。两组输出可以逐字节比较，观察参数改变的实际效果。

公共配置在 [media_exposure_fusion.h](../../include/media_exposure_fusion.h)，完整限制见 [模块文档](../../docs/plugins/exposure_fusion.md)。示例中的测试图只用于说明处理管线；真实场景应替换为同步采集的两路传感器帧。
