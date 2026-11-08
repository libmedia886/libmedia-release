# VDEC 入门：H.264 码流 → NV12 帧

VDEC 把压缩的视频码流解码为原始图像。这个例子读取一个完整的 H.264 文件，把它作为一个 packet 送给 RK3588 MPP 解码器，取出第一帧 NV12 保存。最简单的输入来自本仓库的 `examples/venc`。

## 1. 先生成输入码流

```sh
cmake -S examples/venc -B build/venc && cmake --build build/venc -j2
LD_LIBRARY_PATH=lib:plugins ./build/venc/venc_tutorial venc_out.h264
```

然后编译、运行 VDEC：

```sh
cmake -S examples/vdec -B build/vdec
cmake --build build/vdec -j2
LD_LIBRARY_PATH=lib:plugins ./build/vdec/vdec_tutorial venc_out.h264 decoded.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 320x240 decoded.nv12
```

程序参数：`[输入 H.264] [输出 NV12]`，默认分别是 `venc_out.h264` 和 `vdec_out.nv12`。

## 2. 关键参数

`MEDIA_VDEC_ATTR` 中：

- `width/height/stride` 必须与码流编码时的图像一致。本例为 320×240，stride 320。
- `video_type` 是 `MEDIA_VIDEO_H264`；解 H.265 时改成对应枚举，并使用 H.265 码流。
- `buf_cnt` 是解码参考帧/在途帧数量，太小可能无法创建或卡住。
- `output_format=0` 表示插件默认 NV12；`output_compress=NONE` 表示线性输出，便于 CPU 保存。
- `pool_id` 是输出 Pool ID。示例先调用 `MEDIA_VDEC_QueryOutputPoolRequirements`，按返回的 `slot_size/slot_count` 创建 `NATIVE` Pool。

VDEC 的输入端是压缩 packet，不是 `MEDIA_BUFFER` 视频帧；本例调用 `MEDIA_VDEC_SendPacket`。真实网络接收时应按访问单元（access unit）分包，并循环 Send/Get，而不是把无限增长的文件一次性读入内存。

## 3. 为什么要 QueryOutputPoolRequirements

解码器需要额外的对齐、参考帧和硬件布局。只用 `width*height*3/2` 估算常会导致 `CreateChn` 失败。查询接口返回插件实际需要的每个槽大小和数量，应用必须使用这些值创建 Pool；槽数不足会造成 GetFrame 超时。

## 4. 常见错误

- `open ... failed`：路径错误；先运行 VENC，或把相机/网络得到的 H.264 文件传入。
- `VDEC create failed (MPP/device unavailable?)`：检查 RK3588 MPP、`/dev/mpp_service`、插件 `.so/.sig` 和动态库路径；x86 主机不能代替目标板验证硬件解码。
- `decode timeout`：文件不是完整访问单元、编码格式与 `video_type` 不一致，或宽高与码流 SPS 不符。
- ffplay 花屏：输出不是紧密布局时，应从 metadata 读取真实 pitch；本例的 320×240 VENC 输出是线性布局。

详见 [`docs/plugins/vdec.md`](../../docs/plugins/vdec.md)、[`include/media_vdec.h`](../../include/media_vdec.h)。
