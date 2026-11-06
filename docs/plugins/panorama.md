# panorama

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `PANO` | FILTER |
| `AVM` | FILTER |

`libmedia_panorama.so` 提供旧版兼容的 `PANO` 与 `AVM` 两个 FILTER type。插件只使用公开 SDK 和 OpenGL ES：多路 NV12 DMA-BUF 输入经过 2/4-word LUT 映射，可选双相机融合后输出 LINEAR NV12。输入端口为 `input0`…`inputN`，输出端口为 `output0`；输出 Pool 通过 `output_pool_id` 指定，容量应覆盖下游在途帧。

公共 API 位于 `include/media_panorama.h`，分别使用旧版的 `MEDIA_PANO_ATTR`、`MEDIA_AVM_ATTR` 和 `MEDIA_PANO_*`、`MEDIA_AVM_*` 函数。两个结构体的旧字段名、类型和顺序保持不变；`CreateGrpEx(attr_size)` 未覆盖的尾部字段按零处理。`PANO` 使用 `pto_path` 生成 LUT，`AVM` 使用 `lut_path` 读取预计算 LUT。

典型流程：

```c
MEDIA_PANO_ATTR attr = {
    .input_count = 4
    .in_width = 1280
    .in_height = 720
    .out_width = 1920
    .out_height = 1080
    .output_pool_id = 3
    .pto_path = "/etc/media/camera.pto"
}
MEDIA_PANO_CreateGrp(0, &attr)
MEDIA_SYS_Bind("VI", 0, "output0", "PANO", 0, "input0")
MEDIA_PANO_Start(0)
```

直接调用 `MEDIA_PANO_SendFrame` 或 `MEDIA_AVM_SendFrame` 时，线性 NV12 buffer 可以没有 metadata；插件会按对应输入配置补齐公开 layout。已带 metadata 的 buffer 必须与配置一致，AFBC 等压缩输入必须由上游提供真实 metadata。

输入格式固定为 NV12，尺寸必须与配置一致；输入可以使用 SDK 支持的 GLES AFBC，输出只支持 LINEAR NV12。`PANO` 遵循 GLES 的 64 字节线性 stride 约束；旧 `AVM` 的自定义线性 stride 仍保留，并按真实布局发布 metadata。LUT 支持每像素 2 个或 4 个 `uint32_t`，可用 `MEDIA_PANO_LUT_XY` 和 `MEDIA_PANO_LUT_INFO` 生成。任一路输入到达都可触发一次合成；`sync_wait_inputs` 为 1 时会在 `sync_timeout_ms` 内等待其它路，未到达的路使用最近帧。运行时可用 `MEDIA_PLUGIN_SetParam("PANO"/"AVM", id, "crop_xy", int[2], sizeof(int[2]))` 更新裁剪起点。

GPU program binary 在目标板构建期生成，不能跨 Mali 驱动复用。

## 在发布包中使用

```c
#include "media_panorama.h"
```

- 头文件：`include/media_panorama.h`
- 模块映像：`plugins/libmedia_panorama.so`（与 `libmedia_panorama.so.sig` 必须同目录）
- 链接：`-lmedia_panorama -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_panorama.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("PANO", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

