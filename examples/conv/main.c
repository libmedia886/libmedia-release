/*
 * CONV 入门教程：可调核大小的卷积（模糊 / 锐化 / 边缘）
 *
 * 卷积就是把每个像素换成“周围一圈像素的加权和”。核越大，影响范围越宽：
 * k=3 轻微模糊，k=15 明显模糊，k=31 非常模糊。核大小必须是奇数，
 * 因为它要以当前像素为中心左右对称。
 *
 * 本例不读摄像头：examples/common/filter_runner.h 创建 TPG 作为输入源。
 *
 * 阅读顺序：
 *   [1] 解析并校验命令行 -> [2] 填模块属性 -> [3] 初始化 + 加载插件
 *   -> [4] 跑管线
 *
 * 本文件比其它教程多一步参数校验，因为核大小有硬性限制（奇数、1..31）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_api.h"
#include "media_conv.h"
#include "../common/filter_runner.h"

int main(int argc, char **argv)
{
    /* [1] 第 1 个参数是输出文件名，第 2 个是核大小。 */
    const char *out = argc > 1 ? argv[1] : "conv.nv12";
    int k = argc > 2 ? atoi(argv[2]) : 5;
    MEDIA_CONV_CL_ATTR a;
    int rc;

    /* 先校验再建管线：参数非法时尽早退出，不要把错误带进模块。
     * 偶数核没有中心像素，所以这里要求 k 为奇数。 */
    if (k < 1 || k > 31 || !(k & 1)) {
        fprintf(stderr, "kernel must be odd 1..31\n");
        return 2;
    }

    /* [2] 模块属性。 */
    memset(&a, 0, sizeof(a));
    a.width = 640;
    a.height = 360;
    a.format = MEDIA_FORMAT_NV12;
    a.kernel_size = k;
    a.output_pool_id = 21;

    /* [3] 初始化并加载 TPG + CONV。 */
    if (MEDIA_SYS_Init() != 0 ||
        tutorial_load_pair(TUTORIAL_PLUGIN_DIR, "libmedia_conv.so") != 0)
        return 1;

    /* [4] 输入输出都是 NV12、同尺寸。 */
    rc = tutorial_run_pipeline("CONV_CL", out, 640, 360, MEDIA_FORMAT_NV12, 640, 360,
                               MEDIA_FORMAT_NV12, &a, sizeof(a), 1);

    MEDIA_SYS_Exit();
    return rc;
}
