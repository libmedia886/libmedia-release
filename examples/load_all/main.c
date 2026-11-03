#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "media_api.h"

static int is_plugin(const char *name)
{
    size_t n = strlen(name);
    return n > 3 && strcmp(name + n - 3, ".so") == 0;
}

int main(void)
{
    DIR *dir;
    struct dirent *entry;
    int loaded = 0;
    int ret;

    ret = MEDIA_SYS_Init();
    if (ret != MEDIA_STATUS_OK) {
        fprintf(stderr, "MEDIA_SYS_Init failed: %d\n", ret);
        return 1;
    }

    dir = opendir("../../plugins");
    if (!dir) {
        perror("opendir plugins");
        MEDIA_SYS_Exit();
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        if (!is_plugin(entry->d_name)) continue;
        snprintf(path, sizeof(path), "../../plugins/%s", entry->d_name);
        ret = MEDIA_PLUGIN_Load(path);
        if (ret >= 0) loaded++;
    }
    closedir(dir);
    MEDIA_SYS_Exit();
    printf("loaded %d signed plugins\n", loaded);
    return loaded > 0 ? 0 : 1;
}
