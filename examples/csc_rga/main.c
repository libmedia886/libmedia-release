#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_csc_rga.h"
#include "../common/filter_runner.h"
int main(int argc,char **argv) {
    const char *out=argc>1?argv[1]:"csc_rga.rgb"; int mode=argc>2?atoi(argv[2]):CSC_RGA_CSC_BT601_LIMIT;
    MEDIA_CSC_RGA_ATTR a; memset(&a,0,sizeof(a)); a.input_width=640; a.input_height=360;
    a.input_format=MEDIA_FORMAT_NV12; a.output_format=MEDIA_FORMAT_RGB888; a.output_pool_id=21; a.csc_mode=mode;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_csc_rga.so")!=0) return 1;
    int rc=tutorial_run_pipeline("CSC_RGA",out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_RGB888,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
