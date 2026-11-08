# svm_3d 实帧教程

该程序生成两路带 metadata 的测试帧，送入 `SVM3D`，取回输出并写入原始文件。SVM3D 需要实际标定资产；示例展示两路输入同步、输出 Pool 和 ABGR 输出，未提供 mesh 时创建可能失败。

## 编译和运行

```sh
cmake -S examples/svm_3d -B build/svm_3d
cmake --build build/svm_3d -j2
(cd build/svm_3d && ./svm_3d_tutorial output.rgba)
```

参数一是输出路径，参数二可指定插件绝对路径，参数三传入标定资产路径（SVM3D 为 asset manifest，PANO 为 PTO）。输入尺寸为 640×480；程序会创建输入/输出 DMA-BUF Pool、填充两种渐变、发送、取帧和归还。输出尺寸和字节数由程序打印。

## 模块参数

完整字段和限制见 [include/media_svm_3d.h](../../include/media_svm_3d.h) 与 [docs/plugins/svm_3d.md](../../docs/plugins/svm_3d.md)。示例故意保留真实模块要求：SVM3D 的 `asset_path`、PANO 的 `pto_path/lut_path` 需要项目标定文件时由调用方填入，无法用空字符串伪造环视几何。
