# VPSS 16 路输出教程

这个示例演示一帧输入经过 `TPG -> VPSS` 后，生成 16 路独立输出。每路可以单独配置旋转、水平/垂直翻转和保宽高比留边，程序再把 16 路 NV12 转成一张 RGB24 4x4 画布。

## 编译和运行

在发布包根目录执行：

```sh
cmake -S examples/vpss_16out -B build/vpss_16out
cmake --build build/vpss_16out -j2
mkdir -p frames
LD_LIBRARY_PATH=plugins:lib ./build/vpss_16out/vpss_16out frames 1 0
```

参数：

```text
./vpss_16out <输出目录> [帧数] [间隔毫秒] [源]
```

- `源` 省略时使用 TPG 色条；
- `源=img:<文件>` 时读取 1920x1080 原始 RGB24 文件并手动送入 VPSS；
- 输出为 `frame_000.rgb` 等 RGB24 文件，尺寸和布局写入 `info.txt`。

## 关键参数

- 输入：1920x1080、NV12、LINEAR、BT.601 limited。
- 输出：16 路，每路 480x270、NV12、LINEAR、独立 Pool。
- 后端：`MEDIA_VPSS_BACKEND_GLES`。
- Pool：使用 `MEDIA_POOL_ALLOC_MODE_NATIVE`，每路 4 个 buffer。

## 代码流程

1. 创建输入和 16 个输出 Pool。
2. 创建 TPG（默认源）和 VPSS group。
3. 为每个输出设置 crop、rotate、flip、keep-aspect 和 output pool。
4. Bind TPG 到 VPSS，或把 RGB24 转成 NV12 后调用 `MEDIA_SYS_SendFrame()`。
5. 用 `MEDIA_SYS_GetFrame()` 逐路取出并转换为 RGB24。
6. Disable、UnBind、Destroy，释放所有 Pool。
