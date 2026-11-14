/*
 * CLAHE 入门教程：限制对比度自适应直方图均衡
 *
 * CLAHE 把画面切成若干小格，在每格内拉开亮度差异，再把格子边界平滑连起来。
 * 适合雾天、背光、灰暗画面；格子越细（grid 越大）局部适应越强，噪声也越明显。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 里的 tutorial_run_pipeline
 * 会创建 TPG（测试图案源）作为输入，把 TPG 输出 Bind 到本模块输入，
 * 取一帧结果写文件。所以本文件只需要描述“模块参数”，生命周期都在 runner 里。
 *
 * 阅读顺序：
 *   [1] 解析命令行 -> [2] 填模块属性结构体 -> [3] 初始化 + 加载插件
 *   -> [4] 交给 runner 跑完整条管线 -> [5] 退出
 *
 * 想理解 TPG / Pool / Bind / GetFrame 这些步骤，看 examples/tpg 或
 * examples/vmix 的 main.c，那里是完整展开的。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_clahe.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是格子数。
     *     位置参数和 README 的“参数速查”表一一对应。 */
    const char *out = argc > 1 ? argv[1] : "clahe.nv12";
    int grid = argc > 2 ? atoi(argv[2]) : 8;
    MEDIA_CLAHE_ATTR a;
    int rc;

    /* [2] 模块属性。整块结构体必须先 memset 清零，
     *     否则未初始化的字段会被当成有效值传给模块。 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.tile_grid_x = grid;              /* 横向分成几格 */
    a.tile_grid_y = grid;              /* 纵向分成几格 */
    a.clip_limit = 2.0f;               /* 对比度限制，越大越强也越噪 */
    a.highlight_protect_start = 0.0f;  /* 高光保护起点，0 表示关闭 */
    a.highlight_protect_strength = 1.0f;
    a.output_pool_id = 21;             /* 输出 Pool 编号，与 runner 中的约定一致 */

    /* [3] 初始化系统并加载插件。tutorial_load_pair 会先加载 TPG 再加载本模块，
     *     两者都必须有配对的 .so 和 .so.sig。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_clahe.so") != 0)
        return 1;

    /* [4] 参数顺序：模块类型串、输出路径、输入宽高格式、输出宽高格式、
     *     属性结构体指针与字节数、处理帧数。 */
    rc = tutorial_run_pipeline("CLAHE", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    /* [5] 无论成功失败都要释放系统资源。 */
    MEDIA_SYS_Exit();
    return rc;
}
