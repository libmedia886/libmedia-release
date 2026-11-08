# 16 路 VPSS 示例链接缺少 TPG 和 VPSS 插件

metadata:
- date: 2026-11-07 22:36:54
- kind: link
- status: fixed
- repository: /root/libmedia-release
- summary: 16 路示例只链接 core，调用插件 API 时链接失败。

## Symptom

链接器报告 MEDIA_TPG_CreateChn、MEDIA_VPSS_SetAttr 等符号 undefined reference。

## Reproduction

```bash
cmake -S examples/vpss_16out -B /tmp/libmedia-review-vpss_16out && cmake --build /tmp/libmedia-review-vpss_16out -j2
```

## Evidence

- undefined reference to MEDIA_TPG_CreateChn
- undefined reference to MEDIA_VPSS_SetAttr
- 修复后：frame 0: 16/16 outputs

## Affected Files

- examples/vpss_16out/CMakeLists.txt
- examples/vpss_16out/vpss_16out.c

## Root Cause

examples/vpss_16out/CMakeLists.txt 只链接 libmedia.so；TPG 和 VPSS 的模块 API 导出于各自插件库。

## Fix Or Workaround

把 libmedia_tpg.so 和 libmedia_vpss.so 作为 imported shared libraries 加入 target_link_libraries；重建成功，并在 RK3588 板上得到 16/16 路输出。

## Recurrence Prevention

示例直接调用插件的 MEDIA_<模块>_* API 时，先核对符号所属 .so，再检查 CMake 的链接项；编译所有公开示例。

## Related

- None
