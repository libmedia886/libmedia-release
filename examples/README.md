# Examples / 模块教程

每个模块子目录都是一个独立教程：README 说明模块用途、参数、Pool/格式约束和运行方式，CMake 文件和 C 程序可以直接编译。`common/` 是教程共用的加载检查代码，不是独立模块。

| 目录 | 模块 | 作用 |
| --- | --- | --- |
| [`minimal/`](minimal/) | Plugin host | 初始化核心并加载一个已签名插件 |
| [`load_all/`](load_all/) | Plugin host | 扫描插件目录并验证签名、依赖和加载上限 |
| [`vpss_16out/`](vpss_16out/) | TPG + VPSS | 一路输入生成 16 路独立变换输出 |
| [`vi_mipi/`](vi_mipi/) | VI | 从 RK3588 MIPI camera / RKISP 采集 NV12 帧 |
| [`tpg/`](tpg/) | TPG | 生成 NV12 测试图案帧 |

## 教程类型

除了 `minimal` 和 `load_all` 这两个专门讲插件加载的教程，其余目录都会创建模块并运行一条最小可观察管线。输入优先使用 TPG 或程序生成的 DMA-BUF 测试帧，程序会设置 `VIDEO_FRAME` metadata、创建输入/输出 Pool、送帧、取帧并保存结果。

| 类型 | 目录 |
| --- | --- |
| 真实采集/显示 | `vi_mipi`, `vo`, `vo_wbc` |
| 处理和融合 | `vpss_16out`, `clahe`, `conv`, `cap_dehaze`, `dcp_fast_dehaze`, `highlight_suppress`, `retinex`, `tnr`, `wavelet_nr`, `thermal`, `resize_rga`, `csc_rga`, `blend_pyr`, `dualview`, `stereo_3d`, `edof`, `exposure_fusion`, `fluo_visible_fusion`, `thermal_lowlight_fusion`, `mcf_fusion`, `vmix`, `vmix_rga`, `svm_3d`, `panorama`, `eis`, `osd`, `rga`, `transform`, `raw_wb`, `demosaic` |
| 编解码/音频 | `venc`, `vdec`, `audio` |
| 生成输入 | `tpg` |

`svm_3d` 和 `panorama` 的程序也是真实送帧管线，但必须额外提供项目标定文件；README 说明了参数位置。`venc`/`vdec` 需要 MPP，`vi_mipi`/`vo`/`vo_wbc` 需要对应的摄像头或 DRM 设备，设备不存在时程序会打印原因并退出。

每个示例都从发布包根目录构建，例如：

```sh
cmake -S examples/vi_mipi -B build/vi_mipi
cmake --build build/vi_mipi
```
