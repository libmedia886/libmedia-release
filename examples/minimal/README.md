# Minimal 插件加载教程

这个示例展示最小的 libmedia 生命周期：初始化核心、加载一个带 `.sig` 的插件、退出。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/minimal -B build/minimal
cmake --build build/minimal -j2
(cd build/minimal && ./libmedia_minimal)
```

程序固定加载 `../../plugins/libmedia_tpg.so`。运行目录必须是 `build/minimal`，插件旁边的 `libmedia_tpg.so.sig` 也必须存在。

## 关键 API

- `MEDIA_SYS_Init()`：初始化核心。
- `MEDIA_PLUGIN_Load()`：先校验相邻 `.sig`，再映射插件。
- `MEDIA_SYS_Exit()`：销毁实例并卸载插件。

修改插件任意字节、删除签名或使用错误签名密钥都会返回 `MEDIA_ERR_PLUGIN_SIGNATURE`。
