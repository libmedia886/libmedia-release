#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_highlight_suppress.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"highlight_suppress.nv12"; float strength=argc>2?(float)atof(argv[2]):.94f;
    MEDIA_HIGHLIGHT_SUPPRESS_ATTR a; memset(&a,0,sizeof(a)); a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.output_pool_id=21; a.threshold_low=.58f; a.threshold_high=.86f; a.knee=.68f; a.ratio=.08f; a.strength=strength;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_highlight_suppress.so")!=0) return 1;
    int rc=tutorial_run_pipeline("HIGHLIGHT_SUPPRESS",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
