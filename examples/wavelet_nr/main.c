#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_wavelet_nr.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"wavelet_nr.nv12"; int levels=argc>2?atoi(argv[2]):1;
    MEDIA_WAVELET_NR_CL_ATTR a; memset(&a,0,sizeof(a)); a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.output_pool_id=21; a.levels=levels; a.threshold_y=6.f/255.f; a.strength=.60f; a.uv_enable=1; a.uv_strength=.5f;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_wavelet_nr.so")!=0) return 1;
    int rc=tutorial_run_pipeline("WAVELET_NR_CL",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
