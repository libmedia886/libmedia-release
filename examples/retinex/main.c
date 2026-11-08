#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_retinex.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"retinex.nv12"; float gain=argc>2?(float)atof(argv[2]):30.f;
    MEDIA_RETINEX_ATTR a; memset(&a,0,sizeof(a)); a.scale_count=1; a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.gain=gain; a.log_min=-4.f; a.log_max=4.f; a.output_pool_id=21;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_retinex.so")!=0) return 1;
    int rc=tutorial_run_pipeline("RETINEX",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
