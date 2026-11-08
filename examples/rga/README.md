# rga 教程：NV12 缩放

程序将 640×480 checkerboard TPG 连接到 RGA `RESIZE`，输出 320×240 NV12。

```sh
cmake -S examples/rga -B build/rga && cmake --build build/rga
LD_LIBRARY_PATH=lib:plugins ./build/rga/rga_tutorial [out.nv12]
```

将 `algo` 改为 `COPY/ROTATE/FLIP/CROP/CSC` 并调整 input/output port 属性，即可对比不同算法；压缩格式需按 `docs/plugins/rga.md` 的矩阵约束配置。
