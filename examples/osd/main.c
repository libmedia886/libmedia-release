#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_osd.h"
#include "media_tpg.h"
#define W 640
#define H 480
#define SRC_POOL 0
#define OUT_POOL 1
static int dump(const char *path, MEDIA_BUFFER b){FILE*f=fopen(path,"wb");void*p=MEDIA_POOL_GetVaddr(b);size_t n=MEDIA_POOL_GetSize(b);int r=-1;if(f&&p&&MEDIA_POOL_BeginCpuAccess(b,DMA_BUF_SYNC_READ)==0){r=fwrite(p,1,n,f)==n?0:-1;MEDIA_POOL_EndCpuAccess(b,DMA_BUF_SYNC_READ);}if(f)fclose(f);return r;}
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"osd_out.nv12";MEDIA_VIDEO_LAYOUT l;size_t sz;MEDIA_OSD_ATTR a;MEDIA_OSD_REGION_ATTR ra;MEDIA_OSD_RECT_DESC rd;MEDIA_TPG_ATTR t;MEDIA_BUFFER b;int tpg=0,osd=0,run=0,rc=1;if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_VIDEO_BuildLinearLayout(W,H,MEDIA_FORMAT_NV12,&l,&sz)!=0||MEDIA_POOL_CreateWithMode(SRC_POOL,sz,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,sz,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0)goto done;memset(&t,0,sizeof(t));t.width=W;t.height=H;t.format=MEDIA_FORMAT_NV12;t.fps=30;t.pool_id=SRC_POOL;t.pattern=MEDIA_TPG_PATTERN_COLOR_BARS;if(MEDIA_TPG_CreateChn(0,&t)!=0)goto cleanup;tpg=1;memset(&a,0,sizeof(a));a.input_width=W;a.input_height=H;a.format=MEDIA_FORMAT_NV12;a.input_depth=4;a.output_pool_id=OUT_POOL;a.input_stride=l.pitch_bytes[0];a.output_stride=l.pitch_bytes[0];a.max_regions=4;if(MEDIA_OSD_CreateGrp(0,&a)!=0)goto cleanup;osd=1;memset(&ra,0,sizeof(ra));ra.enabled=1;ra.x=40;ra.y=30;ra.width=240;ra.height=120;ra.global_alpha=220;memset(&rd,0,sizeof(rd));rd.filled=1;rd.color.r=255;rd.color.g=32;rd.color.b=32;rd.color.a=255;if(MEDIA_OSD_UpdateRegion(0,0,&ra)!=0||MEDIA_OSD_SetRegionRect(0,0,&rd)!=0)goto cleanup;if(MEDIA_SYS_Bind("TPG",0,"output0","OSD",0,"input0")!=0)goto cleanup;if(MEDIA_OSD_Start(0)!=0||MEDIA_TPG_Enable(0)!=0)goto cleanup;run=1;if(MEDIA_OSD_GetFrame(0,&b,2000)!=0||dump(out,b)!=0){fprintf(stderr,"OSD output unavailable\n");goto cleanup;}MEDIA_OSD_ReleaseFrame(0,b);printf("OSD RECT x=%d y=%d %dx%d alpha=%u -> %s\n",ra.x,ra.y,ra.width,ra.height,ra.global_alpha,out);rc=0;cleanup:if(run){MEDIA_TPG_Disable(0);MEDIA_OSD_Stop(0);MEDIA_SYS_UnBind("TPG",0,"output0","OSD",0,"input0");}if(osd)MEDIA_OSD_DestroyGrp(0);if(tpg)MEDIA_TPG_DestroyChn(0);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(SRC_POOL);done:MEDIA_SYS_Exit();return rc;}
