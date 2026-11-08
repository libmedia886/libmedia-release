# RGB_U16 没有 canonical layout

metadata:
- date: 2026-11-08 00:07:54
- kind: validation
- status: resolved
- repository: /root/libmedia-release
- summary: MEDIA_VIDEO_BuildLinearLayout 不支持 RGB_U16

## Symptom

对 MEDIA_FORMAT_RGB_U16 调用 MEDIA_VIDEO_BuildLinearLayout 返回 -1，输出 stride 和 size 未填充。

## Reproduction

```bash
/tmp/check_layout
```

## Evidence

- raw layout r=0 s=6144 pitch=128
- rgb layout r=-1 s=0 pitch=0

## Affected Files

- examples/demosaic/main.c
- examples/demosaic/README.md

## Root Cause

当前公开 SDK 的 canonical layout helper 没有实现 RGB_U16；DEMOSAIC 文档要求应用按 width*6 和显式 stride 配置输出。

## Fix Or Workaround

DEMOSAIC 教程只用 helper 计算 RAW16 输入 layout，输出 pool 按 OUT_STRIDE*H 创建。

## Recurrence Prevention

增加新格式教程前先检查 helper 返回值；对 SDK 未支持的输出格式使用模块文档规定的显式 stride，并在 README 说明。

## Related

- None
