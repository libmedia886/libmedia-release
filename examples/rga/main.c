#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_rga.h"
#include "media_tpg.h"
#define IW 640
#define IH 480
#define OW 320
#define OH 240
#define SRC_POOL 0
#define OUT_POOL 1
static int dump(const char *path, MEDIA_BUFFER b){FILE*f=fopen(path,"wb");void*p=MEDIA_POOL_GetVaddr(b);size_t n=MEDIA_POOL_GetSize(b);int r=-1;if(f&&p&&MEDIA_POOL_BeginCpuAccess(b,DMA_BUF_SYNC_READ)==0){r=fwrite(p,1,n,f)==n?0:-1;MEDIA_POOL_EndCpuAccess(b,DMA_BUF_SYNC_READ);}if(f)fclose(f);return r;}
int main(int argc,char**argv){const char*out=argc>1?argv[1]:"rga_out.nv12";MEDIA_VIDEO_LAYOUT li,lo;size_t si,so;MEDIA_RGA_GRP_ATTR a;MEDIA_RGA_PORT_ATTR in,outp;MEDIA_TPG_ATTR t;MEDIA_BUFFER b;int tpg=0,rga=0,run=0,rc=1;if(MEDIA_SYS_Init()!=0)return 1;if(MEDIA_VIDEO_BuildLinearLayout(IW,IH,MEDIA_FORMAT_NV12,&li,&si)!=0||MEDIA_VIDEO_BuildLinearLayout(OW,OH,MEDIA_FORMAT_NV12,&lo,&so)!=0||MEDIA_POOL_CreateWithMode(SRC_POOL,si,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0||MEDIA_POOL_CreateWithMode(OUT_POOL,so,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0)goto done;memset(&in,0,sizeof(in));in.port_id=0;in.width=IW;in.height=IH;in.stride=li.pitch_bytes[0];in.format=MEDIA_FORMAT_NV12;in.queue_depth=4;memset(&outp,0,sizeof(outp));outp.port_id=0;outp.width=OW;outp.height=OH;outp.stride=lo.pitch_bytes[0];outp.format=MEDIA_FORMAT_NV12;outp.pool_id=OUT_POOL;outp.queue_depth=4;memset(&a,0,sizeof(a));a.algo=MEDIA_RGA_ALG_RESIZE;a.input_count=1;a.output_count=1;a.input_depth=4;a.output_depth=4;a.inputs[0]=in;a.outputs[0]=outp;if(MEDIA_RGA_CreateGrp(0,&a)!=0){fprintf(stderr,"RGA create failed\n");goto cleanup;}rga=1;memset(&t,0,sizeof(t));t.width=IW;t.height=IH;t.format=MEDIA_FORMAT_NV12;t.fps=30;t.pool_id=SRC_POOL;t.pattern=MEDIA_TPG_PATTERN_CHECKERBOARD;if(MEDIA_TPG_CreateChn(0,&t)!=0||MEDIA_SYS_Bind("TPG",0,"output0","RGA",0,"input0")!=0||MEDIA_RGA_Start(0)!=0||MEDIA_TPG_Enable(0)!=0)goto cleanup;tpg=1;run=1;if(MEDIA_RGA_GetFrame(0,&b,2000)!=0||dump(out,b)!=0){fprintf(stderr,"RGA output unavailable\n");goto cleanup;}MEDIA_RGA_ReleaseFrame(0,b);printf("RGA RESIZE NV12: %dx%d -> %dx%d -> %s\n",IW,IH,OW,OH,out);rc=0;cleanup:if(run){MEDIA_TPG_Disable(0);MEDIA_RGA_Stop(0);MEDIA_SYS_UnBind("TPG",0,"output0","RGA",0,"input0");}if(tpg)MEDIA_TPG_DestroyChn(0);if(rga)MEDIA_RGA_DestroyChn(0);MEDIA_POOL_Destroy(OUT_POOL);MEDIA_POOL_Destroy(SRC_POOL);done:MEDIA_SYS_Exit();return rc;}
