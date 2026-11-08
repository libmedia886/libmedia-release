# clahe 模块教程

这个目录用于学习和检查 `CLAHE` 模块：局部对比度增强。

## 模块参数

| 项目 | 值 |
| --- | --- |
| 插件 | `plugins/libmedia_clahe.so` |
| 签名 | `plugins/libmedia_clahe.so.sig` |
| 公共头 | [`include/media_clahe.h`](../../include/media_clahe.h) |
| type | `CLAHE` |
| 完整参数与限制 | [`docs/plugins/clahe.md`](../../docs/plugins/clahe.md) |

属性结构体、输入输出端口、格式、尺寸、Pool 要求和运行时参数，以公共头文件和模块文档为准。滤镜类模块需要上游帧、输出 Pool 以及与硬件格式匹配的配置，不能用一个空配置伪造通用帧；这个教程程序因此专门验证模块发布物本身。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/clahe -B build/clahe
cmake --build build/clahe -j2
(cd build/clahe && ./clahe_tutorial)
```

也可以指定插件的绝对路径：

```sh
./build/clahe/clahe_tutorial "$PWD/plugins/libmedia_clahe.so"
```

程序会初始化 `libmedia.so`，调用 `MEDIA_PLUGIN_Load()` 验证相邻 `.sig`，然后打印插件版本、路径、注册 class 数和 type。输出 `signed plugin smoke test passed` 表示签名、ABI、动态依赖和插件入口都通过。

## 接入实际管线

1. 先按上面的命令确认发布包和签名完整。
2. 阅读模块文档和公共头文件中的属性结构体、端口 API 与示例配置。
3. 用 `MEDIA_VIDEO_BuildLinearLayout()` 创建正确尺寸和格式的 Pool。
4. 通过 `MEDIA_SYS_Bind()` 连接输入输出，或使用模块头文件中的 `SendFrame/GetFrame` API。
5. 处理结束时按模块文档顺序 Disable、Destroy、Release Pool。
