#include <stdio.h>
#include "media_api.h"

int main(void)
{
    int ret = MEDIA_SYS_Init();
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed: %d\n", ret);
        return 1;
    }

    ret = MEDIA_PLUGIN_Load("../../plugins/libmedia_tpg.so");
    if (ret < 0) {
        fprintf(stderr, "MEDIA_PLUGIN_Load failed: %d\n", ret);
        MEDIA_SYS_Exit();
        return 1;
    }

    puts("libmedia initialized and TPG plugin loaded");
    MEDIA_SYS_Exit();
    return 0;
}
