/*
 * RETINEX 入门教程：Retinex 增强（提亮暗部）
 *
 * Retinex 理论把图像看成“光照 × 反射率”，先估计出光照分量，再把它除掉，
 * 剩下的反射率就是物体本来的颜色。实际做法是对数域做高斯滤波差分，
 * 因此暗部会被显著提亮，适合夜间、背光场景。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 创建 TPG 作为输入源。
 *
 * 阅读顺序：
 *   [1] 解析命令行 -> [2] 填模块属性 -> [3] 初始化 + 加载插件 -> [4] 跑管线
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_retinex.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是增益。 */
    const char *out = argc > 1 ? argv[1] : "retinex.nv12";
    float gain = argc > 2 ? (float)atof(argv[2]) : 30.f;
    MEDIA_RETINEX_ATTR a;
    int rc;

    /* [2] 模块属性：
     *     scale_count —— 用几个尺度做 Retinex，越多越慢但过渡更自然
     *     gain        —— 增益，越大提亮越猛，也越容易出噪点
     *     log_min/log_max —— 对数域的有效范围 */
    memset(&a, 0, sizeof(a));
    a.scale_count = 1;
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.gain = gain;
    a.log_min = -4.f;
    a.log_max = 4.f;
    a.output_pool_id = 21;

    /* [3] 初始化并加载 TPG + RETINEX。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_retinex.so") != 0)
        return 1;

    /* [4] 输入输出都是 NV12、同尺寸。 */
    rc = tutorial_run_pipeline("RETINEX", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
