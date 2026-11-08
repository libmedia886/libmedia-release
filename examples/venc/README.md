# VENC 入门：NV12 帧 → H.264 码流

VENC 把原始视频帧压缩成 H.264/H.265。示例程序生成 4 帧 320×240 的灰度渐变 NV12，送入 RK3588 MPP 编码器，把返回的 packet 依次写成一个 `out.h264` 文件。它和 `examples/vdec` 配套，可用于学习完整的“原始帧→码流→原始帧”流程。

## 1. 编译、运行和播放

```sh
cmake -S examples/venc -B build/venc
cmake --build build/venc -j2
LD_LIBRARY_PATH=lib:plugins ./build/venc/venc_tutorial out.h264
ffplay -f h264 out.h264
```

默认输出 `venc_out.h264`。也可以立即解码第一帧：

```sh
cmake -S examples/vdec -B build/vdec && cmake --build build/vdec -j2
LD_LIBRARY_PATH=lib:plugins ./build/vdec/vdec_tutorial out.h264 decoded.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 320x240 decoded.nv12
```

## 2. 每个参数的意义

示例调用 `MEDIA_VENC_CreateChn(0, 320, 240, 320, 30, 4, 4, 1, 4000000, 30, H264, NONE)`，对应：

| 参数 | 示例值 | 作用 |
|---|---:|---|
| channel | 0 | 编码通道编号 |
| width/height | 320/240 | 可见图像尺寸 |
| stride | 320 | 输入 Y plane 每行字节数 |
| fps | 30 | 时间基准，影响码率和参考帧调度 |
| input_depth | 4 | 输入排队深度 |
| output_depth | 4 | packet 输出排队深度 |
| output_pool_id | 1 | 码流 Buffer Pool |
| bitrate | 4000000 | 目标码率 bit/s；越大通常画质越好、文件越大 |
| gop | 30 | I 帧间隔；越小越易随机 seek、码率更高 |
| video_format | H.264 | 可换成插件支持的 H.265 枚举 |
| compress | NONE | 输入为线性 NV12；不要把压缩标志当编码格式 |

输入 Pool 0 是 `NATIVE` DMA-BUF，输出码流 Pool 1 是 `HOST` Pool（CPU 直接写文件）。码流槽大小本例固定 256 KiB；生产应用应按最大帧大小和码率计算，并处理满池/阻塞。

## 3. 代码流程

1. 创建系统和两个 Pool，计算 NV12 canonical layout。
2. 创建并 Start `VENC_0`。
3. 对每帧从输入 Pool 取 Buffer，写入 Y（亮度）和 UV（128 表示中性色），设置 `MEDIA_BUFFER_META_VIDEO_FRAME`。
4. `MEDIA_SYS_SendFrame("VENC",0,"input0",buf,1000)` 成功后由编码器持有输入 Buffer。
5. `MEDIA_VENC_GetPacket` 取压缩片段，写入文件，再 `MEDIA_VENC_ReleasePacket`。
6. 全部帧结束后 Stop、Destroy，最后销毁 Pool。

输出 packet 可能一个输入帧对应多个片段，也可能带有 SPS/PPS。写文件时必须保持顺序，不能丢掉长度为非零的 packet。

## 4. 常见问题

- `VENC create failed`：没有 RK3588 MPP 或 `/dev/mpp_service` 权限，x86 主机只能编译不能硬件运行。
- 没有 packet：输入 metadata 的 format/stride 不正确，或输出 Pool 太小。
- ffplay 无法识别：确认文件是 Annex-B H.264，且没有把日志写进输出文件；可用 `file out.h264`、`ffprobe -f h264 out.h264` 检查。
- 画面尺寸不对：stride 是每行字节数，不是可见宽度；修改 width 时要重新计算 layout。

详见 [`docs/plugins/venc.md`](../../docs/plugins/venc.md) 和 [`include/media_venc.h`](../../include/media_venc.h)。
