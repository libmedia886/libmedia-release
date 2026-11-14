/*
 * RESIZE_RGA 入门教程：用 RGA 硬件缩放
 *
 * 把 640x360 缩到目标尺寸（默认 320x180，正好一半）。keep_aspect_ratio=1
 * 表示保持宽高比：如果目标尺寸比例和原图不同，会按比例居中放置而不是拉伸。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 创建 TPG 作为输入源。
 *
 * 阅读顺序：
 *   [1] 解析两个尺寸参数 -> [2] 填模块属性 -> [3] 初始化 + 加载插件
 *   -> [4] 跑管线（输出尺寸来自命令行）
 *
 * 本文件是唯一有两个尺寸参数的教程，所以 runner 的输出宽高必须传变量，
 * 不能像别的例子那样写死 640x360。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_resize_rga.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 参数：输出文件名、目标宽、目标高。
     *     位置和 README 的参数表对应。 */
    const char *out = argc > 1 ? argv[1] : "resize_rga.nv12";
    int outw = argc > 2 ? atoi(argv[2]) : 320;
    int outh = argc > 3 ? atoi(argv[3]) : 180;
    MEDIA_RESIZE_RGA_ATTR a;
    int rc;

    /* [2] 模块属性。src_* 是源图尺寸，input_* 是实际送进来的帧尺寸，
     *     两者在简单场景下相同；out_* 是缩放目标。 */
    memset(&a, 0, sizeof(a));
    a.src_width = 640;
    a.src_height = 360;
    a.input_width = 640;
    a.input_height = 360;
    a.input_format = MEDIA_FORMAT_NV12;
    a.out_width = outw;
    a.out_height = outh;
    a.output_format = MEDIA_FORMAT_NV12;
    a.output_pool_id = 21;
    a.keep_aspect_ratio = 1;

    /* [3] 初始化并加载 TPG + RESIZE_RGA。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_resize_rga.so") != 0)
        return 1;

    /* [4] 输入固定 640x360，输出用命令行给的尺寸。
     *     runner 会用 outw/outh 计算输出 Pool 大小。 */
    rc = tutorial_run_pipeline("RESIZE_RGA", out, 640, 360, MEDIA_FORMAT_NV12, outw, outh,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
