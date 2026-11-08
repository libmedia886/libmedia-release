# vdec 教程：H.264 → NV12

程序读取 H.264 文件，按 `MEDIA_VDEC_QueryOutputPoolRequirements` 创建输出 pool，解码并写出第一帧 NV12。

```sh
cmake -S examples/vdec -B build/vdec && cmake --build build/vdec
# 先运行 venc 生成输入
LD_LIBRARY_PATH=lib:plugins ./build/vdec/vdec_tutorial venc_out.h264 [out.nv12]
```

`video_type` 可切换 H.265；宽高、stride 必须与码流一致。需要 RK3588 MPP。
