# demosaic

[← 返回模块索引](../README.md)

| type 名 | 类型 |
| --- | --- |
| `DEMOSAIC` | FILTER |

`libmedia_demosaic.so` 提供旧 `DEMOSAIC` 模块的外部插件版本。模块 type 名为
`DEMOSAIC`，实例名为 `DEMOSAIC_<grp>`，输入端口是 `input0`，输出端口是
`output0`。输入为单平面 `MEDIA_FORMAT_BAYER_RAW16_LE`，输出为交错的
`MEDIA_FORMAT_RGB_U16`（每像素 R、G、B 三个 little-endian `uint16_t`）。当前实现支持
BGGR、GBRG、GRBG、RGGB 四种 Bayer 排列，并透传帧时间戳和序号。

配置结构体 `MEDIA_DEMOSAIC_ATTR` 的字段如下：

- `width`、`height`：输入和输出有效尺寸。
- `input_depth`：输入队列深度，0 使用默认值。
- `output_pool_id`：应用创建的输出 Pool。Pool 的每个 buffer 至少为
  `output_stride * height` 字节，数量应覆盖下游在途帧。
- `input_stride`：RAW16 每行字节数，0 使用布局默认值（公开 SDK 的 canonical stride）。
- `output_stride`：RGB_U16 每行字节数，0 使用 `width * 6`。
- `bayer_pattern`：`MEDIA_BAYER_PATTERN_*` CFA 排列。

示例：

```c
MEDIA_DEMOSAIC_ATTR attr = {
    .width = 1920
    .height = 1080
    .input_depth = 4
    .output_pool_id = 2
    .input_stride = 0
    .output_stride = 0
    .bayer_pattern = MEDIA_BAYER_PATTERN_RGGB
}
MEDIA_DEMOSAIC_Create(0, &attr)
MEDIA_DEMOSAIC_Start(0)
/* 通过 MEDIA_PLUGIN_SendFrame("DEMOSAIC", 0, 0, raw, timeout) 送入 RAW16。 */
MEDIA_DEMOSAIC_GetFrame(0, &out, 2000)
MEDIA_DEMOSAIC_ReleaseFrame(0, out)
MEDIA_DEMOSAIC_Stop(0)
MEDIA_DEMOSAIC_Destroy(0)
```

插件只依赖 SDK 的 `media_api.h`、`media_plugin.h` 和 libc。CMake 会用
生成的 `.c/.h` 不进入发布包。由于公开 SDK 尚未为 RGB_U16 提供 GLES DMA-BUF render target
实际 RAW16→RGB_U16 处理使用 CPU 访问路径该路径按 SDK
要求成对调用 `MEDIA_POOL_BeginCpuAccess/EndCpuAccess`，不会修改输入帧。

构建与测试：

```bash
cmake -S . -B /root/work/rktohi/wt/build/demosaic \
  -DBUILD_TESTING=ON -DMEDIA_SDK_DIR=/root/work/rktohi/wt/sdk
cmake --build /root/work/rktohi/wt/build/demosaic -j2
cd /root/work/rktohi/wt/build/demosaic
LD_LIBRARY_PATH=/root/work/rktohi/wt/sdk/lib ctest --output-on-failure
```

## 在发布包中使用

```c
#include "media_demosaic.h"
```

- 头文件：`include/media_demosaic.h`
- 模块映像：`plugins/libmedia_demosaic.so`（与 `libmedia_demosaic.so.sig` 必须同目录）
- 链接：`-lmedia_demosaic -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_demosaic.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("DEMOSAIC", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

