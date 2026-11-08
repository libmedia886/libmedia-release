#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_conv.h"
#include "../common/filter_runner.h"
int main(int argc, char **argv) {
    const char *out=argc>1?argv[1]:"conv.nv12"; int k=argc>2?atoi(argv[2]):5;
    if (k<1 || k>31 || !(k&1)) { fprintf(stderr,"kernel must be odd 1..31\n"); return 2; }
    MEDIA_CONV_CL_ATTR a; memset(&a,0,sizeof(a));
    a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12; a.kernel_size=k; a.output_pool_id=21;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_conv.so")!=0) return 1;
    int rc=tutorial_run_pipeline("CONV_CL",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
