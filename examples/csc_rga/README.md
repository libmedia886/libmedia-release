# csc_rga 模块教程

这个例子不是只检查插件文件，而是运行一条完整的一帧管线：`TPG -> CSC_RGA -> GetFrame`。程序创建真实的 DMA-BUF 输入/输出 Pool，处理成功后把输出 raw buffer 保存到文件。

## 本例做什么

- 输入：TPG 生成的 640x360 RGB888 彩条。
- 模块：`CSC_RGA`，参数通过命令行改变 `csc_mode`。
- 输出：`csc_rga.rgb`（无文件头；NV12 按 Y 平面后接 UV 平面，RGB 按 packed 字节保存）。

第二个参数 csc_mode（默认 1 BT601_LIMIT）。可选 0 DEFAULT、1 BT601_LIMIT、2 BT601_FULL、3 BT709_LIMIT、4 BT709_FULL；输出固定 RGB888。

## 编译和运行

```sh
cmake -S examples/csc_rga -B build/csc_rga
cmake --build build/csc_rga -j2
LD_LIBRARY_PATH=lib:plugins ./build/csc_rga/csc_rga_tutorial
```

也可以指定输出文件：

```sh
./build/csc_rga/csc_rga_tutorial result.rgb [参数]
```

示例：

```sh
./build/csc_rga/csc_rga_tutorial low.rgb 1
```

程序通过 `MEDIA_PLUGIN_Load` 验证 `.so.sig`，再调用公开的 `MEDIA_PLUGIN_CreateModule/Start`、`MEDIA_SYS_Bind`、`MEDIA_PLUGIN_GetFrame/ReleaseFrame` API。插件依赖 GLES/RGA 的设备能力，必须在目标板上运行；如果驱动不支持相应格式，启动或取帧会返回错误码。

## 参数要点

完整字段、端口、Pool 容量和格式限制见 [`include/media_csc_rga.h`](../../include/media_csc_rga.h) 与 [`docs/plugins/csc_rga.md`](../../docs/plugins/csc_rga.md)。本例的输入输出 Pool 分别为 20 和 21，使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，模块销毁后由程序清理。
