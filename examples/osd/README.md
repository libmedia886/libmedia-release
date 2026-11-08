# osd 教程：NV12 + RECT region

程序将 COLOR_BARS TPG 连接到 OSD，在 `(40,30)` 绘制 240×120 半透明红色矩形，输出 NV12。

```sh
cmake -S examples/osd -B build/osd && cmake --build build/osd
LD_LIBRARY_PATH=lib:plugins ./build/osd/osd_tutorial [out.nv12]
```

修改 `MEDIA_OSD_REGION_ATTR` 的位置、尺寸、alpha 或 `MEDIA_OSD_RECT_DESC` 的颜色；再调用 `SetRegionMask` 可改成 A8 mask。
