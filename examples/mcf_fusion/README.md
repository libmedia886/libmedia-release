# mcf_fusion 实帧教程

该程序生成两路带 metadata 的测试帧，送入 `MCF_FUSION_CL`，取回输出并写入原始文件。参数 0=融合，1=彩色直通，2=单色直通；程序会送入两路渐变图。

## 编译和运行

```sh
cmake -S examples/mcf_fusion -B build/mcf_fusion
cmake --build build/mcf_fusion -j2
(cd build/mcf_fusion && ./mcf_fusion_tutorial output.nv12)
```

参数一是输出路径，参数二可指定插件绝对路径。输入尺寸为 640×480；程序会创建输入/输出 DMA-BUF Pool、填充两种渐变、发送、取帧和归还。输出尺寸和字节数由程序打印。

## 模块参数

完整字段和限制见 [include/media_mcf_fusion.h](../../include/media_mcf_fusion.h) 与 [docs/plugins/mcf_fusion.md](../../docs/plugins/mcf_fusion.md)。示例故意保留真实模块要求：SVM3D 的 `asset_path`、PANO 的 `pto_path/lut_path` 需要项目标定文件时由调用方填入，无法用空字符串伪造环视几何。
