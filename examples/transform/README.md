# TRANSFORM 入门：用 LUT 做缩放、裁剪和镜像

`TRANSFORM` 根据查找表（LUT, Look-Up Table）为输出的每个像素指定输入坐标。与只描述一个矩形的 resize 不同，LUT 可以表达非线性映射、镜像、局部裁剪和镜头畸变校正。本例生成 640×480 NV12 渐变，建立 320×240 的双线性 identity LUT，实现缩小。

## 1. LUT 是什么

LUT 中每个输出像素存两个 `float`：`(src_x, src_y)`。例如输出 `(10,20)` 的 LUT 值是 `(20,40)`，表示从输入 `(20,40)` 采样。代码把 `x` 按 `x*IN_W/OUT_W`、`y` 按 `y*IN_H/OUT_H` 填入，因此看起来是普通缩放。

- x/y 坐标从 0 开始，通常允许小数，插件按采样模式插值。
- 越界坐标通常输出黑色或被插件裁剪，应用不要依赖未定义行为。
- LUT 尺寸必须等于输出可见宽高，`lut_size = out_w*out_h*2*sizeof(float)`。

端口为 `TRANSFORM_0:input0/output0`。输入输出都使用线性 NV12；输出 Pool 由 `pool_id` 指定并由应用创建。

## 2. 编译、运行和查看

```sh
cmake -S examples/transform -B build/transform
cmake --build build/transform -j2
LD_LIBRARY_PATH=lib:plugins ./build/transform/transform_tutorial transform_out.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 320x240 transform_out.nv12
```

输入 Pool 0 为 640×480，输出 Pool 1 为 320×240；两者均为 `NATIVE` DMA-BUF。`in_stride/out_stride` 必须使用 `MEDIA_VIDEO_BuildLinearLayout` 返回的 pitch，不能假设等于 width。

## 3. 逐步流程

1. 计算两种尺寸的 NV12 layout 和 Pool 大小。
2. 分配 LUT，并按输出坐标填入源坐标。
3. 填写 `MEDIA_TRANSFORM_ATTR`：输入/输出宽高、format、stride、`input_depth`、`pool_id`、LUT 指针、LUT 尺寸。
4. 创建 Transform，创建渐变 TPG，Bind `TPG:output0 → TRANSFORM:input0`。
5. Start/Enable 后取一帧，读同步、保存、Release。

## 4. 修改 LUT 做实验

- **左右镜像**：`src_x = IN_W - 1 - x*IN_W/OUT_W`。
- **上下镜像**：`src_y = IN_H - 1 - y*IN_H/OUT_H`。
- **中央裁剪**：给 x/y 加上 `(IN_W-OUT_W)/2`、`(IN_H-OUT_H)/2`。
- **鱼眼/桶形**：先把坐标归一化到 [-1,1]，按半径做非线性变换，再映射回像素。

每次修改后仍要保证 LUT 地址有效且和输出尺寸匹配；插件不会替应用管理 LUT 的生命周期。

## 5. 常见错误

- `CreateGrp` 失败：LUT size/指针不正确、格式不支持，或输入输出 Pool 不是可导入 DMA-BUF。
- 输出黑屏：坐标全部越界，或 CPU 写 LUT 后未按插件要求同步。
- 画面倾斜/花屏：stride、UV offset 或输入尺寸写错。

详见 [`docs/plugins/transform.md`](../../docs/plugins/transform.md) 与 [`include/media_transform.h`](../../include/media_transform.h)。
