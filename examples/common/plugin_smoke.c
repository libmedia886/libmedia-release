#include <stdio.h>
#include <string.h>

#include "media_api.h"

#ifndef PLUGIN_BASENAME
#error "PLUGIN_BASENAME must be defined by the example CMakeLists.txt"
#endif
#ifndef MODULE_TYPES
#define MODULE_TYPES "(see plugin info)"
#endif
#ifndef PRIMARY_TYPE
#define PRIMARY_TYPE ""
#endif

int main(int argc, char **argv)
{
    char default_path[512];
    const char *path;
    MEDIA_PLUGIN_INFO info;
    int plugin_id;
    int ret;

    snprintf(default_path, sizeof(default_path), "../../plugins/%s", PLUGIN_BASENAME);
    path = argc > 1 ? argv[1] : default_path;

    ret = MEDIA_SYS_Init();
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed: %d\n", ret);
        return 1;
    }

    plugin_id = MEDIA_PLUGIN_Load(path);
    if (plugin_id < 0) {
        fprintf(stderr, "MEDIA_PLUGIN_Load(%s) failed: %d\n", path, plugin_id);
        MEDIA_SYS_Exit();
        return 1;
    }

    memset(&info, 0, sizeof(info));
    ret = MEDIA_PLUGIN_GetInfo(plugin_id, &info);
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_PLUGIN_GetInfo failed: %d\n", ret);
        MEDIA_SYS_Exit();
        return 1;
    }

    printf("plugin=%s\n", info.name);
    printf("version=%s\n", info.version);
    printf("path=%s\n", info.path);
    printf("classes=%d\n", info.class_count);
    printf("expected_types=%s\n", MODULE_TYPES);
    ret = MEDIA_PLUGIN_FindType(PRIMARY_TYPE);
    if (ret < 0) {
        fprintf(stderr, "MEDIA_PLUGIN_FindType(%s) failed: %d\n", PRIMARY_TYPE, ret);
        MEDIA_SYS_Exit();
        return 1;
    }
    printf("primary_type=%s\n", PRIMARY_TYPE);
    printf("signed plugin smoke test passed\n");

    MEDIA_SYS_Exit();
    return 0;
}
