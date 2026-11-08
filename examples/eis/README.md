# eis 教程：TPG → EIS

程序以 moving-box TPG 作为输入，EIS 使用 NV12 线性帧和 passthrough 路径输出一帧。当前插件是恒等 warp，`crop_ratio` 等参数会被保存但不会产生运动估计。

```sh
cmake -S examples/eis -B build/eis && cmake --build build/eis
LD_LIBRARY_PATH=lib:plugins ./build/eis/eis_tutorial [out.nv12]
```

关闭 `passthrough` 或修改 `crop_ratio` 可观察参数校验；传感器/融合模式在当前实现会明确拒绝，不会静默降级。
