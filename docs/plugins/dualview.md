# dualview

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `DUALVIEW` | FILTER |

`libmedia_dualview.so` 将两路 `RGB888` 线性 DMA-BUF 合成为一路 `RGB888` 线性输出，使用
OpenGL ES 3 fragment shader 完成缩放和写入。插件 type 名是 `DUALVIEW`，端口固定为
`input0`、`input1` 和 `output0`。

支持两种模式：`MEDIA_DUALVIEW_MODE_SIDE_BY_SIDE` 把两路放在同一画布中，区域可用
`inputs[i].x/y/width/height` 指定；`MEDIA_DUALVIEW_MODE_LINE_BY_LINE` 的偶数行来自 input0
奇数行来自 input1。某路禁用后，合成画面复用另一输入的最新帧。没有显式填写 `inputs` 时两路
都启用。输入输出格式当前固定为 `MEDIA_FORMAT_RGB888`，压缩固定为 LINEAR，输出尺寸和 pitch
必须与 `MEDIA_VIDEO_QueryLayout` 的结果一致。

属性字段如下：

| 字段 | 含义 |
| --- | --- |
| `inputs[2]` | 两路启用状态、位置、视图尺寸和输入 pitch |
| `input_width/height` | 两路输入有效尺寸 |
| `input_stride` | 输入 plane0 pitch；0 使用 SDK canonical pitch |
| `output_width/height` | 输出尺寸；<=0 按模式自动计算 |
| `output_stride` | 输出 plane0 pitch；0 使用 SDK canonical pitch |
| `mode` | `SIDE_BY_SIDE` 或 `LINE_BY_LINE` |
| `format` | 只能是 `MEDIA_FORMAT_RGB888` |
| `input_depth` | 每个输入队列深度；<=0 为 4 |
| `output_pool_id` | 应用创建的输出 Pool ID |

应用需先 `MEDIA_SYS_Init`，为每路输入和输出创建足够大的 Pool，再创建 TPG/采集源和
插件。例如：

```c
MEDIA_DUALVIEW_ATTR attr = {0}
attr.input_width = 1920
attr.input_height = 1080
attr.output_width = 3840
attr.output_height = 1080
attr.format = MEDIA_FORMAT_RGB888
attr.output_pool_id = output_pool
attr.mode = MEDIA_DUALVIEW_MODE_SIDE_BY_SIDE
MEDIA_DUALVIEW_CreateGrp(0, &attr)
MEDIA_SYS_Bind("CAM0", 0, "output0", "DUALVIEW", 0, "input0")
MEDIA_SYS_Bind("CAM1", 0, "output0", "DUALVIEW", 0, "input1")
MEDIA_DUALVIEW_Start(0)
```

输出 Pool 的 slot 数应覆盖下游队列和在途帧；测试或手动取帧得到的输出必须调用
`MEDIA_DUALVIEW_ReleaseFrame`。插件只读输入 buffer，输出 metadata 会透传第一路输入的
驱动相关 program binary，因此应在目标板或完全相同的 Mali 驱动环境中构建。

限制：当前只接受 RGB888、LINEAR 和两路相同的输入尺寸；不提供 YUV、AFBC、CPU fallback、
Vulkan/OpenCL/NPU 路径，也不做跨时间戳同步。程序二进制和生成的 C 文件只存在于构建目录。

构建和测试：

```bash
cmake -S . -B /root/work/rktohi/wt/build/dualview \
  -DBUILD_TESTING=ON -DMEDIA_SDK_DIR=/root/work/rktohi/wt/sdk
cmake --build /root/work/rktohi/wt/build/dualview -j2
cd /root/work/rktohi/wt/build/dualview
LD_LIBRARY_PATH=/root/work/rktohi/wt/sdk/lib ctest --output-on-failure
```

## 在发布包中使用

```c
#include "media_dualview.h"
```

- 头文件：`include/media_dualview.h`
- 模块映像：`plugins/libmedia_dualview.so`（与 `libmedia_dualview.so.sig` 必须同目录）
- 链接：`-lmedia_dualview -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_dualview.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("DUALVIEW", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

