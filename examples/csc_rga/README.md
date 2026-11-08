# csc_rga 模块教程

这个目录用于学习和检查 `CSC_RGA` 模块：RGA 色彩空间与像素格式转换。

## 模块参数

| 项目 | 值 |
| --- | --- |
| 插件 | `plugins/libmedia_csc_rga.so` |
| 签名 | `plugins/libmedia_csc_rga.so.sig` |
| 公共头 | [`include/media_csc_rga.h`](../../include/media_csc_rga.h) |
| type | `CSC_RGA` |
| 完整参数与限制 | [`docs/plugins/csc_rga.md`](../../docs/plugins/csc_rga.md) |

属性结构体、输入输出端口、格式、尺寸、Pool 要求和运行时参数，以公共头文件和模块文档为准。滤镜类模块需要上游帧、输出 Pool 以及与硬件格式匹配的配置，不能用一个空配置伪造通用帧；这个教程程序因此专门验证模块发布物本身。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/csc_rga -B build/csc_rga
cmake --build build/csc_rga -j2
(cd build/csc_rga && ./csc_rga_tutorial)
```

也可以指定插件的绝对路径：

```sh
./build/csc_rga/csc_rga_tutorial "$PWD/plugins/libmedia_csc_rga.so"
```

程序会初始化 `libmedia.so`，调用 `MEDIA_PLUGIN_Load()` 验证相邻 `.sig`，然后打印插件版本、路径、注册 class 数和 type。输出 `signed plugin smoke test passed` 表示签名、ABI、动态依赖和插件入口都通过。

## 接入实际管线

1. 先按上面的命令确认发布包和签名完整。
2. 阅读模块文档和公共头文件中的属性结构体、端口 API 与示例配置。
3. 用 `MEDIA_VIDEO_BuildLinearLayout()` 创建正确尺寸和格式的 Pool。
4. 通过 `MEDIA_SYS_Bind()` 连接输入输出，或使用模块头文件中的 `SendFrame/GetFrame` API。
5. 处理结束时按模块文档顺序 Disable、Destroy、Release Pool。
