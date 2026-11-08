#include <linux/dma-buf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_demosaic.h"
#define W 64
#define H 48
#define IN_STRIDE 128
#define OUT_STRIDE 384
#define IN_POOL 0
#define OUT_POOL 1
static int dump(const char *path, MEDIA_BUFFER b) { FILE *f=fopen(path,"wb"); void *p=MEDIA_POOL_GetVaddr(b); size_t n=MEDIA_POOL_GetSize(b); int r=-1; if(f&&p&&MEDIA_POOL_BeginCpuAccess(b,DMA_BUF_SYNC_READ)==0){r=fwrite(p,1,n,f)==n?0:-1; MEDIA_POOL_EndCpuAccess(b,DMA_BUF_SYNC_READ);} if(f)fclose(f); return r; }
int main(int argc,char **argv){const char *out=argc>1?argv[1]:"demosaic_out.rgb_u16"; MEDIA_BUFFER in,b; MEDIA_BUFFER_META m; MEDIA_VIDEO_LAYOUT li; MEDIA_DEMOSAIC_ATTR a; size_t si=0,so=(size_t)OUT_STRIDE*H; int made=0,started=0,rc=1; memset(&li,0,sizeof(li)); li.visible_width=W; li.visible_height=H; li.storage_width=W; li.storage_height=H; li.plane_count=1; li.pitch_bytes[0]=IN_STRIDE; if(MEDIA_VIDEO_BuildLinearLayout(W,H,MEDIA_FORMAT_BAYER_RAW16_LE,&li,&si)!=0) { fprintf(stderr,"DEMOSAIC RAW layout setup failed\n"); return 1; } if(MEDIA_SYS_Init()!=0)return 1; if(MEDIA_POOL_CreateWithMode(IN_POOL,si,2,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,so,2,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0){fprintf(stderr,"DEMOSAIC pool setup failed\n");goto done;} memset(&a,0,sizeof(a));a.width=W;a.height=H;a.input_depth=2;a.output_pool_id=OUT_POOL;a.input_stride=IN_STRIDE;a.output_stride=OUT_STRIDE;a.bayer_pattern=MEDIA_BAYER_PATTERN_RGGB; if(MEDIA_DEMOSAIC_Create(0,&a)!=0){fprintf(stderr,"DEMOSAIC create failed\n");goto cleanup;}made=1;if(MEDIA_DEMOSAIC_Start(0)!=0){fprintf(stderr,"DEMOSAIC start failed\n");goto cleanup;}started=1; if(MEDIA_POOL_GetBuffer(IN_POOL,&in)!=0)goto cleanup; uint8_t *p=MEDIA_POOL_GetVaddr(in); if(!p||MEDIA_POOL_BeginCpuAccess(in,DMA_BUF_SYNC_WRITE)!=0){MEDIA_POOL_PutBuffer(in);goto cleanup;} for(int y=0;y<H;y++)for(int x=0;x<W;x++)*(uint16_t*)(p+y*IN_STRIDE+x*2)=(uint16_t)(((x/8+y/8)&1)?50000:10000); MEDIA_POOL_EndCpuAccess(in,DMA_BUF_SYNC_WRITE); memset(&m,0,sizeof(m));m.valid=1;m.type=MEDIA_BUFFER_META_VIDEO_FRAME;m.media_format=MEDIA_FORMAT_BAYER_RAW16_LE;m.len=IN_STRIDE*H;m.u.video=li; if(MEDIA_BUFFER_SetMeta(in,&m)!=0||MEDIA_SYS_SendFrame("DEMOSAIC",0,"input0",in,1000)!=0){MEDIA_POOL_PutBuffer(in);goto cleanup;} if(MEDIA_DEMOSAIC_GetFrame(0,&b,2000)!=0||dump(out,b)!=0){fprintf(stderr,"DEMOSAIC output unavailable\n");goto cleanup;} MEDIA_DEMOSAIC_ReleaseFrame(0,b);printf("DEMOSAIC RGGB RAW16 %dx%d -> RGB_U16 %s\n",W,H,out);rc=0;cleanup:if(started)MEDIA_DEMOSAIC_Stop(0);if(made)MEDIA_DEMOSAIC_Destroy(0);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(IN_POOL);done:MEDIA_SYS_Exit();return rc;}
