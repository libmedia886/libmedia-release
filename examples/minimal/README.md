# Minimal：第一次加载 libmedia 插件

这个例子只做三件事：初始化 libmedia 核心、加载一个带签名的插件、退出清理。它不会创建视频帧，因此非常适合先确认发布包、签名文件和动态库路径是否完整。

## 1. 编译和运行

必须在发布包根目录执行：

```sh
cmake -S examples/minimal -B build/minimal
cmake --build build/minimal -j2
(cd build/minimal && ./libmedia_minimal)
```

程序固定加载 `../../plugins/libmedia_tpg.so`，相对于 `build/minimal` 的位置如下：

```text
libmedia-release/
├── lib/libmedia.so
├── plugins/libmedia_tpg.so
├── plugins/libmedia_tpg.so.sig
└── build/minimal/libmedia_minimal
```

运行目录错了会找不到插件。也可以从其他目录运行，但要把源码中的相对路径改成绝对路径或正确的相对路径。

## 2. 三个 API 的作用

```c
MEDIA_SYS_Init();
MEDIA_PLUGIN_Load("../../plugins/libmedia_tpg.so");
MEDIA_SYS_Exit();
```

- `MEDIA_SYS_Init`：初始化全局注册表、Pool 和插件管理器；所有其他 API 前都应成功调用。
- `MEDIA_PLUGIN_Load`：读取 `.so`，验证同目录同名 `.so.sig`，成功后注册插件导出的模块类型。
- `MEDIA_SYS_Exit`：销毁仍存活的全局资源；真实程序应先 Stop/Destroy 自己创建的模块。

## 3. 成功时你会看到什么

一行标准输出，两行标准错误，退出码 0：

```text
stdout: libmedia initialized and TPG plugin loaded
stderr: [libmedia license] tpg：本软件为闭源软件，个人非商业使用免费；商业使用请联系 allinjetson@gmail.com。
stderr: [libmedia license] tpg: This is closed-source software. Free for personal, non-commercial use; for commercial use, please contact allinjetson@gmail.com。
exit code: 0
```

**那两行 `[libmedia license]` 是正常的，不是错误。** 每个插件被加载时都会打印一次许可声明（中英各一行），
所以 `stderr` 非空完全不代表失败——本示例退出码是 0。

没有文件产生，也不会出现画面——本示例不处理任何帧。**判断成功与否请看退出码，不要看 stderr 是否为空。**
用 `echo $?` 核对退出码比读日志更快。

失败时退出码为 1，`stdout` 为空，`stderr` 里没有 `[libmedia license]` 行，而是两行具体原因。
下面是把 `plugins/libmedia_tpg.so.sig` 临时改名后实测的输出：

```text
[libmedia] plugin .../plugins/libmedia_tpg.so: signature file .../plugins/libmedia_tpg.so.sig not found; refusing to load
MEDIA_PLUGIN_Load failed: -1013
```

`-1013` 就是 `MEDIA_ERR_PLUGIN_SIGNATURE`（见 `include/media_api.h`）。所以区分成功和失败有两眼可看的标志：
**成功有 `[libmedia license]`、退出码 0；失败没有许可行、退出码 1 且末行是 `MEDIA_PLUGIN_Load failed:`。**

插件越多，许可行越多：`load_all` 加载 32 个插件，会打印 64 行 `[libmedia license]` 后再打印结果。

加载成功只表示插件可以被映射和认证，不表示摄像头、GPU、MPP 或显示设备可用。要继续看到实际帧，请运行 `tpg`、`vi_mipi` 等教程。

## 4. 签名和错误排查

- `MEDIA_ERR_PLUGIN_SIGNATURE`：缺少 `.sig`、签名与文件不匹配，或文件被修改；重新从同一发布包复制 `.so` 和 `.so.sig`。
- `dlopen`/依赖错误：执行 `ldd plugins/libmedia_tpg.so`，检查 `libmedia.so` 是否存在；必要时设置 `LD_LIBRARY_PATH=lib:plugins`。
- `MEDIA_SYS_Init failed`：检查运行架构、glibc 和发布包版本。

不要手工编辑 `.so` 或 `.sig`。修改插件后必须重新签名，否则这是预期的安全拒绝。
