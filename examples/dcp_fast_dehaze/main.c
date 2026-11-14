/*
 * DCP_FAST_DEHAZE 入门教程：暗通道先验去雾（快速版）
 *
 * 雾天图像的对比度被散射光压平。暗通道先验的思路是：绝大多数自然图像里，
 * 总有一个颜色通道在某处接近 0；被雾覆盖后这个“暗通道”被抬高，
 * 抬高的程度就反映了雾的浓度。估计出浓度后反推就能去雾。
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
#include "media_dcp_fast_dehaze.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是细化尺度。 */
    const char *out = argc > 1 ? argv[1] : "dcp_fast_dehaze.rgb";
    float refine = argc > 2 ? (float)atof(argv[2]) : .5f;
    MEDIA_DCP_FAST_DEHAZE_ATTR a;
    int rc;

    /* [2] 模块属性：
     *     patch    —— 估计雾浓度时的局部窗口大小
     *     omega    —— 去雾强度，接近 1 更彻底但也更容易过曝
     *     t0       —— 透射率下限，防止除零
     *     refine_scale —— 透射率细化尺度，越小越细但越慢 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_RGB888;
    a.output_pool_id = 21;
    a.patch = 15;
    a.omega = .95f;
    a.t0 = .40f;
    a.refine_scale = refine;

    /* [3] 初始化并加载 TPG + DCP_FAST_DEHAZE。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_dcp_fast_dehaze.so") != 0)
        return 1;

    /* [4] 输入输出都是 RGB888、同尺寸。 */
    rc = tutorial_run_pipeline("DCP_FAST_DEHAZE", out, 640, 360, MEDIA_FORMAT_RGB888, 640,
                               360, MEDIA_FORMAT_RGB888, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
