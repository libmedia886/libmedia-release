/*
 * TNR 入门教程：时域降噪（Temporal Noise Reduction）
 *
 * TNR 把当前帧和前面几帧做加权平均，静止区域的噪声会被平均掉，
 * 运动区域则用阈值判断后保留当前帧，避免拖影。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 会创建 TPG 作为输入源。
 * 注意 runner 对 TNR_CL 会用“移动方块”图案而不是彩条——只有画面在动，
 * 才能看出静止区和运动区在阈值两侧的不同表现。
 *
 * 阅读顺序：
 *   [1] 解析命令行 -> [2] 填模块属性 -> [3] 初始化 + 加载插件
 *   -> [4] 跑管线（处理 2 帧，因为时域算法至少要两帧才有历史）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_tnr.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是运动阈值。 */
    const char *out = argc > 1 ? argv[1] : "tnr.nv12";
    float threshold = argc > 2 ? (float)atof(argv[2]) : .12f;
    MEDIA_TNR_CL_ATTR a;
    int rc;

    /* [2] 模块属性。两个 alpha 决定“新旧帧各占多少”：
     *     static_alpha 用于静止区（越小越平滑），
     *     motion_alpha 用于运动区（越大越信任当前帧，拖影越少）。 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.output_pool_id = 21;
    a.block_size = 16;      /* 按 16x16 块判断静止/运动 */
    a.threshold = threshold;
    a.static_alpha = .35f;
    a.motion_alpha = .88f;

    /* [3] 初始化并加载 TPG + TNR 两个插件。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_tnr.so") != 0)
        return 1;

    /* [4] 最后一个参数是 2：时域降噪需要历史帧，只处理一帧看不到效果。
     *     runner 会把两帧分别存成 <out>.00 和 <out>.01。 */
    rc = tutorial_run_pipeline("TNR_CL", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 2);

    MEDIA_SYS_Exit();
    return rc;
}
