# Load All 插件扫描教程

这个示例扫描发布包的 `plugins/` 目录，逐个调用 `MEDIA_PLUGIN_Load()`，用于检查插件文件、签名和运行时依赖是否完整。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/load_all -B build/load_all
cmake --build build/load_all -j2
(cd build/load_all && ./libmedia_load_all)
```

程序会打印每个插件的授权提示和最终成功数量。单个进程最多同时加载 `MEDIA_PLUGIN_MAX_LOADED`（32）个插件；发布包目前有 37 个插件，因此看到 `loaded 32 signed plugins` 是预期行为，实际应用应只加载需要的模块。

## 关键 API

- `MEDIA_SYS_Init()`：初始化核心。
- `MEDIA_PLUGIN_Load(path)`：校验 `<path>.sig` 并加载插件。
- `MEDIA_SYS_Exit()`：统一清理。

这个示例只验证加载，不创建模块、不启动采集或处理管线。
