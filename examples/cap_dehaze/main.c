#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_cap_dehaze.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"cap_dehaze.rgb"; float scale=argc>2?(float)atof(argv[2]):0.25f;
    MEDIA_CAP_DEHAZE_ATTR a; memset(&a,0,sizeof(a)); a.width=640; a.height=360; a.format=MEDIA_FORMAT_RGB888;
    a.output_pool_id=21; a.guided_radius=24; a.guided_eps=1e-3f; a.t0=.12f; a.refine_scale=scale;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_cap_dehaze.so")!=0) return 1;
    int rc=tutorial_run_pipeline("CAP_DEHAZE",out,640,360,MEDIA_FORMAT_RGB888,640,360,MEDIA_FORMAT_RGB888,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
