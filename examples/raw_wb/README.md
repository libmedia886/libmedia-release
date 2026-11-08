# raw_wb 教程：Bayer 白平衡

程序生成 RGGB RAW16，使用 `(r_gain,g_gain,b_gain)=(1.8,1.0,1.35)` 处理并写出 RAW16。

```sh
cmake -S examples/raw_wb -B build/raw_wb && cmake --build build/raw_wb
LD_LIBRARY_PATH=lib:plugins ./build/raw_wb/raw_wb_tutorial [out.raw16]
```

在 `main.c` 中调整三路 gain 或 Bayer pattern；输出仍是 RAW16，可直接接 demosaic。板上要求 native DMA-BUF 和 GLES。
