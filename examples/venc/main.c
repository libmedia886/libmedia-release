#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_venc.h"
#define W 320
#define H 240
#define STRIDE 320
#define IN_POOL 0
#define OUT_POOL 1
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"venc_out.h264";MEDIA_VIDEO_LAYOUT l;size_t sz;MEDIA_BUFFER_META m;MEDIA_PACKET pkt;FILE*fout=NULL;int made=0,run=0,rc=1,packets=0;size_t total=0;if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_VIDEO_BuildLinearLayout(W,H,MEDIA_FORMAT_NV12,&l,&sz)!=0||MEDIA_POOL_CreateWithMode(IN_POOL,sz,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,256*1024,8,MEDIA_POOL_ALLOC_MODE_HOST)!=0)goto done;if(MEDIA_VENC_CreateChn(0,W,H,STRIDE,30,4,4,OUT_POOL,4000000,30,MEDIA_FORMAT_H264,MEDIA_VIDEO_COMPRESS_NONE)!=0){fprintf(stderr,"VENC create failed (MPP/device unavailable?)\n");goto cleanup;}made=1;if(MEDIA_VENC_Start(0)!=0)goto cleanup;run=1;fout=fopen(out,"wb");if(!fout)goto cleanup;for(int frame=0;frame<4;frame++){MEDIA_BUFFER in;if(MEDIA_POOL_GetBuffer(IN_POOL,&in)!=0)goto cleanup;uint8_t*p=MEDIA_POOL_GetVaddr(in);if(!p||MEDIA_POOL_BeginCpuAccess(in,DMA_BUF_SYNC_WRITE)!=0){MEDIA_POOL_PutBuffer(in);goto cleanup;}memset(p,16+frame*8,STRIDE*H);memset(p+STRIDE*H,128,STRIDE*H/2);MEDIA_POOL_EndCpuAccess(in,DMA_BUF_SYNC_WRITE);memset(&m,0,sizeof(m));m.valid=1;m.type=MEDIA_BUFFER_META_VIDEO_FRAME;m.media_format=MEDIA_FORMAT_NV12;m.len=sz;m.u.video=l;if(MEDIA_BUFFER_SetMeta(in,&m)!=0||MEDIA_SYS_SendFrame("VENC",0,"input0",in,1000)!=0){MEDIA_POOL_PutBuffer(in);goto cleanup;}if(MEDIA_VENC_GetPacket(0,&pkt,3000)==0){if(pkt.len&&fwrite(pkt.data,1,pkt.len,fout)==pkt.len){total+=pkt.len;packets++;}MEDIA_VENC_ReleasePacket(0,&pkt);}}fclose(fout);fout=NULL;if(!total){fprintf(stderr,"VENC produced no non-empty packet\n");goto cleanup;}printf("VENC H.264 NV12 %dx%d packets=%d bytes=%zu -> %s\n",W,H,packets,total,out);rc=0;cleanup:if(fout)fclose(fout);if(run)MEDIA_VENC_Stop(0);if(made)MEDIA_VENC_DestroyChn(0);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(IN_POOL);done:MEDIA_SYS_Exit();return rc;}
