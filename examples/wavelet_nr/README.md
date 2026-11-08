# wavelet_nr 模块教程

这个例子不是只检查插件文件，而是运行一条完整的一帧管线：`TPG -> WAVELET_NR_CL -> GetFrame`。程序创建真实的 DMA-BUF 输入/输出 Pool，处理成功后把输出 raw buffer 保存到文件。

## 本例做什么

- 输入：TPG 生成的 640x360 NV12 彩条。
- 模块：`WAVELET_NR_CL`，参数通过命令行改变 `levels`。
- 输出：`wavelet_nr.nv12`（无文件头；NV12 按 Y 平面后接 UV 平面，RGB 按 packed 字节保存）。

第二个参数 levels（1 或 2，默认 1）。2 会再做一级低频抑制；程序同时打开 UV 平滑（uv_strength=0.5）。

## 编译和运行

```sh
cmake -S examples/wavelet_nr -B build/wavelet_nr
cmake --build build/wavelet_nr -j2
LD_LIBRARY_PATH=lib:plugins ./build/wavelet_nr/wavelet_nr_tutorial
```

也可以指定输出文件：

```sh
./build/wavelet_nr/wavelet_nr_tutorial result.nv12 [参数]
```

示例：

```sh
./build/wavelet_nr/wavelet_nr_tutorial low.nv12 2
```

程序通过 `MEDIA_PLUGIN_Load` 验证 `.so.sig`，再调用公开的 `MEDIA_PLUGIN_CreateModule/Start`、`MEDIA_SYS_Bind`、`MEDIA_PLUGIN_GetFrame/ReleaseFrame` API。插件依赖 GLES/RGA 的设备能力，必须在目标板上运行；如果驱动不支持相应格式，启动或取帧会返回错误码。

## 参数要点

完整字段、端口、Pool 容量和格式限制见 [`include/media_wavelet_nr.h`](../../include/media_wavelet_nr.h) 与 [`docs/plugins/wavelet_nr.md`](../../docs/plugins/wavelet_nr.md)。本例的输入输出 Pool 分别为 20 和 21，使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，模块销毁后由程序清理。
