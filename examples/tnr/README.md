# tnr 模块教程

这个例子不是只检查插件文件，而是运行一条完整的一帧管线：`TPG -> TNR_CL -> GetFrame`。程序创建真实的 DMA-BUF 输入/输出 Pool，处理成功后把输出 raw buffer 保存到文件。

## 本例做什么

- 输入：TPG 生成的 640x360 NV12 彩条。
- 模块：`TNR_CL`，参数通过命令行改变 `threshold`。
- 输出：`tnr.nv12.00`、`tnr.nv12.01`（程序取两帧以建立时域历史；无文件头，NV12 按 Y 平面后接 UV 平面保存）。若指定输出前缀 `result.nv12`，实际文件名为 `result.nv12.00` 和 `result.nv12.01`。

第二个参数 threshold（默认 0.12）。程序取两帧以建立历史；阈值增大时更多像素被判为运动。static_alpha=0.35、motion_alpha=0.88。

## 编译和运行

```sh
cmake -S examples/tnr -B build/tnr
cmake --build build/tnr -j2
LD_LIBRARY_PATH=lib:plugins ./build/tnr/tnr_tutorial
```

也可以指定输出文件：

```sh
./build/tnr/tnr_tutorial result.nv12 [参数]
```

示例：

```sh
./build/tnr/tnr_tutorial low.nv12 1
```

程序通过 `MEDIA_PLUGIN_Load` 验证 `.so.sig`，再调用公开的 `MEDIA_PLUGIN_CreateModule/Start`、`MEDIA_SYS_Bind`、`MEDIA_PLUGIN_GetFrame/ReleaseFrame` API。插件依赖 GLES/RGA 的设备能力，必须在目标板上运行；如果驱动不支持相应格式，启动或取帧会返回错误码。

## 参数要点

完整字段、端口、Pool 容量和格式限制见 [`include/media_tnr.h`](../../include/media_tnr.h) 与 [`docs/plugins/tnr.md`](../../docs/plugins/tnr.md)。本例的输入输出 Pool 分别为 20 和 21，使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，模块销毁后由程序清理。
