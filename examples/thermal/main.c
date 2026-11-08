#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_thermal.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"thermal.rgb"; int mode=argc>2?atoi(argv[2]):MEDIA_THERMAL_COLOR_RAINBOW;
    MEDIA_THERMAL_ATTR a; memset(&a,0,sizeof(a)); a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.color_mode=mode; a.output_format=MEDIA_FORMAT_RGB888; a.output_pool_id=21;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_thermal.so")!=0) return 1;
    int rc=tutorial_run_pipeline("THERMAL",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_RGB888,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
