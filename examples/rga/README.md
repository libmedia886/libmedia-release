# RGA 入门：NV12 缩放与常用 2D 操作

RGA 是 Rockchip 的 2D 加速器，适合做缩放、裁剪、旋转、翻转和部分色彩格式转换。这个例子把 640×480 checkerboard NV12 缩小为 320×240 NV12，输入来自 TPG，因此不需要摄像头。

## 1. 参数和端口

程序创建 `RGA_0`，端口名固定为 `input0` 和 `output0`：

| 属性 | 输入 | 输出 |
|---|---|---|
| 尺寸 | 640×480 | 320×240 |
| 格式 | `MEDIA_FORMAT_NV12` | `MEDIA_FORMAT_NV12` |
| stride | 由 `MEDIA_VIDEO_BuildLinearLayout` 得到 | 同左 |
| Pool | Pool 0，TPG 源 | Pool 1，RGA 输出 |
| 队列深度 | 4 | 4 |

`algo=MEDIA_RGA_ALG_RESIZE` 表示缩放。代码中也可尝试 `COPY`（尺寸相同的复制）、`ROTATE`、`FLIP`、`CROP`、`CSC`；每种算法对输入输出尺寸、格式和端口属性有不同约束，不能只改名字而不改尺寸。

## 2. 编译、运行和查看

```sh
cmake -S examples/rga -B build/rga
cmake --build build/rga -j2
LD_LIBRARY_PATH=lib:plugins ./build/rga/rga_tutorial rga_out.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 320x240 rga_out.nv12
```

输入和输出 Pool 使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，因为 RGA 通过 DMA-BUF 导入。每个槽必须至少能容纳对应 layout 的所有 plane（Y + UV），不能只按 `width*height` 分配。拿到输出后必须调用 `MEDIA_RGA_ReleaseFrame`，否则重复运行多帧会耗尽 Pool。

## 3. 代码执行顺序

1. `MEDIA_SYS_Init` 初始化核心并注册 RGA/TPG 插件。
2. 为 640×480 和 320×240 分别调用 `MEDIA_VIDEO_BuildLinearLayout`。
3. 创建源、输出 Pool，填充 `MEDIA_RGA_PORT_ATTR` 的 `port_id/width/height/stride/format/pool_id/queue_depth`。
4. `MEDIA_RGA_CreateGrp` 创建 group，Bind `TPG:output0 → RGA:input0`。
5. Start RGA、Enable TPG，调用 `MEDIA_RGA_GetFrame`。
6. CPU 读同步后保存文件，再 Release、Stop、UnBind、Destroy。

## 4. 对比实验

- 把输出改成 640×480 并使用 `COPY`，观察缩放前后的文件大小和图像是否一致。
- 使用 320×240 输出但把 crop 区域设成输入中央 320×240，比较“缩放全图”和“裁剪后不缩放”的差异。
- 将 `format` 改为插件支持的 RGB/BGR 格式做 CSC；同时修改 ffplay 的 `-pixel_format`，并检查 `docs/plugins/rga.md` 的格式矩阵。

## 5. 常见错误

- `RGA create failed`：尺寸、stride、格式和 algo 的组合不被硬件支持，或 Pool 不是 DMA-BUF。
- 输出花屏：使用 metadata 的 `offset_bytes/pitch_bytes`，不要把 UV 紧接在 `width*height` 后面当作通用规则。
- 运行时超时：TPG 没有 Enable、Bind 失败或输出 Pool 已全部被下游占用。
- 在没有 `/dev/rga`/RGA 驱动的主机上失败是正常的；请在目标 RK3588 板上运行。

详见 [`docs/plugins/rga.md`](../../docs/plugins/rga.md)、[`include/media_rga.h`](../../include/media_rga.h)。
