# DEMOSAIC 入门：Bayer RAW16 → RGB_U16

相机传感器通常输出 Bayer 马赛克：每个像素只有一个颜色采样，按 RGGB、BGGR、GBRG 或 GRBG 周期排列。`DEMOSAIC`（去马赛克）根据邻近像素估算缺失的两个颜色，输出每个像素都有 R/G/B 的交错图像。本例不需要相机，程序生成 64×48 的 RAW16 棋盘图，送入模块并保存 RGB_U16。

## 1. 输入、输出和端口

- 模块实例：`DEMOSAIC_0`。
- 输入端口：`input0`，单平面 `MEDIA_FORMAT_BAYER_RAW16_LE`。
- 输出端口：`output0`，交错 RGB_U16；每像素 3 个 16 bit 通道，共 6 字节。
- `input_stride` / `output_stride`：一行实际占用的字节数，可能大于可见宽度乘字节数，因为 DMA 布局通常按 64 字节对齐。
- `input_depth`：输入队列能排队多少帧；不是图像位深。
- `output_pool_id`：应用预先创建的输出 Pool。

Bayer pattern 是“第 0 行第 0 列是什么颜色”。如果 pattern 错了，画面会出现明显的彩色网格，即使尺寸完全正确也不代表配置正确。

## 2. 编译、运行和查看输出

```sh
cmake -S examples/demosaic -B build/demosaic
cmake --build build/demosaic -j2
LD_LIBRARY_PATH=lib:plugins ./build/demosaic/demosaic_tutorial out.rgb_u16
ls -l out.rgb_u16
```

输出是裸 RGB_U16，没有 PNG 头。当前程序固定 64×48、每像素 6 字节，文件应约为 `64*48*6 = 18432` 字节（Pool 可能包含对齐填充）。查看时要按交错 16 bit、小端、RGB 顺序解释；可用 Python 检查：

```sh
python3 - <<'PY'
import numpy as np
p=np.fromfile('out.rgb_u16',dtype='<u2')[:64*48*3].reshape(48,64,3)
print(p.min(), p.max(), p[0,0], p[20,20])
PY
```

## 3. 逐步理解程序

1. `MEDIA_VIDEO_BuildLinearLayout` 计算 RAW16 的 canonical layout 和 Pool 大小。
2. 创建输入、输出两个 `NATIVE` Pool。NATIVE 表示可导入 DMA-BUF 的连续内存，适合 GLES/硬件插件。
3. `MEDIA_DEMOSAIC_Create(0, &attr)` 设置尺寸、stride、Bayer pattern 和输出 Pool。
4. 从输入 Pool 取一个 Buffer，CPU 填入棋盘 RAW16，并用 `MEDIA_BUFFER_META_VIDEO_FRAME` 描述格式和 layout。
5. `MEDIA_SYS_SendFrame("DEMOSAIC", 0, "input0", in, 1000)` 送帧。
6. `MEDIA_DEMOSAIC_GetFrame` 取输出，读同步后写文件，最后 `ReleaseFrame`。

Send 成功后 Buffer 的所有权交给模块；不要再次 `MEDIA_POOL_PutBuffer`。Get 到的输出必须 Release，否则 Pool 很快耗尽。

## 4. 可以尝试的修改

在 `main.c` 中：

- 把 `MEDIA_BAYER_PATTERN_RGGB` 改为 `BGGR/GBRG/GRBG`，观察颜色相位差异。
- 把 `W/H` 改成偶数尺寸，并同步修改输入填充和输出 stride。
- 将输入填充替换为 VI/RKCIF 采到的真实 RAW16；真实传感器的 bit packing（RAW10/RAW12）不能直接当 RAW16 使用，需要先解包。

## 5. 常见错误

- `RAW layout setup failed`：尺寸、格式或 stride 不符合公开 layout 规则。
- `pool setup failed`：输出 Pool 太小；至少 `output_stride * height` 字节，槽数要覆盖在途帧。
- 输出全黑或颜色异常：检查 CPU cache begin/end、Bayer pattern、RAW 数据端序和有效位位置。
- 板上创建失败：确认 `libmedia_demosaic.so`、`.sig`、GLES 驱动和 `LD_LIBRARY_PATH=lib:plugins`。

详见 [`docs/plugins/demosaic.md`](../../docs/plugins/demosaic.md) 和 [`include/media_demosaic.h`](../../include/media_demosaic.h)。
