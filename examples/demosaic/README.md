# demosaic 教程：RAW16 Bayer → RGB_U16

程序生成 64×48 的 RGGB RAW16 棋盘图，送入 DEMOSAIC，输出交错 RGB_U16 原始文件。输入输出 stride 均为 64 字节对齐。

```sh
cmake -S examples/demosaic -B build/demosaic && cmake --build build/demosaic
LD_LIBRARY_PATH=lib:plugins ./build/demosaic/demosaic_tutorial [out.rgb_u16]
```

修改 `bayer_pattern` 为 BGGR/GBRG/GRBG 可观察 CFA 相位差异；真实相机 RAW 只需替换送入 input0 的 buffer。
