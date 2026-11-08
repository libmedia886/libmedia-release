#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_audio.h"
#define RATE 8000
#define SAMPLES 160
#define POOL 1
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"audio_g711a.bin";MEDIA_AUDIO_ENC_ATTR a;MEDIA_AUDIO_FRAME fr;MEDIA_AUDIO_STREAM st;int made=0,run=0,rc=1;size_t fs=MEDIA_AUDIO_CalcFramePoolSize(RATE,16,MEDIA_AUDIO_SOUND_MODE_MONO,SAMPLES);size_t ss=MEDIA_AUDIO_CalcStreamPoolSize(MEDIA_AUDIO_CODEC_G711A,RATE,16,MEDIA_AUDIO_SOUND_MODE_MONO,SAMPLES,0);if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_POOL_CreateWithMode(POOL,fs>ss?fs:ss,8,MEDIA_POOL_ALLOC_MODE_HOST)!=0){fprintf(stderr,"audio pool setup failed\n");goto done;}memset(&a,0,sizeof(a));a.codec=MEDIA_AUDIO_CODEC_G711A;a.sample_rate=RATE;a.bit_width=16;a.snd_mode=MEDIA_AUDIO_SOUND_MODE_MONO;a.point_num_per_frame=SAMPLES;a.output_pool_id=POOL;if(MEDIA_AUDIO_ENC_CreateChn(0,&a)!=0){fprintf(stderr,"AUDIO_ENC create failed\n");goto cleanup;}made=1;if(MEDIA_AUDIO_ENC_Start(0)!=0)goto cleanup;run=1;uint8_t pcm[SAMPLES*2];for(int i=0;i<SAMPLES;i++){int16_t v=(int16_t)((i< SAMPLES/2?i:SAMPLES-i)*500);pcm[2*i]=(uint8_t)v;pcm[2*i+1]=(uint8_t)(v>>8);}memset(&fr,0,sizeof(fr));fr.virt_addr[0]=pcm;fr.len=sizeof(pcm);fr.sample_rate=RATE;fr.bit_width=16;fr.snd_mode=MEDIA_AUDIO_SOUND_MODE_MONO;fr.point_num_per_frame=SAMPLES;if(MEDIA_AUDIO_ENC_SendFrame(0,&fr,1000)!=0||MEDIA_AUDIO_ENC_GetStream(0,&st,3000)!=0){fprintf(stderr,"AUDIO_ENC stream timeout\n");goto cleanup;}uint32_t out_len=st.len;FILE*f=fopen(out,"wb");if(!f||fwrite(st.stream,1,out_len,f)!=out_len){if(f)fclose(f);MEDIA_AUDIO_ENC_ReleaseStream(0,&st);goto cleanup;}fclose(f);MEDIA_AUDIO_ENC_ReleaseStream(0,&st);printf("AUDIO_ENC G711A %d Hz mono %d samples -> %u bytes %s\n",RATE,SAMPLES,out_len,out);rc=0;cleanup:if(run)MEDIA_AUDIO_ENC_Stop(0);if(made)MEDIA_AUDIO_ENC_DestroyChn(0);MEDIA_POOL_Destroy(POOL);done:MEDIA_SYS_Exit();return rc;}
