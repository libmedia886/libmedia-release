# OSD 入门：在 NV12 视频上叠加矩形

OSD（On Screen Display）把文字、矩形、位图或 mask 合成到视频上。本例用色条 TPG 生成 640×480 NV12，在坐标 `(40,30)` 叠加一个 240×120 的半透明红色矩形，再写出一帧 NV12。

## 1. 先理解坐标和 Alpha

坐标原点在左上角，`x` 向右、`y` 向下。`width/height` 是 region 的外框尺寸。`global_alpha` 范围通常是 0（全透明）到 255（不透明）；本例 220 表示大约 86% 不透明。矩形颜色使用 `MEDIA_OSD_RECT_DESC.color` 的 RGBA 字段，插件会根据目标 NV12 做色彩转换。

模块端口：`OSD_0:input0` 接收 NV12，`output0` 输出叠加后的 NV12。程序通过 Bind 连接 TPG，使用 `MEDIA_OSD_UpdateRegion` 配置 region，再用 `MEDIA_OSD_SetRegionRect` 填充矩形描述。

## 2. 编译、运行和查看

```sh
cmake -S examples/osd -B build/osd
cmake --build build/osd -j2
LD_LIBRARY_PATH=lib:plugins ./build/osd/osd_tutorial osd_out.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 osd_out.nv12
```

程序使用两个 640×480 NV12 `NATIVE` Pool：TPG 的源 Pool 和 OSD 的输出 Pool，各 4 个槽。NATIVE 是 DMA-BUF 内存，适合硬件/GLES 处理；如果改成 HOST，创建或导入可能失败。

## 3. 代码流程

1. 初始化系统并计算 NV12 layout（plane0 是 Y，plane1 是交错 UV）。
2. 创建 TPG、OSD 和两个 Pool。
3. 设置 `MEDIA_OSD_ATTR.input_width/input_height/format/input_depth/output_pool_id`。
4. 设置 region 的 `enabled/x/y/width/height/global_alpha`。
5. 设置 `MEDIA_OSD_RECT_DESC.filled` 和 `color`。
6. Bind、Start、Enable，取一帧并写文件。
7. Release 输出，再 Stop/UnBind/Destroy。

region ID 从 0 开始；`max_regions=4` 表示本实例最多管理 4 个 region，并不代表必须创建 4 个。

## 4. 做两个对比实验

- 把 `global_alpha` 改成 64、128、255，观察透明程度。
- 把 `(x,y,width,height)` 改成 `(0,0,640,60)`，得到顶部色带；超出输入尺寸通常会被拒绝。
- 将矩形改为 A8 mask：准备一张每像素 8 bit 的 mask，并使用 `MEDIA_OSD_SetRegionMask`。mask 的 stride 和 Pool 大小必须按文档计算。
- 把 TPG 的 `COLOR_BARS` 改成 `CHECKERBOARD`，更容易观察边缘对齐。

## 5. 常见问题

- region 不可见：确认 `enabled=1`、Start 顺序正确、alpha 非 0，且 region 在画面范围内。
- `SetRegionRect` 失败：颜色/尺寸结构未初始化，或 region ID 超过 `max_regions`。
- 输出花屏：NV12 的 pitch/offset 必须使用 metadata/layout，不要假设 stride 等于 width。
- `GetFrame timeout`：检查 TPG 是否 Enable、Bind 是否成功以及输出 Pool 是否还有空槽。

详见 [`docs/plugins/osd.md`](../../docs/plugins/osd.md) 和 [`include/media_osd.h`](../../include/media_osd.h)。
