#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_eis.h"
#include "media_tpg.h"
#define W 640
#define H 480
#define SRC_POOL 0
#define OUT_POOL 1
static int dump(const char *path, MEDIA_BUFFER b){FILE*f=fopen(path,"wb");void*p=MEDIA_POOL_GetVaddr(b);size_t n=MEDIA_POOL_GetSize(b);int r=-1;if(f&&p&&MEDIA_POOL_BeginCpuAccess(b,DMA_BUF_SYNC_READ)==0){r=fwrite(p,1,n,f)==n?0:-1;MEDIA_POOL_EndCpuAccess(b,DMA_BUF_SYNC_READ);}if(f)fclose(f);return r;}
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"eis_out.nv12";MEDIA_VIDEO_LAYOUT l;size_t sz;MEDIA_EIS_ATTR a;MEDIA_TPG_ATTR t;MEDIA_BUFFER b;int tpg=0,eis=0,run=0,rc=1;if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_VIDEO_BuildLinearLayout(W,H,MEDIA_FORMAT_NV12,&l,&sz)!=0||MEDIA_POOL_CreateWithMode(SRC_POOL,sz,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,sz,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0)goto done;memset(&t,0,sizeof(t));t.width=W;t.height=H;t.format=MEDIA_FORMAT_NV12;t.fps=30;t.pool_id=SRC_POOL;t.pattern=MEDIA_TPG_PATTERN_MOVING_BOX;if(MEDIA_TPG_CreateChn(0,&t)!=0)goto cleanup;tpg=1;memset(&a,0,sizeof(a));a.width=W;a.height=H;a.format=MEDIA_FORMAT_NV12;a.input_depth=4;a.output_pool_id=OUT_POOL;a.input_stride=l.pitch_bytes[0];a.output_stride=l.pitch_bytes[0];a.crop_ratio=0.84f;a.passthrough=1;if(MEDIA_EIS_CreateGrp(0,&a)!=0)goto cleanup;eis=1;if(MEDIA_SYS_Bind("TPG",0,"output0","EIS",0,"input0")!=0)goto cleanup;if(MEDIA_EIS_Start(0)!=0||MEDIA_TPG_Enable(0)!=0)goto cleanup;run=1;if(MEDIA_EIS_GetFrame(0,&b,2000)!=0||dump(out,b)!=0){fprintf(stderr,"EIS output unavailable\n");goto cleanup;}MEDIA_EIS_ReleaseFrame(0,b);printf("EIS passthrough crop_ratio=%.2f %dx%d -> %s\n",a.crop_ratio,W,H,out);rc=0;cleanup:if(run){MEDIA_TPG_Disable(0);MEDIA_EIS_Stop(0);MEDIA_SYS_UnBind("TPG",0,"output0","EIS",0,"input0");}if(eis)MEDIA_EIS_DestroyGrp(0);if(tpg)MEDIA_TPG_DestroyChn(0);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(SRC_POOL);done:MEDIA_SYS_Exit();return rc;}
