/*
 * THERMAL 入门教程：把灰度图按伪彩映射成 RGB
 *
 * 热成像传感器输出的是单个温度/亮度值，人眼不敏感。THERMAL 把亮度按调色板
 * 映射成彩色，冷色=低温、暖色=高温，便于快速定位热源。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 创建 TPG 彩条作为输入。
 * 输入 NV12、输出 RGB888，所以两者的字节数不同，runner 会分别建 Pool。
 *
 * 阅读顺序：
 *   [1] 解析命令行 -> [2] 填模块属性 -> [3] 初始化 + 加载插件 -> [4] 跑管线
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_thermal.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是调色板编号
     *     （默认 MEDIA_THERMAL_COLOR_RAINBOW，彩虹色）。 */
    const char *out = argc > 1 ? argv[1] : "thermal.rgb";
    int mode = argc > 2 ? atoi(argv[2]) : MEDIA_THERMAL_COLOR_RAINBOW;
    MEDIA_THERMAL_ATTR a;
    int rc;

    /* [2] 模块属性。注意这里输入格式和输出格式不同：
     *     输入走 NV12，输出走 RGB888。 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.color_mode = mode;
    a.output_format = MEDIA_FORMAT_RGB888;
    a.output_pool_id = 21;

    /* [3] 初始化并加载 TPG + THERMAL。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_thermal.so") != 0)
        return 1;

    /* [4] 输入 NV12、输出 RGB888 —— 这两组参数必须和上面的属性一致，
     *     否则 runner 建的输出 Pool 尺寸会不对。 */
    rc = tutorial_run_pipeline("THERMAL", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_RGB888, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
