/*
 * VO_WBC 入门教程：探测 writeback（回写抓帧）能力
 *
 * VO_WBC 利用 DRM writeback connector，把某个 CRTC 当前合成的画面回写成
 * DMA-BUF——相当于“从显示器抓一帧”。适合录屏、截图、把画面再送去编码。
 *
 * 本程序只做能力探测，不真的抓帧：不同板卡的 writeback connector、活动 CRTC
 * 和桌面占用状态差别很大，没探测就抓帧只会得到误导性的黑图或失败。
 *
 * 阅读顺序：
 *   [1] 初始化 -> [2] 探测 connector_id / crtc_id -> [3] 打印 -> [4] 退出
 *
 * 失败时先自己确认环境（两个命令都在板端跑）：
 *   modetest -M rockchip -c
 *   ls -l /dev/dri/card*
 * 需要一个支持 writeback 的 connector 和一个活动的 CRTC；桌面 compositor
 * 独占 DRM master 时，探测也可能被打断。
 */
#include <stdio.h>

#include "media_api.h"
#include "media_vo_wbc.h"

int main(void)
{
    int conn = 0, crtc = 0;
    int r;

    /* [1] 先初始化。 */
    if (MEDIA_SYS_Init() != 0)
        return 1;

    /* [2] 探测。传 NULL 表示不指定 DRM 设备，由插件自动选择；
     *     找到后库会把 id 写进 conn / crtc。 */
    r = MEDIA_VO_WBC_Probe(NULL, &conn, &crtc);
    if (r != 0) {
        fprintf(stderr,
                "VO_WBC writeback probe failed: %d "
                "(needs DRM writeback connector and active CRTC)\n",
                r);
        MEDIA_SYS_Exit();
        return 2;
    }

    /* [3] 探测成功。真正抓帧时要把这两个 id 填进 MEDIA_VO_WBC_ATTR。 */
    printf("VO_WBC connector_id=%d crtc_id=%d available\n", conn, crtc);

    /* [4] 退出。返回 0 表示环境具备 writeback 条件，但不代表抓帧一定成功。 */
    MEDIA_SYS_Exit();
    return 0;
}
