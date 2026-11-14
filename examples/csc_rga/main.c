/*
 * CSC_RGA 入门教程：用 RGA 硬件做色彩空间转换（NV12 -> RGB888）
 *
 * CSC = Color Space Conversion。视频常以 NV12（YUV）存放，显示或算法常要
 * RGB，中间必须做一次转换。这里走 RGA 硬件，不占 CPU。
 *
 * 有多种标准与取值范围，选错会导致颜色偏淡或过饱和：
 *   BT601 用于标清，BT709 用于高清；
 *   LIMIT 表示 Y 在 16..235 的“有限范围”，FULL 表示 0..255。
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
#include "media_csc_rga.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是转换模式（默认 BT601 有限范围）。 */
    const char *out = argc > 1 ? argv[1] : "csc_rga.rgb";
    int mode = argc > 2 ? atoi(argv[2]) : CSC_RGA_CSC_BT601_LIMIT;
    MEDIA_CSC_RGA_ATTR a;
    int rc;

    /* [2] 模块属性。注意字段名是 input_width / input_height（不是 width/height），
     *     看着相似但结构体不同，照抄别的例子会编译失败。 */
    memset(&a, 0, sizeof(a));
    a.input_width = 640;
    a.input_height = 360;
    a.input_format = MEDIA_FORMAT_NV12;
    a.output_format = MEDIA_FORMAT_RGB888;
    a.output_pool_id = 21;
    a.csc_mode = mode;

    /* [3] 初始化并加载 TPG + CSC_RGA。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_csc_rga.so") != 0)
        return 1;

    /* [4] 输入 NV12、输出 RGB888，尺寸不变。 */
    rc = tutorial_run_pipeline("CSC_RGA", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_RGB888, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
