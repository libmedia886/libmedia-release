# transform 教程：LUT 缩放

程序生成 640×480 NV12 渐变，创建 320×240 identity XY LUT（双线性采样），输出缩小后的 NV12。

```sh
cmake -S examples/transform -B build/transform && cmake --build build/transform
LD_LIBRARY_PATH=lib:plugins ./build/transform/transform_tutorial [out.nv12]
```

把 LUT 的 x/y 加上偏移或反转，可观察裁剪和镜像；越界坐标会输出黑色。输入输出必须是 GPU 可导入的线性/AFBC tuple。
