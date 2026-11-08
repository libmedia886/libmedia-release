# EIS 入门：电子防抖模块的输入输出

EIS（Electronic Image Stabilization）根据连续帧的运动估计和裁剪区域，抵消手持抖动。本示例用 `TPG` 的移动方块代替相机，建立 `TPG → EIS` 管线并保存一帧 NV12。当前发布插件实现的是 GLES 恒等 warp/直通路径：它能验证格式、Pool、端口和生命周期，`crop_ratio` 等 DIS 参数会保存并校验，但不会凭空产生真实的 IMU 运动补偿。

## 1. 参数是什么意思

`MEDIA_EIS_ATTR` 的核心字段：

| 字段 | 示例值 | 说明 |
|---|---:|---|
| `width/height` | 640/480 | 输入和输出可见尺寸；当前要求相同且通常为偶数 |
| `format` | `MEDIA_FORMAT_NV12` | 当前 GLES 路径只接受线性 NV12 |
| `input_depth` | 4 | 输入队列深度，太小会阻塞上游，太大浪费内存 |
| `output_pool_id` | 1 | 输出 DMA-BUF Pool，由应用创建 |
| `input_stride/output_stride` | layout pitch | 每行字节数，不能只写可见宽度（需遵守对齐） |
| `crop_ratio` | 0.84 | 保留画面百分比；真实防抖用它换取边缘裁剪空间 |
| `passthrough` | 1 | 直通开关；本例设 1 便于在没有 IMU 的板子上验证链路 |

头文件中还保留 `mode`、`motion_level`、`smoothing_window`、`search_radius`、IMU 和 DIS 结构。这些字段用于真实产品配置；使用前应确认目标插件是否实现对应算法。

## 2. 编译和运行

```sh
cmake -S examples/eis -B build/eis
cmake --build build/eis -j2
LD_LIBRARY_PATH=lib:plugins ./build/eis/eis_tutorial eis_out.nv12
ffplay -f rawvideo -pixel_format nv12 -video_size 640x480 eis_out.nv12
```

程序默认创建两个 640×480 NV12 `NATIVE` Pool：Pool 0 给 TPG 输出，Pool 1 给 EIS 输出。视频 Pool 必须能导出 DMA-BUF，因此不能随意改成 `HOST`。

## 3. 管线的每一步

1. `MEDIA_VIDEO_BuildLinearLayout` 计算 NV12 的 Y/UV 两个 plane、pitch 和总大小。
2. 创建两个 Pool，每个 4 个槽；4 是队列深度示例，不是固定要求。
3. 创建 TPG moving-box，输出端口为 `TPG_0:output0`。
4. 创建 EIS_0，输入端口为 `input0`，输出由 `output_pool_id` 提供。
5. `MEDIA_SYS_Bind("TPG",0,"output0","EIS",0,"input0")` 建立连接。
6. 先 `MEDIA_EIS_Start`，再 `MEDIA_TPG_Enable`，然后从 `MEDIA_EIS_GetFrame` 取帧。
7. 读同步、写 NV12 文件并 `MEDIA_EIS_ReleaseFrame`；退出时按反向顺序 Stop、UnBind、Destroy。

## 4. 连接真实 VI/IMU 时怎么改

把 TPG 替换成 VI：先用 `examples/vi_mipi` 确认相机能输出同尺寸 NV12，再将 Bind 的源改为 `"VI", dev`。真实 EIS 还需要按时间戳把陀螺仪样本送入 DIS/IMU API；只修改 `crop_ratio` 不会生成运动估计。输出尺寸固定时 `scale_output` 应保持插件要求的值。

## 5. 常见问题

- `format unsupported`：输入不是线性 NV12，或 width/height 为奇数。
- `GetFrame timeout`：没有先 Enable TPG、没有 Bind，或输入 Pool 槽数被占满。
- 关闭 `passthrough` 后失败：当前插件不承诺无 IMU 的完整运动估计；先用直通模式验证硬件链路。
- 输出全黑：检查 `MEDIA_POOL_BeginCpuAccess/EndCpuAccess` 和 NV12 文件的尺寸参数。

详见 [`docs/plugins/eis.md`](../../docs/plugins/eis.md)、[`include/media_eis.h`](../../include/media_eis.h)。
