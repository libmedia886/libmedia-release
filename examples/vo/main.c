/*
 * VO 入门教程：只是探测显示接口，不做任何输出
 *
 * VO（Video Output）负责把视频帧提交给 DRM/KMS 显示控制器。真正显示之前，
 * 必须先知道板子上有哪些 connector（HDMI、DP、MIPI-DSI……）、哪个接了显示器、
 * 支持什么分辨率。
 *
 * 本程序只调用 MEDIA_VO_ProbeConnectors：不创建 plane、不抢 DRM master，
 * 所以不会干扰正在运行的桌面，适合第一次上手时确认环境。
 *
 * 阅读顺序：
 *   [1] 初始化 -> [2] 探测 connector -> [3] 打印结果 -> [4] 退出
 *
 * 想真正出画面，看 README 的“2. 从探测到真正显示”，需要自己创建 VO channel
 * 并把上游模块 Bind 到它的 input0。
 */
#include <stdio.h>
#include <string.h>

#include "media_api.h"
#include "media_vo.h"

#define MAX_CONNECTORS 16  /* 一块板子上的 connector 不会超过这个数 */

int main(void)
{
    MEDIA_VO_CONNECTOR_INFO c[MAX_CONNECTORS];
    int n = 0;
    int r;

    /* [1] 使用任何 media API 之前先初始化。 */
    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [2] 探测。第二个参数是数组容量，第三个是实际找到的数量。
     *     n 由库填写，不要自己预设。 */
    r = MEDIA_VO_ProbeConnectors(c, MAX_CONNECTORS, &n);
    if (r != 0) {
        fprintf(stderr, "VO probe failed: %d\n", r);
        MEDIA_SYS_Exit();
        return 1;
    }

    /* [3] 打印每个 connector。connected=0 表示接口存在但没插显示器。
     *     width/height/fps 是首选 mode，不是全部可用 mode。 */
    printf("VO connector count=%d\n", n);
    for (int i = 0; i < n; i++) {
        printf("%s connected=%d mode=%dx%d@%d\n", c[i].name, c[i].connected, c[i].width,
               c[i].height, c[i].fps);
    }

    /* [4] 退出。返回 2 表示“没探测到任何 connector”，
     *     方便脚本用退出码判断，而不是去解析输出文本。 */
    MEDIA_SYS_Exit();
    return n ? 0 : 2;
}
