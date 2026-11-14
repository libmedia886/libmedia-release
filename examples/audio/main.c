/*
 * AUDIO 入门教程：把内存里生成的一段 PCM 编成 G.711 A-law 码流
 *
 * 本程序不需要声卡，也不读麦克风：自己在内存里造一小段 8 kHz / 16 bit / 单声道
 * 三角波 PCM，送进 AUDIO_ENC，取出一帧压缩码流写成裸文件。
 *
 * 本例故意不走 MEDIA_SYS_Bind：音频只有“送一帧、收一帧”两个动作，
 * 手动 SendFrame / GetStream 更直观。视频例子里的 Bind 是为了让模块
 * 在后台自动流转，音频这里不需要。
 *
 * 阅读顺序：
 *   [1] 算 Pool 大小 -> [2] 建 Pool -> [3] CreateChn -> Start
 *   -> [4] 造 PCM 并 SendFrame -> [5] GetStream / 写文件 / ReleaseStream
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_audio.h"

/* ---- 本次实验的固定参数 ------------------------------------------------ */
#define RATE 8000    /* 采样率：电话语音常用 8 kHz */
#define SAMPLES 160  /* 每帧样本数：160 / 8000 = 20 ms */
#define POOL 1       /* 音频帧和码流共用一个 Pool */

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "audio_g711a.bin";
    MEDIA_AUDIO_ENC_ATTR a;
    MEDIA_AUDIO_FRAME fr;
    MEDIA_AUDIO_STREAM st;
    int made = 0, run = 0, rc = 1;
    size_t fs, ss;

    /* [1] 音频的 Pool 大小不能拍脑袋：帧池和码流池各有自己的计算公式，
     *     两个算出来可能不一样大，所以取较大值建一个 Pool 共用。 */
    fs = MEDIA_AUDIO_CalcFramePoolSize(RATE, 16, MEDIA_AUDIO_SOUND_MODE_MONO, SAMPLES);
    ss = MEDIA_AUDIO_CalcStreamPoolSize(MEDIA_AUDIO_CODEC_G711A, RATE, 16,
                                        MEDIA_AUDIO_SOUND_MODE_MONO, SAMPLES, 0);

    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [2] 音频用 HOST 内存（CPU 访问），不需要 NATIVE。 */
    if (MEDIA_POOL_CreateWithMode(POOL, fs > ss ? fs : ss, 8, MEDIA_POOL_ALLOC_MODE_HOST) != 0) {
        fprintf(stderr, "audio pool setup failed\n");
        goto done;
    }

    /* [3] 编码通道参数：编码格式、采样率、位宽、声道、每帧点数、输出 Pool。 */
    memset(&a, 0, sizeof(a));
    a.codec = MEDIA_AUDIO_CODEC_G711A;
    a.sample_rate = RATE;
    a.bit_width = 16;
    a.snd_mode = MEDIA_AUDIO_SOUND_MODE_MONO;
    a.point_num_per_frame = SAMPLES;
    a.output_pool_id = POOL;
    if (MEDIA_AUDIO_ENC_CreateChn(0, &a) != 0) {
        fprintf(stderr, "AUDIO_ENC create failed\n");
        goto cleanup;
    }
    made = 1;

    if (MEDIA_AUDIO_ENC_Start(0) != 0)
        goto cleanup;
    run = 1;

    /* [4] 造 PCM：一个三角波，前半个周期上升、后半个周期下降，
     *     峰值 SAMPLES/2 * 500 ≈ 40000，在 int16 范围内。
     *     注意 PCM 是小端存放：低字节在前、高字节在后。 */
    {
        uint8_t pcm[SAMPLES * 2];

        for (int i = 0; i < SAMPLES; i++) {
            int16_t v = (int16_t)((i < SAMPLES / 2 ? i : SAMPLES - i) * 500);

            pcm[2 * i] = (uint8_t)v;
            pcm[2 * i + 1] = (uint8_t)(v >> 8);
        }

        memset(&fr, 0, sizeof(fr));
        fr.virt_addr[0] = pcm;
        fr.len = sizeof(pcm);
        fr.sample_rate = RATE;
        fr.bit_width = 16;
        fr.snd_mode = MEDIA_AUDIO_SOUND_MODE_MONO;
        fr.point_num_per_frame = SAMPLES;

        if (MEDIA_AUDIO_ENC_SendFrame(0, &fr, 1000) != 0 ||
            MEDIA_AUDIO_ENC_GetStream(0, &st, 3000) != 0) {
            fprintf(stderr, "AUDIO_ENC stream timeout\n");
            goto cleanup;
        }
    }

    /* [5] 取到码流后写入文件。st.len 是有效字节数，
     *     G.711 A-law 每样本 1 字节，所以通常 ≈ SAMPLES 字节。 */
    {
        FILE *f = fopen(out, "wb");
        uint32_t out_len = st.len;

        if (!f || fwrite(st.stream, 1, out_len, f) != out_len) {
            if (f)
                fclose(f);
            MEDIA_AUDIO_ENC_ReleaseStream(0, &st);
            goto cleanup;
        }
        fclose(f);
        MEDIA_AUDIO_ENC_ReleaseStream(0, &st);
        printf("AUDIO_ENC G711A %d Hz mono %d samples -> %u bytes %s\n", RATE, SAMPLES,
               out_len, out);
        rc = 0;
    }

cleanup:
    if (run)
        MEDIA_AUDIO_ENC_Stop(0);
    if (made)
        MEDIA_AUDIO_ENC_DestroyChn(0);
    MEDIA_POOL_Destroy(POOL);

done:
    MEDIA_SYS_Exit();
    return rc;
}
