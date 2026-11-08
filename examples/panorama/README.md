# panorama 实帧教程

该程序生成两路带 metadata 的测试帧，送入 `PANO`，取回输出并写入原始文件。PANO 需要 PTO/LUT 标定资产；示例使用两路输入和默认 LUT，部署时应设置 pto_path。

## 编译和运行

```sh
cmake -S examples/panorama -B build/panorama
cmake --build build/panorama -j2
(cd build/panorama && ./panorama_tutorial output.nv12)
```

参数一是输出路径，参数二可指定插件绝对路径，参数三传入标定资产路径（SVM3D 为 asset manifest，PANO 为 PTO）。输入尺寸为 640×480；程序会创建输入/输出 DMA-BUF Pool、填充两种渐变、发送、取帧和归还。输出尺寸和字节数由程序打印。

## 模块参数

完整字段和限制见 [include/media_panorama.h](../../include/media_panorama.h) 与 [docs/plugins/panorama.md](../../docs/plugins/panorama.md)。示例故意保留真实模块要求：SVM3D 的 `asset_path`、PANO 的 `pto_path/lut_path` 需要项目标定文件时由调用方填入，无法用空字符串伪造环视几何。
