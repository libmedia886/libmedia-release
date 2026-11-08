# 模块索引

[English](README.md) | **简体中文**

[👈 返回发布包首页](../README.zh-CN.md)

发布包内的 37 个外部模块。每个模块的文档说明其 type 名、端口、配置字段、使用示例与已知限制。

可运行教程在 [`examples/`](../examples/README.md) 下。每个模块都有自己的 README 和 CMake 工程；采集、TPG、VPSS 教程会完整跑一条帧管线，其余模块教程先验证签名、type、ABI 和动态依赖，再按模块页面接入完整管线。

| 模块 | type 名 | 类型 | 说明 |
| --- | --- | --- | --- |
| [vi](plugins/vi.md) | `VI` | SOURCE | V4L2 / HDMI-RX 视频采集 |
| [tpg](plugins/tpg.md) | `TPG` | SOURCE | 测试图案源（写外部模块的参考实现） |
| [vdec](plugins/vdec.md) | `VDEC` | SOURCE / FILTER | MPP 硬件视频解码（H.264 / H.265 / MJPEG） |
| [audio](plugins/audio.md) | `AUDIO_IN` `AUDIO_OUT` `AUDIO_ENC` `AUDIO_DEC` | SOURCE / SINK / FILTER | ALSA 音频输入输出与 AAC 编解码 |
| [vpss](plugins/vpss.md) | `VPSS` | FILTER | GLES 裁剪 / 缩放 / 旋转 / 色彩空间，最多 16 路输出 |
| [vmix](plugins/vmix.md) | `VMIX` | FILTER | GLES 多路视频混合拼接，最多 16 路输入 |
| [vmix_rga](plugins/vmix_rga.md) | `VMIX_RGA` | FILTER | RGA 硬件多路合成 |
| [osd](plugins/osd.md) | `OSD` | FILTER | GPU 图形叠加，最多 64 个 region |
| [vo](plugins/vo.md) | `VO` | SINK | DRM/KMS 零拷贝显示输出 |
| [vo_wbc](plugins/vo_wbc.md) | `VO_WBC` | SOURCE | DRM writeback 回写抓屏 |
| [venc](plugins/venc.md) | `VENC` | FILTER | MPP 硬件视频编码（H.264 / H.265） |
| [rga](plugins/rga.md) | `RGA` | FILTER | RGA 拷贝 / 缩放 / 旋转 / 翻转 / 合成 |
| [csc_rga](plugins/csc_rga.md) | `CSC_RGA` | FILTER | RGA 色彩空间 / 像素格式转换 |
| [resize_rga](plugins/resize_rga.md) | `RESIZE_RGA` | FILTER | RGA 裁剪与缩放 |
| [transform](plugins/transform.md) | `TRANSFORM` | FILTER | LUT 图像变换 |
| [dualview](plugins/dualview.md) | `DUALVIEW` | FILTER | 双路 RGB888 合成 |
| [stereo_3d](plugins/stereo_3d.md) | `STEREO_3D` | FILTER | 双目合成（并排 / 隔行） |
| [svm_3d](plugins/svm_3d.md) | `SVM3D` | FILTER | 多路视频合成 |
| [panorama](plugins/panorama.md) | `PANO` `AVM` | FILTER | 全景拼接与四路鸟瞰 |
| [mcf_fusion](plugins/mcf_fusion.md) | `MCF_FUSION_CL` | FILTER | 单色 / 彩色图像融合 |
| [thermal](plugins/thermal.md) | `THERMAL` | FILTER | 热成像伪彩色 |
| [thermal_lowlight_fusion](plugins/thermal_lowlight_fusion.md) | `THERMAL_LOWLIGHT_FUSION_CL` | FILTER | 热成像 + 低照度融合 |
| [fluo_visible_fusion](plugins/fluo_visible_fusion.md) | `FLUO_VISIBLE_FUSION_CL` | FILTER | 荧光 + 可见光融合 |
| [exposure_fusion](plugins/exposure_fusion.md) | `EXPOSURE_FUSION_CL` | FILTER | 双路曝光融合 |
| [blend_pyr](plugins/blend_pyr.md) | `BLEND_PYR` | FILTER | 金字塔掩膜融合 |
| [edof](plugins/edof.md) | `EDOF_CL` | FILTER | 扩展景深双路融合 |
| [eis](plugins/eis.md) | `EIS` | FILTER | 电子防抖 |
| [tnr](plugins/tnr.md) | `TNR_CL` | FILTER | 时域降噪 |
| [wavelet_nr](plugins/wavelet_nr.md) | `WAVELET_NR_CL` | FILTER | 小波空间降噪 |
| [clahe](plugins/clahe.md) | `CLAHE` | FILTER | 局部对比度增强 |
| [retinex](plugins/retinex.md) | `RETINEX` | FILTER | Retinex 图像增强 |
| [highlight_suppress](plugins/highlight_suppress.md) | `HIGHLIGHT_SUPPRESS` | FILTER | 高光软压制 |
| [cap_dehaze](plugins/cap_dehaze.md) | `CAP_DEHAZE` | FILTER | CAP 先验单帧去雾 |
| [dcp_fast_dehaze](plugins/dcp_fast_dehaze.md) | `DCP_FAST_DEHAZE` | FILTER | 暗通道快速去雾 |
| [demosaic](plugins/demosaic.md) | `DEMOSAIC` | FILTER | Bayer 去马赛克 |
| [conv](plugins/conv.md) | `CONV_CL` | FILTER | 二维卷积，最大 31×31 |
| [raw_wb](plugins/raw_wb.md) | `RAW_WB` | FILTER | RAW16_LE Bayer 手动白平衡 |

## 加载与验签

显式加载通过 `MEDIA_PLUGIN_Load()` 验签；直接链接模块库则通过 `MEDIA_PLUGIN_LoadSelf()` 在注册时验签。签名文件 `plugins/<模块>.so.sig` 必须与 `.so` 同目录分发。
缺签名、内容被改动或密钥不符都会被拒绝加载或注册。详见[发布包首页](../README.zh-CN.md#签名校验)。

单个进程同时最多持有 `MEDIA_PLUGIN_MAX_LOADED`（32）个插件。37 个模块不要求同时全部加载，
应用只加载自己实际用到的模块即可。
