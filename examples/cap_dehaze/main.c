/*
 * CAP_DEHAZE 入门教程：基于导向滤波的去雾
 *
 * 与 DCP_FAST_DEHAZE 目标相同（去掉雾造成的低对比度），但用引导滤波
 * 估计并细化透射率，边缘保留更好、速度稍慢。适合对画质要求更高的场景。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 创建 TPG 作为输入源。
 * 输入输出都是 RGB888。
 *
 * 阅读顺序：
 *   [1] 解析命令行 -> [2] 填模块属性 -> [3] 初始化 + 加载插件 -> [4] 跑管线
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_cap_dehaze.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是细化尺度。 */
    const char *out = argc > 1 ? argv[1] : "cap_dehaze.rgb";
    float scale = argc > 2 ? (float)atof(argv[2]) : 0.25f;
    MEDIA_CAP_DEHAZE_ATTR a;
    int rc;

    /* [2] 模块属性：
     *     guided_radius —— 导向滤波窗口半径，越大越平滑
     *     guided_eps    —— 正则项，越大越平滑但细节越少
     *     t0            —— 透射率下限，防止除零导致过曝 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_RGB888;
    a.output_pool_id = 21;
    a.guided_radius = 24;
    a.guided_eps = 1e-3f;
    a.t0 = .12f;
    a.refine_scale = scale;

    /* [3] 初始化并加载 TPG + CAP_DEHAZE。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_cap_dehaze.so") != 0)
        return 1;

    /* [4] 输入输出都是 RGB888、同尺寸。 */
    rc = tutorial_run_pipeline("CAP_DEHAZE", out, 640, 360, MEDIA_FORMAT_RGB888, 640, 360,
                               MEDIA_FORMAT_RGB888, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
