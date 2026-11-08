#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_tnr.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"tnr.nv12"; float threshold=argc>2?(float)atof(argv[2]):.12f;
    MEDIA_TNR_CL_ATTR a; memset(&a,0,sizeof(a)); a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.output_pool_id=21; a.block_size=16; a.threshold=threshold; a.static_alpha=.35f; a.motion_alpha=.88f;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_tnr.so")!=0) return 1;
    int rc=tutorial_run_pipeline("TNR_CL",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),2);
    MEDIA_SYS_Exit(); return rc;
}
