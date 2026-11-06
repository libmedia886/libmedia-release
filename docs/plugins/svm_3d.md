# svm_3d

[← 返回模块索引](../README.zh-CN.md)

| type 名 | 类型 |
| --- | --- |
| `SVM3D` | FILTER |

`libmedia_svm_3d.so` 是 SVM 3D 多路视频合成插件，使用公开 `media_api.h`、`media_plugin.h` 和 `MEDIA_GPU_*` GLES 接口。插件类型名为 `SVM3D`，实例名为 `SVM3D_<grp>`，输入端口为 `input0..inputN`，输出端口为 `output0`（可选 `output1`）。

当前 GLES 实现将多路输入导入为 EGLImage，在输出 FBO 上按透明度合成，并通过构建期生成的 GLES program binary 完成绘制。输入支持 NV12、RGB888、BGR888、RGBA8888、BGRA8888、ARGB8888、ABGR8888；输出支持四种 8888 RGBA 排列，使用 LINEAR 存储。输出尺寸、stride 和 metadata 来自配置及 `MEDIA_VIDEO_QueryLayout`。

配置字段如下：

- `input_count`：2 到 8 路输入。
- `input_format`、`in_width`、`in_height`、`in_stride`：输入端口格式和布局。
- `output_format`、`out_width`、`out_height`、`out_stride`：输出格式和布局。
- `output_pool_id`：output0 的 Pool；`top_output_enable` 非零时还需 `top_output_pool_id`。
- `input_depth`：输入队列深度，零使用框架默认值；`sync_timeout_ms` 控制多路收帧等待。
- `asset_path`：`svm_3d_asset_v1` 清单路径。清单中的 `cameras` 数量必须与输入路数一致；mesh 字段保留用于应用侧资产兼容。
- `sync_wait_inputs`：保留旧属性语义，非零等待每路输入。

示例：

```c
MEDIA_SVM3D_ATTR attr = {
    .input_count = 2
    .input_format = MEDIA_FORMAT_NV12
    .in_width = 1920
    .in_height = 1080
    .output_format = MEDIA_FORMAT_ABGR8888
    .out_width = 1280
    .out_height = 720
    .output_pool_id = 3
    .asset_path = "/opt/svm3d/asset.json"
}
MEDIA_SVM3D_CreateGrp(0, &attr)
MEDIA_SYS_Bind("VI", 0, "output0", "SVM3D", 0, "input0")
MEDIA_SYS_Bind("VI", 1, "output0", "SVM3D", 0, "input1")
MEDIA_SVM3D_Start(0)
```

输出 Pool 每个 buffer 必须至少容纳查询到的线性四通道帧大小，数量应覆盖下游队列和在途帧。插件不会修改输入 buffer；Stop/Destroy 后所有 RecvFrame 和输出 buffer 都归还。AFBC、10-bit 输出、CPU fallback、OpenCL、Vulkan 和 NPU 后端不支持。

独立构建：

```sh
cmake -S . -B /root/work/rktohi/wt/build/svm_3d -DBUILD_TESTING=ON -DMEDIA_SDK_DIR=/root/work/rktohi/wt/sdk
cmake --build /root/work/rktohi/wt/build/svm_3d -j2
cd /root/work/rktohi/wt/build/svm_3d
LD_LIBRARY_PATH=/root/work/rktohi/wt/sdk/lib ctest --output-on-failure
```

## 在发布包中使用

```c
#include "media_svm_3d.h"
```

- 头文件：`include/media_svm_3d.h`
- 模块映像：`plugins/libmedia_svm_3d.so`（与 `libmedia_svm_3d.so.sig` 必须同目录）
- 链接：`-lmedia_svm_3d -lmedia`

加载方式两种，二者等价：

```c
/* 1. 显式加载（失败会说明缺 .sig 还是签名不符） */
int ret = MEDIA_PLUGIN_Load("plugins/libmedia_svm_3d.so");
if (ret < 0) { /* 验签失败或映像不可用 */ }
```

```c
/* 2. 创建实例后由框架按需加载 */
MEDIA_PLUGIN_CreateModule("SVM3D", 0, &attr, sizeof(attr));
```

插件必须与本目录下的同名 `.sig` 一起分发；改动 `.so` 的任何字节都会导致加载被拒绝。

