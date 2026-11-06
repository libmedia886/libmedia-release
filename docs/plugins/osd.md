# osd

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `OSD` | FILTER |

`OSD`（type `OSD`）模块：在 GPU（OpenGL ES）上把最多 64 个 region 一次绘制叠加到输入帧。
`RECT` 与 `A8_MASK` 全在同一个 fragment shader 内完成，不做 CPU 回写。

由 libmedia core 原有实现独立而成
现在是一个独立 git 仓库，只依赖公开 SDK（`media_api.h` / `media_plugin.h`），不包含也不复制 core 内部头文件。

## 使用

实例名为 `OSD_<grp>`，端口 `input0` / `output0`，因此 `MEDIA_SYS_Bind("OSD", grp, "output0", ...)`
与迁移前的写法完全一致。

方式一，直接链接：

```c
#include "media_osd.h"

MEDIA_OSD_ATTR attr = {
    .input_width = 1920
    .input_height = 1080
    .format = MEDIA_FORMAT_NV12
    .input_depth = 4
    .output_pool_id = out_pool
    .max_regions = 8
}
MEDIA_OSD_CreateGrp(0, &attr)
MEDIA_OSD_UpdateRegion(0, 0, &region_attr);      /* enabled / x / y / w / h / global_alpha */
MEDIA_OSD_SetRegionRect(0, 0, &rect);            /* RECT 内容 */
MEDIA_OSD_SetRegionMask(0, 0, &mask);            /* A8_MASK 内容，可运行期反复更新 */
MEDIA_OSD_Start(0)
```

方式二，作为插件：`MEDIA_PLUGIN_Load("libmedia_osd.so")` 后
`MEDIA_PLUGIN_CreateModule("OSD", grp, &attr, sizeof(attr))`。

`MEDIA_SYS_Init` 按 `MEDIA_PLUGIN_PATH` 自动加载时无需显式 Load。

## 约束

- 输入/输出只支持 LINEAR（`input_compress` / `output_compress` 必须为 0）；压缩格式在 Create 时拒绝并打印原因。
- 输入输出格式：NV12、GRAY8、RGB888/BGR888、RGBA8888/BGRA8888/ARGB8888/ABGR8888
  Create 时还会用 `MEDIA_GPU_FormatSupported` 逐格式确认本机驱动可导入。
- region 叠加顺序按 `region_id` 递增（后设置的覆盖先设置的）；越界 region 按输出帧裁剪。
- `A8_MASK` 的 mask 像素与 region 像素一对一：`lx/ly` 落在 mask 的 `width/height` 之外时 alpha 视为 0
  （与迁移前语义相同，不做缩放）。
- mask 数据只在上传期间有效（`MEDIA_OSD_SetRegionMask` 返回后即可释放）。
- 语义与迁移前一致：只做叠加，不做文本排版、字体、图标解码。

## 在发布包中使用

```c
#include "media_osd.h"
```

- 头文件：`include/media_osd.h`
- 模块映像：`plugins/libmedia_osd.so`（与 `libmedia_osd.so.sig` 必须同目录）
- 链接：`-lmedia_osd -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_osd.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("OSD", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

