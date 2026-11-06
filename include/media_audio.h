#ifndef MEDIA_AUDIO_H
#define MEDIA_AUDIO_H

/* 音频外部模块公共 API；通用帧、码流和 metadata 类型见 media_api.h。 */
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif
#if defined(__GNUC__)
#define MEDIA_AUDIO_EXPORT __attribute__((visibility("default")))
#else
#define MEDIA_AUDIO_EXPORT
#endif

/* dev 取 0..255，chn 取 0..255；dev0 保持 id=chn。 */
#define MEDIA_AUDIO_ID(dev, chn) ((dev) * 256 + (chn))
#define MEDIA_AUDIO_OUTPUT_POOL_ID_UNSET 0

typedef struct {
    const char *device;          // NULL -> plughw:0,0
    int sample_rate;             // 0 -> 48000
    int bit_width;               // 0 -> 16
    int snd_mode;                // 0 -> stereo
    int frame_num;               // 0 -> module default
    int point_num_per_frame;     // 0 -> module default
    int output_pool_id;          // required external HOST pool id (>0; 0=unset)
} MEDIA_AUDIO_IN_ATTR;

typedef struct {
    const char *device;          // NULL -> plughw:0,0
    int sample_rate;             // 0 -> 48000
    int bit_width;               // 0 -> 16
    int snd_mode;                // 0 -> stereo
    int frame_num;               // 0 -> module default
    int point_num_per_frame;     // 0 -> module default
} MEDIA_AUDIO_OUT_ATTR;

typedef struct {
    int codec;                   // MEDIA_AUDIO_CODEC_*
    int sample_rate;             // 0 -> 48000
    int bit_width;               // 0 -> 16
    int snd_mode;                // 0 -> stereo
    int frame_num;               // 0 -> module default
    int point_num_per_frame;     // G711: 80/160/240/320/480, AAC: 1024
    int bit_rate;                // AAC only, 0 -> 128000
    int transport_type;          // MEDIA_AUDIO_AAC_TRANSPORT_*
    int output_pool_id;          // required external HOST stream pool id (>0; 0=unset)
} MEDIA_AUDIO_ENC_ATTR;

typedef struct {
    int codec;                   // MEDIA_AUDIO_CODEC_*
    int sample_rate;             // 0 -> 48000
    int bit_width;               // 0 -> 16
    int snd_mode;                // 0 -> stereo
    int frame_num;               // 0 -> module default
    int point_num_per_frame;     // G711: 80/160/240/320/480, AAC: 1024
    int transport_type;          // MEDIA_AUDIO_AAC_TRANSPORT_*
    int output_pool_id;          // required external HOST frame pool id (>0; 0=unset)
} MEDIA_AUDIO_DEC_ATTR;

// AUDIO_IN (ALSA capture)
MEDIA_AUDIO_EXPORT size_t MEDIA_AUDIO_CalcFramePoolSize(int sample_rate, int bit_width, int snd_mode, int point_num_per_frame);
MEDIA_AUDIO_EXPORT size_t MEDIA_AUDIO_CalcStreamPoolSize(int codec, int sample_rate, int bit_width, int snd_mode, int point_num_per_frame, int bit_rate);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_CreateChn(int dev, int chn, const MEDIA_AUDIO_IN_ATTR *attr);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_DestroyChn(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_Start(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_Stop(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_Enable(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_Disable(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_GetFrame(int dev, int chn, MEDIA_AUDIO_FRAME *frame, int timeout_ms);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_IN_ReleaseFrame(int dev, int chn, MEDIA_AUDIO_FRAME *frame);

// AUDIO_OUT (ALSA playback)
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_CreateChn(int dev, int chn, const MEDIA_AUDIO_OUT_ATTR *attr);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_DestroyChn(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_Start(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_Stop(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_Enable(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_Disable(int dev, int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_OUT_SendFrame(int dev, int chn, const MEDIA_AUDIO_FRAME *frame, int timeout_ms);

// AUDIO_ENC (software AENC)
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_CreateChn(int chn, const MEDIA_AUDIO_ENC_ATTR *attr);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_DestroyChn(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_Start(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_Stop(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_Enable(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_Disable(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_SendFrame(int chn, const MEDIA_AUDIO_FRAME *frame, int timeout_ms);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_GetStream(int chn, MEDIA_AUDIO_STREAM *stream, int timeout_ms);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_ENC_ReleaseStream(int chn, MEDIA_AUDIO_STREAM *stream);

// AUDIO_DEC (software ADEC)
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_CreateChn(int chn, const MEDIA_AUDIO_DEC_ATTR *attr);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_DestroyChn(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_Start(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_Stop(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_Enable(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_Disable(int chn);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_SendStream(int chn, const MEDIA_AUDIO_STREAM *stream, int timeout_ms);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_GetFrame(int chn, MEDIA_AUDIO_FRAME *frame, int timeout_ms);
MEDIA_AUDIO_EXPORT int MEDIA_AUDIO_DEC_ReleaseFrame(int chn, MEDIA_AUDIO_FRAME *frame);

#ifdef __cplusplus
}
#endif
#endif
