# resize_rga 模块教程

这个例子不是只检查插件文件，而是运行一条完整的一帧管线：`TPG -> RESIZE_RGA -> GetFrame`。程序创建真实的 DMA-BUF 输入/输出 Pool，处理成功后把输出 raw buffer 保存到文件。

## 本例做什么

- 输入：TPG 生成的 640x360 NV12 彩条。
- 模块：`RESIZE_RGA`，参数通过命令行改变 `out_width out_height`。
- 输出：`resize_rga.nv12`（无文件头；NV12 按 Y 平面后接 UV 平面，RGB 按 packed 字节保存）。

第二、三个参数输出宽高（默认 320 180）。改为 640 360 可观察等比缩放和留边；输入固定 640x360 NV12。

## 编译和运行

```sh
cmake -S examples/resize_rga -B build/resize_rga
cmake --build build/resize_rga -j2
LD_LIBRARY_PATH=lib:plugins ./build/resize_rga/resize_rga_tutorial
```

也可以指定输出文件：

```sh
./build/resize_rga/resize_rga_tutorial result.nv12 [参数]
```

示例：

```sh
./build/resize_rga/resize_rga_tutorial low.nv12 640 360
```

程序通过 `MEDIA_PLUGIN_Load` 验证 `.so.sig`，再调用公开的 `MEDIA_PLUGIN_CreateModule/Start`、`MEDIA_SYS_Bind`、`MEDIA_PLUGIN_GetFrame/ReleaseFrame` API。插件依赖 GLES/RGA 的设备能力，必须在目标板上运行；如果驱动不支持相应格式，启动或取帧会返回错误码。

## 参数要点

完整字段、端口、Pool 容量和格式限制见 [`include/media_resize_rga.h`](../../include/media_resize_rga.h) 与 [`docs/plugins/resize_rga.md`](../../docs/plugins/resize_rga.md)。本例的输入输出 Pool 分别为 20 和 21，使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，模块销毁后由程序清理。
