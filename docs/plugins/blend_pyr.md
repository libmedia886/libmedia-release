# blend_pyr

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `BLEND_PYR` | FILTER |

两路 NV12 视频按 8bit mask 逐像素融合的 GLES 外部插件，产出 `libmedia_blend_pyr.so` 与
[`include/media_blend_pyr.h`](../../include/media_blend_pyr.h)。`MEDIA_BLEND_PYR_*` 的函数名、参数、
属性结构体名与字段顺序与旧内置模块（`master:include/media_api.h`）源码兼容；插件 type 名仍是
`BLEND_PYR`，实例名 `BLEND_PYR_<grp>`，应用侧 Bind 名不变。

融合公式（Y/U/V 各自按编码值线性混合，不做颜色矩阵转换）：

```
out = clamp(in1 + (in0 - in1) * mask.Y, 0, 255)
```

`mask.Y = 0` 时整图取 input1，`mask.Y = 255` 时整图取 input0，中间值线性插值。mask 只读 Y 平面
U/V 平面被忽略。

## 设计

- FILTER 插件、**自管模式**（`process = NULL`）：插件工作线程用 `MEDIA_PLUGIN_RecvFrame` 收 input0/input1
  合成后 `MEDIA_PLUGIN_EmitFrame` 输出，`MEDIA_PLUGIN_ReturnFrame` 归还输入。
- **GLES only**：`MEDIA_GPU_CreateContext / MakeCurrent / ImportBuffer / Finish / DestroyContext` 全部在该线程内
  两路 NV12 以 `__samplerExternal2DY2YEXT` 直接按编码值采样，输出走 `layout(yuv)` FBO，像素零拷贝
  EGLImage 导入由 core 按 (pool, index, 代次, layout) 缓存，稳态不重复导入。唯一的一次 CPU 写是 mask
  上传到 GL_R8 纹理，且只在 mask 版本变化时发生。
  用本机 Mali 驱动编成 program binary（`MEDIA_GPU_LoadProgramBinary`），binary 与生成的 `.c` 都不进 git。
- **隔离（quarantine）与强同步**：`MEDIA_GPU_Finish` 的 fence 超时后先补一次 `glFinish` 强同步，确认
  GPU 不再访问本帧的输入/输出，再销毁上下文并按原所有权归还 buffer（fail-stop，不再处理新帧）
  `strong` 统计项就是强同步次数。只有归还本身失败（`ReturnFrame` / `PutBuffer` 出错）的 buffer 才进入
  隔离队列，Stop/Destroy 时重试归还，确认前绝不把 buffer 交回 Pool。
- **mask 生命周期**：`MEDIA_BLEND_PYR_SetMask` / `input2` 端口收到的 buffer，插件内部 `MEDIA_BUFFER_Ref`
  自持一份引用，换 mask 或 Destroy 时释放；调用方仍按自己的引用计数 `MEDIA_POOL_PutBuffer`。
- **输出布局**：输出 buffer 的 meta 由插件按 `output_stride` 重新发布为 NV12 LINEAR（`pitch = output_stride`
  `offset1 = output_stride * height`，容量 `output_stride * height * 3 / 2`），时间戳与帧序号沿用 input0。
- 统计：`MEDIA_PLUGIN_RecordProcess` 记录处理耗时，`MEDIA_PLUGIN_ReportStats` 上报 GPU 耗时
  `MEDIA_SYS_GetPipelineStats` 的 `gpu_detail` 形如
  `gles mask=1 draw=120 upload=1 nomask=0 fail=0 strong=0`。

## 端口

| 端口 | 方向 | 格式 | 说明 |
| --- | --- | --- | --- |
| `input0` | 输入 | NV12 LINEAR | 第一路，融合权重 `mask.Y` 对应的 `in0`；缺省时输出也取本路 Pool |
| `input1` | 输入 | NV12 LINEAR | 第二路，权重 `1 - mask.Y` |
| `input2` | 输入 | NV12 LINEAR | 可选，mask 更新通道，只读 Y 平面，运行中可随时换 |
| `output0` | 输出 | NV12 LINEAR | 融合结果，`MEDIA_BLEND_PYR_GetFrame` / `GetFrame("output0")` 取帧 |

三路输入的 visible 尺寸必须等于 `width × height`，`pitch_bytes[0] >= width`（layout 必须通过
`MEDIA_VIDEO_ValidateLayout`）；只接受 `MEDIA_VIDEO_COMPRESS_NONE`（LINEAR），AFBC 等压缩布局和非
zero `drm_modifier` 一律拒收。mask 的有效区偏移（`valid_x` / `valid_y`）会被正确跳过。

端口名由 SDK 统一生成（`input%d` / `output%d`），因此旧模块的 mask 端口名 `mask_input` 在新插件里
叫 `input2`，绑定写法见下文示例。

## 配置字段

`MEDIA_BLEND_PYR_ATTR`（前 6 个字段与旧头文件完全一致，第 7 个是追加字段）：

