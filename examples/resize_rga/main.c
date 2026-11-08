#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_resize_rga.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"resize_rga.nv12"; int outw=argc>2?atoi(argv[2]):320; int outh=argc>3?atoi(argv[3]):180;
    MEDIA_RESIZE_RGA_ATTR a; memset(&a,0,sizeof(a)); a.src_width=640; a.src_height=360; a.input_width=640; a.input_height=360;
    a.input_format=MEDIA_FORMAT_NV12; a.out_width=outw; a.out_height=outh; a.output_format=MEDIA_FORMAT_NV12; a.output_pool_id=21; a.keep_aspect_ratio=1;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_resize_rga.so")!=0) return 1;
    int rc=tutorial_run_pipeline("RESIZE_RGA",out,640,360,MEDIA_FORMAT_NV12,outw,outh,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
