# Module index

**English** | [简体中文](README.zh-CN.md)

[👈 Back to release index](../README.md)

The 37 external modules shipped in this release. Each module page documents its type name, kind, ports, configuration fields, usage and known limits.

| Module | Type name | Kind | Purpose |
| --- | --- | --- | --- |
| [vi](plugins/vi.md) | `VI` | SOURCE | V4L2 / HDMI-RX video capture |
| [tpg](plugins/tpg.md) | `TPG` | SOURCE | Test pattern generator (reference plugin) |
| [vdec](plugins/vdec.md) | `VDEC` | SOURCE / FILTER | MPP hardware video decode (H.264 / H.265 / MJPEG) |
| [audio](plugins/audio.md) | `AUDIO_IN` `AUDIO_OUT` `AUDIO_ENC` `AUDIO_DEC` | SOURCE / SINK / FILTER | ALSA audio I/O and AAC codec |
| [vpss](plugins/vpss.md) | `VPSS` | FILTER | GLES crop / scale / rotate / color space, up to 16 outputs |
| [vmix](plugins/vmix.md) | `VMIX` | FILTER | GLES multi-input blend, up to 16 inputs |
| [vmix_rga](plugins/vmix_rga.md) | `VMIX_RGA` | FILTER | RGA hardware compositing |
| [osd](plugins/osd.md) | `OSD` | FILTER | GPU graphics overlay, up to 64 regions |
| [vo](plugins/vo.md) | `VO` | SINK | DRM/KMS zero-copy display output |
| [vo_wbc](plugins/vo_wbc.md) | `VO_WBC` | SOURCE | DRM writeback capture |
| [venc](plugins/venc.md) | `VENC` | FILTER | MPP hardware video encode (H.264 / H.265) |
| [rga](plugins/rga.md) | `RGA` | FILTER | RGA copy / scale / rotate / flip / compose |
| [csc_rga](plugins/csc_rga.md) | `CSC_RGA` | FILTER | RGA color space / pixel format conversion |
| [resize_rga](plugins/resize_rga.md) | `RESIZE_RGA` | FILTER | RGA crop and scale |
| [transform](plugins/transform.md) | `TRANSFORM` | FILTER | LUT image transform |
| [dualview](plugins/dualview.md) | `DUALVIEW` | FILTER | Dual RGB888 compositing |
| [stereo_3d](plugins/stereo_3d.md) | `STEREO_3D` | FILTER | Stereo compositing (side-by-side / line-interleaved) |
| [svm_3d](plugins/svm_3d.md) | `SVM3D` | FILTER | Multi-input video compositing |
| [panorama](plugins/panorama.md) | `PANO` `AVM` | FILTER | Panorama stitch and four-camera bird's-eye view |
| [mcf_fusion](plugins/mcf_fusion.md) | `MCF_FUSION_CL` | FILTER | Mono / color image fusion |
| [thermal](plugins/thermal.md) | `THERMAL` | FILTER | Thermal false-color mapping |
| [thermal_lowlight_fusion](plugins/thermal_lowlight_fusion.md) | `THERMAL_LOWLIGHT_FUSION_CL` | FILTER | Thermal + low-light fusion |
| [fluo_visible_fusion](plugins/fluo_visible_fusion.md) | `FLUO_VISIBLE_FUSION_CL` | FILTER | Fluorescence + visible fusion |
| [exposure_fusion](plugins/exposure_fusion.md) | `EXPOSURE_FUSION_CL` | FILTER | Dual-exposure fusion |
| [blend_pyr](plugins/blend_pyr.md) | `BLEND_PYR` | FILTER | Pyramid mask blending |
| [edof](plugins/edof.md) | `EDOF_CL` | FILTER | Extended depth of field fusion |
| [eis](plugins/eis.md) | `EIS` | FILTER | Electronic image stabilization |
| [tnr](plugins/tnr.md) | `TNR_CL` | FILTER | Temporal noise reduction |
| [wavelet_nr](plugins/wavelet_nr.md) | `WAVELET_NR_CL` | FILTER | Wavelet spatial noise reduction |
| [clahe](plugins/clahe.md) | `CLAHE` | FILTER | Local contrast enhancement |
| [retinex](plugins/retinex.md) | `RETINEX` | FILTER | Retinex image enhancement |
| [highlight_suppress](plugins/highlight_suppress.md) | `HIGHLIGHT_SUPPRESS` | FILTER | Highlight soft clipping |
| [cap_dehaze](plugins/cap_dehaze.md) | `CAP_DEHAZE` | FILTER | CAP-prior single-frame dehaze |
| [dcp_fast_dehaze](plugins/dcp_fast_dehaze.md) | `DCP_FAST_DEHAZE` | FILTER | Dark-channel fast dehaze |
| [demosaic](plugins/demosaic.md) | `DEMOSAIC` | FILTER | Bayer demosaic |
| [conv](plugins/conv.md) | `CONV_CL` | FILTER | 2D convolution, up to 31×31 |
| [raw_wb](plugins/raw_wb.md) | `RAW_WB` | FILTER | RAW16_LE Bayer manual white balance |

## Loading and verification

Every module is verified through `MEDIA_PLUGIN_LoadSelf()`; the signature file `plugins/<module>.so.sig` must be distributed next to the `.so`. A plugin is rejected if the signature is missing, the plugin has been modified, or the key does not match. See [signature verification](../README.md#signature-verification).

A single process holds at most `MEDIA_PLUGIN_MAX_LOADED` (32) plugins. The 37 modules are not meant to be loaded all at once — an application should load only the modules it uses.
