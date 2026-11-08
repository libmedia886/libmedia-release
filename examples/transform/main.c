#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_transform.h"
#include "media_tpg.h"
#define IN_W 640
#define IN_H 480
#define OUT_W 320
#define OUT_H 240
#define SRC_POOL 0
#define OUT_POOL 1
static int dump(const char *path, MEDIA_BUFFER b){FILE*f=fopen(path,"wb");void*p=MEDIA_POOL_GetVaddr(b);size_t n=MEDIA_POOL_GetSize(b);int r=-1;if(f&&p&&MEDIA_POOL_BeginCpuAccess(b,DMA_BUF_SYNC_READ)==0){r=fwrite(p,1,n,f)==n?0:-1;MEDIA_POOL_EndCpuAccess(b,DMA_BUF_SYNC_READ);}if(f)fclose(f);return r;}
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"transform_out.nv12";MEDIA_VIDEO_LAYOUT li,lo;size_t si,so;float*lut;MEDIA_TRANSFORM_ATTR a;MEDIA_TPG_ATTR t;MEDIA_BUFFER b;int tpg=0,tr=0,run=0,rc=1;if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_VIDEO_BuildLinearLayout(IN_W,IN_H,MEDIA_FORMAT_NV12,&li,&si)!=0||MEDIA_VIDEO_BuildLinearLayout(OUT_W,OUT_H,MEDIA_FORMAT_NV12,&lo,&so)!=0||MEDIA_POOL_CreateWithMode(SRC_POOL,si,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,so,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0)goto done;lut=calloc((size_t)OUT_W*OUT_H*2,sizeof(*lut));if(!lut)goto cleanup;for(int y=0;y<OUT_H;y++)for(int x=0;x<OUT_W;x++){lut[2*((size_t)y*OUT_W+x)]=(float)x*IN_W/OUT_W;lut[2*((size_t)y*OUT_W+x)+1]=(float)y*IN_H/OUT_H;}memset(&a,0,sizeof(a));a.in_width=IN_W;a.in_height=IN_H;a.input_format=MEDIA_FORMAT_NV12;a.in_stride=li.pitch_bytes[0];a.out_width=OUT_W;a.out_height=OUT_H;a.output_format=MEDIA_FORMAT_NV12;a.out_stride=lo.pitch_bytes[0];a.input_depth=4;a.pool_id=OUT_POOL;a.lut_width=OUT_W;a.lut_height=OUT_H;a.lut=lut;a.lut_size=(size_t)OUT_W*OUT_H*2*sizeof(float);if(MEDIA_TRANSFORM_CreateGrp(0,&a)!=0){free(lut);goto cleanup;}tr=1;memset(&t,0,sizeof(t));t.width=IN_W;t.height=IN_H;t.format=MEDIA_FORMAT_NV12;t.fps=30;t.pool_id=SRC_POOL;t.pattern=MEDIA_TPG_PATTERN_GRADIENT;if(MEDIA_TPG_CreateChn(0,&t)!=0||MEDIA_SYS_Bind("TPG",0,"output0","TRANSFORM",0,"input0")!=0||MEDIA_TRANSFORM_Start(0)!=0||MEDIA_TPG_Enable(0)!=0)goto cleanup; tpg=1;run=1;if(MEDIA_TRANSFORM_GetFrame(0,&b,2000)!=0||dump(out,b)!=0){fprintf(stderr,"TRANSFORM output unavailable\n");goto cleanup;}MEDIA_TRANSFORM_ReleaseFrame(0,b);printf("TRANSFORM identity LUT: %dx%d -> %dx%d -> %s\n",IN_W,IN_H,OUT_W,OUT_H,out);rc=0;cleanup:if(run){MEDIA_TPG_Disable(0);MEDIA_TRANSFORM_Stop(0);MEDIA_SYS_UnBind("TPG",0,"output0","TRANSFORM",0,"input0");}if(tpg)MEDIA_TPG_DestroyChn(0);if(tr)MEDIA_TRANSFORM_DestroyGrp(0);free(lut);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(SRC_POOL);done:MEDIA_SYS_Exit();return rc;}
