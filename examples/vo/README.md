# VO 入门：探测显示器和 DRM connector

VO（Video Output）负责把视频帧提交给 DRM/KMS 显示控制器。真正显示前，必须先知道板子有哪些 connector（HDMI、DP、MIPI-DSI 等）、是否连接显示器以及支持哪些 mode。本示例只做安全的探测，不创建 plane、不抢占桌面，因此适合第一次运行。

## 1. 编译和运行

```sh
cmake -S examples/vo -B build/vo
cmake --build build/vo -j2
LD_LIBRARY_PATH=lib:plugins ./build/vo/vo_tutorial
```

可能输出：

```text
VO connector count=1
HDMI-A-1 connected=1 mode=1920x1080@60
```

字段含义：`name` 是 connector 名称；`connected=1` 表示检测到显示器；`width/height/fps` 是首选 mode，不一定是所有可用 mode。程序没有输出文件；它的结果就是终端中的 connector 列表。

## 2. 从探测到真正显示

1. 先用 `modetest -M rockchip` 或 `drm_info` 查看 DRM 设备和 mode。
2. 在 `MEDIA_VO_ATTR.connector` 填入探测到的名称，并选择输出 `width/height/fps`。
3. 创建视频 Pool（通常是 `NATIVE` DMA-BUF），创建 `VO` channel，指定 plane 类型、format、stride 和 queue depth。
4. 把 `TPG/VI/VPSS` 的 output 绑定到 `VO` 的 input0：`MEDIA_SYS_Bind("TPG",0,"output0","VO",0,"input0")`。
5. `MEDIA_VO_Start` 后由 DRM plane commit 显示；正常显示路径不需要手动 `GetFrame`。

VO 的 `output_id` 是物理输出，`chn` 是该输出上的 plane/channel。不要把 connector 名称当作 channel 编号。

## 3. 常见参数

- `intf`：HDMI/DP/MIPI 等接口类型；通常由 connector 探测结果决定。
- `width/height`：显示 mode，不一定等于输入帧尺寸；缩放能力取决于 plane。
- `x/y`：plane 在 CRTC 上的位置；多个 channel 可拼成画中画。
- `plane_type`：主 plane、overlay 等；不同硬件支持的格式不同。
- `format`：常用 `NV12`、RGB 等，必须在 `MEDIA_VO_QueryChnCaps` 返回的能力表中。
- `stride/depth`：输入行步长和队列深度；stride 使用 layout 的 pitch。

## 4. 没有显示器怎么办

没有 connector 或 `/dev/dri/card0` 时，本示例返回非零，这是“没有可探测显示输出”，不是编译错误。可继续学习 TPG、VPSS、RGA 等不依赖 DRM 的例子；VO 真正显示需要有 DRM 设备、权限和活动 connector。

常见错误：

- `probe failed`：检查用户是否属于 `video`/`render` 组，确认 DRM 驱动已加载。
- `count=0`：HDMI 未插线、显示器休眠或设备树没有启用输出。
- 创建 channel 失败：请求的 format/尺寸不在 `MEDIA_VO_QueryChnCaps` 能力表中，或桌面 compositor 已占用 plane。

详见 [`docs/plugins/vo.md`](../../docs/plugins/vo.md)、[`include/media_vo.h`](../../include/media_vo.h)。
