# Load All：批量检查插件包

这个例子扫描 `plugins/` 目录，逐个调用 `MEDIA_PLUGIN_Load`，用于在交付前快速检查：插件文件是否存在、相邻签名是否匹配、动态依赖是否可加载。它**不创建模块、不采集、不处理视频**，所以通过后仍需运行具体教程验证硬件功能。

## 1. 编译和运行

```sh
cmake -S examples/load_all -B build/load_all
cmake --build build/load_all -j2
(cd build/load_all && ./libmedia_load_all)
```

程序依赖当前目录下的 `../../plugins`。从仓库根目录直接执行二进制会导致相对路径不对；如果需要从任意目录运行，请修改 `main.c` 使用绝对插件目录。

## 2. 输出怎么读

程序会为每个 `.so` 调用加载接口，并在最后打印：

```text
loaded 32 signed plugins
```

当前核心的单进程加载上限是 `MEDIA_PLUGIN_MAX_LOADED=32`，而发布包可能有 37 个插件。因此看到 32 并不表示少了 5 个文件：前 32 个能加载即可，实际应用通常只加载所需插件。目录中的 `.so.sig` 不会被当成插件扫描，因为它不是以 `.so` 结尾。

## 3. 检查失败的步骤

1. 确认文件成对存在：`ls plugins/*.so plugins/*.so.sig`。
2. 对失败插件检查依赖：`ldd plugins/libmedia_xxx.so`。
3. 检查签名：不要混用不同版本发布包中的 `.so` 和 `.so.sig`。
4. 确认架构：`file plugins/libmedia_xxx.so` 应与目标板（通常 aarch64）一致。
5. 设置运行时搜索路径：`LD_LIBRARY_PATH=lib:plugins`。

如果只有某个硬件插件失败，仍可先使用其他无关教程；例如没有 MPP 时 VENC/VDEC 失败，不影响 TPG 的 CPU/GLES 教程。

## 4. 与真实管线的关系

加载成功后，应用还需要：

- 用对应头文件创建模块（如 `MEDIA_TPG_CreateChn`）。
- 为视频模块创建正确格式和 Pool（通常 `NATIVE` DMA-BUF）。
- 连接固定端口（例如 TPG `output0`、VPSS `input0/output0..15`）。
- 按 `Create → Start/Enable → Get/Send → Release → Stop → Destroy` 的生命周期清理。

具体参数和端口请进入相应目录的 README；模块 API 细节见 [`docs/plugins`](../../docs/plugins/)。
