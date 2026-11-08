/* Two synthetic NV12 sensors -> MCF_FUSION_CL -> one raw NV12 output. */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_mcf_fusion.h"
#define W 640
#define H 480
#define POOL_COLOR 27
#define POOL_MONO 28
#define POOL_OUT 29
static int make_frame(int pool,const MEDIA_VIDEO_LAYOUT *l,size_t bytes,int mono,MEDIA_BUFFER *b)
{
    MEDIA_BUFFER_META m; unsigned char *p;
    if(MEDIA_POOL_GetBuffer(pool,b)) return -1;
    p=MEDIA_POOL_GetVaddr(*b);
    if(!p || MEDIA_POOL_BeginCpuAccess(*b,DMA_BUF_SYNC_WRITE)) goto fail;
    memset(p,0,bytes);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) p[(size_t)y*l->pitch_bytes[0]+x]=(unsigned char)(mono?40+y*170/H:40+x*170/W);
    for(int y=0;y<H/2;y++) for(int x=0;x<W;x+=2){ unsigned char *q=p+l->offset_bytes[1]+(size_t)y*l->pitch_bytes[1]+x; q[0]=mono?128:90; q[1]=mono?128:180; }
    MEDIA_POOL_EndCpuAccess(*b,DMA_BUF_SYNC_WRITE);
    memset(&m,0,sizeof(m)); m.valid=1; m.type=MEDIA_BUFFER_META_VIDEO_FRAME; m.media_format=MEDIA_FORMAT_NV12; m.len=(uint32_t)bytes; m.u.video=*l; m.time_stamp=1000000; m.seq=1;
    if(MEDIA_BUFFER_SetMeta(*b,&m)==0) return 0;
fail: MEDIA_POOL_PutBuffer(*b); return -1;
}
int main(int argc,char **argv)
{
    int path=argc>1?atoi(argv[1]):0, r=1, created=0, started=0, chn=0;
    const char *out_path=argc>2?argv[2]:"mcf_fusion_frame.nv12";
    const char *plugin=argc>3?argv[3]:"../../plugins/libmedia_mcf_fusion.so";
    MEDIA_VIDEO_LAYOUT l; size_t bytes=0; MEDIA_MCF_GRP_ATTR ga; MEDIA_MCF_CHN_ATTR ca; MEDIA_MCF_ALG_PARAM ap; MEDIA_BUFFER c,m,out; MEDIA_BUFFER_META meta; FILE *fp;
    if(MEDIA_SYS_Init()!=0 || MEDIA_PLUGIN_Load(plugin)<0) goto done;
    if(MEDIA_VIDEO_BuildLinearLayout(W,H,MEDIA_FORMAT_NV12,&l,&bytes)!=0) goto done;
    if(MEDIA_POOL_CreateWithMode(POOL_COLOR,bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0 || MEDIA_POOL_CreateWithMode(POOL_MONO,bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0 || MEDIA_POOL_CreateWithMode(POOL_OUT,bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0) goto done;
    memset(&ga,0,sizeof(ga)); ga.sync_pipe_en=0; ga.path=path; ga.reuse_last_frame=1; ga.timestamp_tolerance_us=2000; ga.depth=4;
    ga.color_pipe_attr=(MEDIA_MCF_PIPE_ATTR){MEDIA_MCF_PIPE_COLOR,W,H,MEDIA_FORMAT_NV12,(int)l.pitch_bytes[0],4};
    ga.mono_pipe_attr=(MEDIA_MCF_PIPE_ATTR){MEDIA_MCF_PIPE_MONO,W,H,MEDIA_FORMAT_NV12,(int)l.pitch_bytes[0],4};
    r=MEDIA_MCF_CreateGrp(0,&ga); if(r) goto done; created=1;
    memset(&ca,0,sizeof(ca)); ca.width=W; ca.height=H; ca.format=MEDIA_FORMAT_NV12; ca.stride=l.pitch_bytes[0]; ca.depth=4; ca.output_pool_id=POOL_OUT;
    r=MEDIA_MCF_SetChnAttr(0,0,&ca); if(r) goto done;
    r=MEDIA_MCF_EnableChn(0,0); if(r) goto done; chn=1;
    memset(&ap,0,sizeof(ap)); ap.mode=MEDIA_MCF_ALG_LOCAL_REALTIME; ap.normalize_mode=MEDIA_MCF_NORMALIZE_NONE; ap.local_window=5; ap.local_map_sigma=0.8f; ap.local_detail_sigma=3.0f; ap.local_detail_gain=0.9f; ap.registration_confidence=1.0f; ap.fallback_threshold=0.0f; ap.epsilon=1e-6f;
    MEDIA_MCF_SetAlgParam(0,&ap);
    r=MEDIA_MCF_StartGrp(0); if(r) goto done; started=1;
    if(make_frame(POOL_COLOR,&l,bytes,0,&c) || make_frame(POOL_MONO,&l,bytes,1,&m)) goto done;
    if(MEDIA_MCF_SendPipeFrame(0,MEDIA_MCF_PIPE_COLOR,c,1000)!=0){MEDIA_POOL_PutBuffer(c); goto done;}
    if(MEDIA_MCF_SendPipeFrame(0,MEDIA_MCF_PIPE_MONO,m,1000)!=0){MEDIA_POOL_PutBuffer(m); goto done;}
    r=MEDIA_MCF_GetChnFrame(0,0,&out,5000); if(r) goto done;
    if(MEDIA_BUFFER_GetMeta(out,&meta)!=0 || MEDIA_POOL_BeginCpuAccess(out,DMA_BUF_SYNC_READ)!=0){MEDIA_MCF_ReleaseChnFrame(0,0,out); goto done;}
    fp=fopen(out_path,"wb"); void *p=MEDIA_POOL_GetVaddr(out); size_t n=meta.len<MEDIA_POOL_GetSize(out)?meta.len:MEDIA_POOL_GetSize(out); r=(fp&&p&&fwrite(p,1,n,fp)==n)?0:1; if(fp)fclose(fp); MEDIA_POOL_EndCpuAccess(out,DMA_BUF_SYNC_READ); MEDIA_MCF_ReleaseChnFrame(0,0,out);
    if(!r) printf("MCF path=%d %ux%u bytes=%zu -> %s\n",path,meta.u.video.visible_width,meta.u.video.visible_height,n,out_path);
done:
    if(started) MEDIA_MCF_StopGrp(0); if(chn) MEDIA_MCF_DisableChn(0,0); if(created) MEDIA_MCF_DestroyGrp(0);
    MEDIA_POOL_Destroy(POOL_OUT); MEDIA_POOL_Destroy(POOL_MONO); MEDIA_POOL_Destroy(POOL_COLOR); MEDIA_SYS_Exit(); return r?1:0;
}
