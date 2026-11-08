# vo 教程：显示连接器探测

程序调用 `MEDIA_VO_ProbeConnectors` 列出 DRM connector、连接状态和首选 mode。这一步是创建 VO 前必须做的参数检查；显示输出需要后续配置 DMA-BUF 帧和 connector。

```sh
cmake -S examples/vo -B build/vo && cmake --build build/vo
LD_LIBRARY_PATH=lib:plugins ./build/vo/vo_tutorial
```

没有 `/dev/dri/card0` 或 connector 时返回非零并说明原因。根据列出的名字填入 `MEDIA_VO_ATTR.connector`，再用 `MEDIA_VO_CreateChn` + `MEDIA_SYS_Bind` 接入 TPG/VPSS。
