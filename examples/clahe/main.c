#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "media_api.h"
#include "media_clahe.h"
#include "../common/filter_runner.h"
int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "clahe.nv12";
    int grid = argc > 2 ? atoi(argv[2]) : 8;
    MEDIA_CLAHE_ATTR a; memset(&a, 0, sizeof(a));
    a.width=640; a.height=360; a.format=MEDIA_FORMAT_NV12;
    a.tile_grid_x=grid; a.tile_grid_y=grid; a.clip_limit=2.0f;
    a.highlight_protect_start=0.0f; a.highlight_protect_strength=1.0f;
    a.output_pool_id=21;
    if (MEDIA_SYS_Init()!=0 || tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_clahe.so")!=0) return 1;
    int rc=tutorial_run_pipeline("CLAHE", out,640,360,MEDIA_FORMAT_NV12,640,360,MEDIA_FORMAT_NV12,&a,sizeof(a),1);
    MEDIA_SYS_Exit(); return rc;
}
