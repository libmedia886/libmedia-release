/* Two synthetic frames -> stereo_3d -> one raw output frame. */
#include <linux/dma-buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "media_api.h"
#include "media_stereo_3d.h"
#define W 640
#define H 480
#define POOL_A 27
#define POOL_B 28
#define POOL_OUT 29
#define POOL_MASK 30
#define FMT MEDIA_FORMAT_NV12
static int make_frame(int pool, const MEDIA_VIDEO_LAYOUT *layout, size_t bytes, int source, MEDIA_BUFFER *out)
{
    MEDIA_BUFFER_META m;
    unsigned char *p;
    if (MEDIA_POOL_GetBuffer(pool,out)) return -1;
    p=MEDIA_POOL_GetVaddr(*out);
    if (!p || MEDIA_POOL_BeginCpuAccess(*out,DMA_BUF_SYNC_WRITE)) goto fail;
    memset(p,0,bytes);
#if FMT == MEDIA_FORMAT_RGB888
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
        unsigned char *q=p+(size_t)y*layout->pitch_bytes[0]+3*x;
        q[0]=(unsigned char)(source ? 20 : x*255/W);
        q[1]=(unsigned char)(source ? y*255/H : 30);
        q[2]=(unsigned char)(source ? 230 : 20);
    }
#else
    for (int y=0;y<H;y++) for (int x=0;x<W;x++)
        p[(size_t)y*layout->pitch_bytes[0]+x]=(unsigned char)(source ? 40+y*150/H : 40+x*150/W);
    for (int y=0;y<H/2;y++) for (int x=0;x<W;x+=2) {
        unsigned char *q=p+layout->offset_bytes[1]+(size_t)y*layout->pitch_bytes[1]+x;
        q[0]=source?90:160; q[1]=source?200:90;
    }
#endif
    MEDIA_POOL_EndCpuAccess(*out,DMA_BUF_SYNC_WRITE);
    memset(&m,0,sizeof(m)); m.valid=1; m.type=MEDIA_BUFFER_META_VIDEO_FRAME;
    m.media_format=FMT; m.len=(uint32_t)bytes; m.u.video=*layout;
    m.seq=1; m.time_stamp=1000000;
    if (MEDIA_BUFFER_SetMeta(*out,&m)==0) return 0;
fail: MEDIA_POOL_PutBuffer(*out); return -1;
}
static int send_frame(int pool, const MEDIA_VIDEO_LAYOUT *layout, size_t bytes, int source, const char *port)
{
    MEDIA_BUFFER b; int r=make_frame(pool,layout,bytes,source,&b);
    if(r) return r;
    r=MEDIA_SYS_SendFrame("STEREO_3D",0,port,b,1000);
    if(r) MEDIA_POOL_PutBuffer(b);
    return r;
}
int main(int argc, char **argv)
{
    int mode=argc>1?atoi(argv[1]):0, r=1, created=0, started=0;
    const char *output=argc>2?argv[2]:"stereo_3d_frame.nv12";
    const char *plugin=argc>3?argv[3]:"../../plugins/libmedia_stereo_3d.so";
    MEDIA_VIDEO_LAYOUT L,OL; size_t in_bytes=0,out_bytes=0;
    MEDIA_STEREO_3D_ATTR a; MEDIA_BUFFER out; MEDIA_BUFFER_META meta; FILE *fp;
    if (MEDIA_SYS_Init()!=0) return 1;
    if (MEDIA_PLUGIN_Load(plugin)<0) { fprintf(stderr,"load %s failed\n",plugin); goto done; }
    if (MEDIA_VIDEO_BuildLinearLayout(W,H,FMT,&L,&in_bytes)!=0 ||
        MEDIA_VIDEO_BuildLinearLayout(W,H,FMT,&OL,&out_bytes)!=0) goto done;
    if (MEDIA_POOL_CreateWithMode(POOL_A,in_bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0 ||
        MEDIA_POOL_CreateWithMode(POOL_B,in_bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0 ||
        MEDIA_POOL_CreateWithMode(POOL_OUT,out_bytes,4,MEDIA_POOL_ALLOC_MODE_NATIVE)!=0) goto done;
    memset(&a,0,sizeof(a)); a.width=W; a.height=H; a.format=FMT; a.input_stride=L.pitch_bytes[0]; a.output_stride=OL.pitch_bytes[0]; a.output_pool_id=POOL_OUT; a.mode=mode; a.rotate_input=1; a.rotation_degrees=3.5f;
    r=MEDIA_PLUGIN_CreateModule("STEREO_3D",0,&a,sizeof(a));
    if(r) { fprintf(stderr,"create failed: %d\n",r); goto done; } created=1;
    r=MEDIA_PLUGIN_Start("STEREO_3D",0);
    if(r) { fprintf(stderr,"start failed: %d\n",r); goto done; } started=1;
    /* Secondary first: the primary input may trigger processing immediately. */
    if ((r=send_frame(POOL_B,&L,in_bytes,1,"input1"))!=0 ||
        (r=send_frame(POOL_A,&L,in_bytes,0,"input0"))!=0) {
        fprintf(stderr,"send failed: %d\n",r); goto done;
    }
    r=MEDIA_SYS_GetFrame("STEREO_3D",0,"output0",&out,5000);
    if(r) { fprintf(stderr,"get failed: %d\n",r); goto done; }
    if (MEDIA_BUFFER_GetMeta(out,&meta)!=0 || !meta.valid ||
        MEDIA_POOL_BeginCpuAccess(out,DMA_BUF_SYNC_READ)!=0) {
        fprintf(stderr,"output metadata/access failed\n");
        MEDIA_SYS_ReleaseFrame("STEREO_3D",0,"output0",out); r=1; goto done;
    }
    fp=fopen(output,"wb");
    void *p=MEDIA_POOL_GetVaddr(out);
    size_t n=meta.len <= MEDIA_POOL_GetSize(out) ? meta.len : MEDIA_POOL_GetSize(out);
    r=(fp && p && fwrite(p,1,n,fp)==n)?0:1;
    if(fp) fclose(fp);
    MEDIA_POOL_EndCpuAccess(out,DMA_BUF_SYNC_READ);
    MEDIA_SYS_ReleaseFrame("STEREO_3D",0,"output0",out);
    if(r) fprintf(stderr,"write failed: %s\n",output);
    else printf("STEREO_3D mode=%d %ux%u bytes=%zu -> %s\n",mode,meta.u.video.visible_width,meta.u.video.visible_height,n,output);
done:
    if(started) MEDIA_PLUGIN_Stop("STEREO_3D",0);
    if(created) MEDIA_PLUGIN_DestroyModule("STEREO_3D",0);
    MEDIA_POOL_Destroy(POOL_MASK); MEDIA_POOL_Destroy(POOL_OUT); MEDIA_POOL_Destroy(POOL_B); MEDIA_POOL_Destroy(POOL_A);
    MEDIA_SYS_Exit(); return r?1:0;
}
