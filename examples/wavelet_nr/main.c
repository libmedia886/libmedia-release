/*
 * WAVELET_NR 入门教程：小波域降噪
 *
 * 小波变换把图像分解成不同尺度的细节层，噪声主要藏在细尺度层里。
 * 对每层按阈值做收缩（小系数直接压到 0，大系数保留），再重构回原图，
 * 就得到降噪结果。这是空间域方法，不依赖历史帧，所以单帧就能看到效果。
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
#include "media_wavelet_nr.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是分解层数。 */
    const char *out = argc > 1 ? argv[1] : "wavelet_nr.nv12";
    int levels = argc > 2 ? atoi(argv[2]) : 1;
    MEDIA_WAVELET_NR_CL_ATTR a;
    int rc;

    /* [2] 模块属性：
     *     levels     —— 分解层数，越多能压制越大尺度的噪声，也更慢
     *     threshold_y —— 亮度阈值，写成 6/255 是为了让读者看清它是归一化值
     *     strength   —— 收缩强度
     *     uv_*       —— 色度通道是否也降噪及其强度 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.output_pool_id = 21;
    a.levels = levels;
    a.threshold_y = 6.f / 255.f;
    a.strength = .60f;
    a.uv_enable = 1;
    a.uv_strength = .5f;

    /* [3] 初始化并加载 TPG + WAVELET_NR。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_wavelet_nr.so") != 0)
        return 1;

    /* [4] 输入输出都是 NV12、同尺寸。 */
    rc = tutorial_run_pipeline("WAVELET_NR_CL", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
