#include <stdio.h>
#include "media_api.h"
#include "media_vo_wbc.h"
int main(void){int conn=0,crtc=0;if(MEDIA_SYS_Init()!=0)return 1;int r=MEDIA_VO_WBC_Probe(NULL,&conn,&crtc);if(r!=0){fprintf(stderr,"VO_WBC writeback probe failed: %d (needs DRM writeback connector and active CRTC)\n",r);MEDIA_SYS_Exit();return 2;}printf("VO_WBC connector_id=%d crtc_id=%d available\n",conn,crtc);MEDIA_SYS_Exit();return 0;}
