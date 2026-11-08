# RAW_WB 入门：Bayer RAW16 白平衡

白平衡（White Balance）在去马赛克前对 Bayer 的 R、G、B 采样分别乘增益，使白色物体在不同色温下仍接近中性。`RAW_WB` 不改变 Bayer 排列，也不把 RAW 变成 RGB；它的输出仍是 `MEDIA_FORMAT_BAYER_RAW16_LE`，可以直接接到 `demosaic` 示例。

## 1. 参数含义

本例输入 64×48、RGGB、RAW16，增益为 `r=1.8, g=1.0, b=1.35`：

- `width/height`：可见尺寸；通常要求与真实传感器帧一致。
- `input_depth`：输入队列深度。
- `input_stride/output_stride`：每行字节数；RAW16 最少是 `width*2`，还要满足设备对齐。
- `bayer_pattern`：`RGGB/BGGR/GBRG/GRBG`，必须与传感器第一像素对应。
- `r_gain/g_gain/b_gain`：三色乘法增益。增益 1.0 不改变，增益 2.0 约使对应通道变亮一倍；过大可能饱和。
- `output_pool_id`：输出 DMA-BUF Pool ID。

## 2. 编译、运行和检查数值

```sh
cmake -S examples/raw_wb -B build/raw_wb
cmake --build build/raw_wb -j2
LD_LIBRARY_PATH=lib:plugins ./build/raw_wb/raw_wb_tutorial out.raw16
ls -l out.raw16
python3 - <<'PY'
import numpy as np
p=np.fromfile('out.raw16',dtype='<u2').reshape(48,64)
print('min/max/center=',p.min(),p.max(),p[24,32])
PY
```

程序写出的是带 stride 的原始 Buffer，不能直接当成紧密 `64*48` 数组；如果修改尺寸或 stride，Python 的 reshape 也要同步修改。把输出送入 demosaic 时，要复用 metadata 中的 layout。

## 3. Pool 和生命周期

输入 Pool 0 存放应用生成/相机送来的 RAW 帧，输出 Pool 1 存放校正结果，两个 Pool 都用 `MEDIA_POOL_ALLOC_MODE_NATIVE`。创建顺序是：`MEDIA_SYS_Init → Pool → MEDIA_RAW_WB_Create → Start`；退出顺序反过来。成功调用 `MEDIA_SYS_SendFrame` 后，输入 Buffer 由模块持有，不能立即 Put；输出取到后必须 `MEDIA_RAW_WB_ReleaseFrame`。

## 4. 真实相机使用

真实相机可能输出 RAW10/RAW12 packed，而本插件输入契约是 RAW16 little-endian。先在驱动或采集程序中把每个像素解包到 16 bit，再设置正确的 Bayer pattern、black level 和 stride。白平衡增益通常来自镜头/色温标定；不要盲目把三个通道都调大。

## 5. 常见问题

- 颜色整体偏红/偏蓝：Bayer pattern 与传感器不匹配，或增益顺序写反。
- 高光变成纯白：增益后超过 16 bit 最大值，检查插件的饱和策略并降低 gain。
- 创建失败：确认目标板有 GLES、Pool 是 NATIVE、stride 与 layout 对齐。
- 输出不变：检查是否在 CPU 写入后调用 `MEDIA_POOL_EndCpuAccess(...WRITE)`。

更多字段见 [`docs/plugins/raw_wb.md`](../../docs/plugins/raw_wb.md) 和 [`include/media_raw_wb.h`](../../include/media_raw_wb.h)。
