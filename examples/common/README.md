# Common tutorial helper

这里保存教程共用的 `filter_runner.h` 和插件加载检查代码 `plugin_smoke.c`，不是一个独立模块教程。前者负责创建 TPG 输入、native DMA-BUF Pool、绑定处理模块、取帧和写出 raw 文件；后者供加载检查场景复用。各模块目录仍包含自己的 `main.c`、CMake 和 README。