| 字段 | 含义 |
| --- | --- |
| `width` | 图像宽（像素），16..8192 且 16 对齐；输入输出共用 |
| `height` | 图像高（像素），16..8192 且 2 对齐；输入输出共用 |
| `input_stride` | 输入 plane0 行字节数；0 = 按 `width` 向上 16 对齐，非 0 时须 >= width 且 16 对齐 |
| `input_depth` | input0/input1/input2 的 Bind 队列深度；0 = `MEDIA_BLEND_PYR_DEFAULT_DEPTH`(4)，范围 1..64 |
| `input_format` | 输入格式，只支持 `MEDIA_FORMAT_NV12`（输出同为 NV12） |
| `output_stride` | 输出 plane0 行字节数；0 = 等于 `input_stride`，非 0 时须 >= width 且 16 对齐 |
| `output_pool_id` | 【新增】输出 buffer 用的 Pool：>0 用指定 Pool（帧容量须 >= 一帧 NV12）；<=0 沿用旧行为，取 input0 所在 Pool |

非法组合在 `MEDIA_BLEND_PYR_SetAttr`（即 `MEDIA_PLUGIN_CreateModule`）阶段就被拒绝，不会留下半成品实例
`output_pool_id` 存在但容量不足同样返回 `MEDIA_ERR_INVALID_PARAM`。

## Pool 要求

- **输出 Pool**：`output_pool_id = 0` 时输出从 input0 所在 Pool 分配，该 Pool 至少要有一个额外空 buffer
  且 buffer 大小 >= `output_stride * height * 3 / 2`；否则请用 `output_pool_id` 指定专用输出 Pool。
- **mask Pool**：mask 要能被 CPU 映射（插件内 `MEDIA_POOL_BeginCpuAccess` + `GetVaddr` 上传到 GL_R8 纹理）
  建议 `MEDIA_POOL_ALLOC_MODE_NATIVE`；尺寸必须与融合组一致。
- 输入 buffer 由插件在工作线程内 `ReturnFrame`，输出由 `MEDIA_PLUGIN_EmitFrame` 交给获取方（手动 GetFrame 或下游模块）。
  调用方只需保证自己不再引用，不要重复归还。

## 使用示例

```c
#include "media_api.h"
#include "media_blend_pyr.h"

MEDIA_BLEND_PYR_ATTR attr = {
    .width = 1920
    .height = 1080
    .input_stride = 0,          /* 自动 16 字节对齐 */
    .input_depth = 4
    .input_format = MEDIA_FORMAT_NV12
    .output_stride = 0,         /* 跟随 input_stride */
    .output_pool_id = 0,        /* 0：沿用 input0 的 Pool */
}
/* 链接 libmedia_blend_pyr.so 后首次调用会自行注册插件，也可先 MEDIA_PLUGIN_Load(path)。 */
MEDIA_BLEND_PYR_SetAttr(0, &attr)
MEDIA_SYS_Bind("TPG", 0, "output0", MEDIA_BLEND_PYR_TYPE, 0, "input0")
MEDIA_SYS_Bind("TPG", 1, "output0", MEDIA_BLEND_PYR_TYPE, 0, "input1")
MEDIA_BLEND_PYR_SetMask(0, &mask_buf);   /* 或用 Bind/ SendFrame 走 "input2" */
MEDIA_BLEND_PYR_Enable(0)
if (MEDIA_BLEND_PYR_GetFrame(0, &out, 100) == MEDIA_STATUS_OK) {
    /* 使用 out ... */
    MEDIA_BLEND_PYR_ReleaseFrame(0, out)
}

MEDIA_BLEND_PYR_Disable(0)
MEDIA_BLEND_PYR_DestroyGrp(0)
```

运行中换 mask：`MEDIA_BLEND_PYR_SetMask(0, &new_mask)`，或
`MEDIA_SYS_SendFrame("BLEND_PYR", 0, "input2", new_mask, 1000)`；插件下一次处理前把新 mask 上传纹理
之前那份引用的 buffer 会被释放。

## 限制

- 只支持 NV12 LINEAR 输入输出，无 RGB / P010 / AFBC；旧模块的 OpenCL、Vulkan、NPU、CPU fallback 后端全部丢弃。
- 宽 16 对齐、高 2 对齐、stride 16 对齐；宽高 16..8192，队列深度 1..64。
- mask 只使用 Y 平面，不缩放：mask 尺寸必须与融合组一致，且必须 CPU 可映射。
- 未设置 mask 时不输出（输入对被丢弃），这是旧模块的行为；mask 一旦设置就一直有效，直到被替换。
- 融合只是逐像素线性加权，旧名字里的“金字塔”在旧实现中并没有多尺度分解，这里同样不做金字塔/带通分解。
- 应用侧若用旧版 6 字段 `MEDIA_BLEND_PYR_ATTR` 编译，需要重新编译以带上追加的 `output_pool_id`（源码级兼容，不是 ABI 兼容）。
- `MEDIA_BLEND_PYR_ReleaseFrame` 在模块已销毁时沿用旧实现的行为，直接把 buffer 归还其 Pool；模块存在时
  按 output0 释放。`MEDIA_BLEND_PYR_SetAttr` 的失败码统一由 SDK 折叠为 `MEDIA_ERR_RESOURCE`，不再区分
  旧实现的 `-1`（源码级仍可用 `< 0` / `!= 0` 判断失败）。

## 在发布包中使用

```c
#include "media_blend_pyr.h"
```

- 头文件：`include/media_blend_pyr.h`
- 模块映像：`plugins/libmedia_blend_pyr.so`（与 `libmedia_blend_pyr.so.sig` 必须同目录）
- 链接：`-lmedia_blend_pyr -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_blend_pyr.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("BLEND_PYR", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

