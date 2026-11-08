# Examples / 模块教程

每个模块子目录都是一个独立教程：README 说明模块用途、参数、Pool/格式约束和运行方式，CMake 文件和 C 程序可以直接编译。`common/` 是教程共用的加载检查代码，不是独立模块。

| 目录 | 模块 | 作用 |
| --- | --- | --- |
| [`minimal/`](minimal/) | Plugin host | 初始化核心并加载一个已签名插件 |
| [`load_all/`](load_all/) | Plugin host | 扫描插件目录并验证签名、依赖和加载上限 |
| [`vpss_16out/`](vpss_16out/) | TPG + VPSS | 一路输入生成 16 路独立变换输出 |
| [`vi_mipi/`](vi_mipi/) | VI | 从 RK3588 MIPI camera / RKISP 采集 NV12 帧 |
| [`tpg/`](tpg/) | TPG | 生成 NV12 测试图案帧 |

其余插件目录也按同一规则提供 smoke tutorial：每个目录包含 README、CMake 和 `main.c`，程序会验证签名、ABI、动态依赖和注册的主 type。它们先确认模块发布物可加载，再按 README 中的模块参数接入真实输入输出管线。

每个示例都从发布包根目录构建，例如：

```sh
cmake -S examples/vi_mipi -B build/vi_mipi
cmake --build build/vi_mipi
```
