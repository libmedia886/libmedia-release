/*
 * HIGHLIGHT_SUPPRESS 入门教程：压制高光
 *
 * 逆光或强光源直射时，画面里会出现大片过曝白斑，既难看也丢细节。
 * 本模块在亮度超过“低阈值”后开始按比例压低，直到“高阈值”处达到最强，
 * 中间用拐点（knee）做平滑过渡，避免出现生硬的边界。
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
#include "media_highlight_suppress.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是压制强度。 */
    const char *out = argc > 1 ? argv[1] : "highlight_suppress.nv12";
    float strength = argc > 2 ? (float)atof(argv[2]) : .94f;
    MEDIA_HIGHLIGHT_SUPPRESS_ATTR a;
    int rc;

    /* [2] 模块属性。三个阈值都是归一化到 0..1 的亮度：
     *     低于 threshold_low 不动，高于 threshold_high 压到最强，
     *     中间按 knee 做平滑过渡。ratio 是基础压低比例。 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.output_pool_id = 21;
    a.threshold_low = .58f;
    a.threshold_high = .86f;
    a.knee = .68f;
    a.ratio = .08f;
    a.strength = strength;

    /* [3] 初始化并加载 TPG + HIGHLIGHT_SUPPRESS。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_highlight_suppress.so") != 0)
        return 1;

    /* [4] 输入输出都是 NV12、同尺寸。 */
    rc = tutorial_run_pipeline("HIGHLIGHT_SUPPRESS", out, 640, 360, MEDIA_FORMAT_NV12, 640,
                               360, MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
