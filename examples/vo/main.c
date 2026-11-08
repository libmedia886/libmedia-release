#include <stdio.h>
#include <string.h>
#include "media_api.h"
#include "media_vo.h"
int main(void){MEDIA_VO_CONNECTOR_INFO c[16];int n=0;if(MEDIA_SYS_Init()!=0)return 1;int r=MEDIA_VO_ProbeConnectors(c,16,&n);if(r!=0){fprintf(stderr,"VO probe failed: %d\n",r);MEDIA_SYS_Exit();return 1;}printf("VO connector count=%d\n",n);for(int i=0;i<n;i++)printf("%s connected=%d mode=%dx%d@%d\n",c[i].name,c[i].connected,c[i].width,c[i].height,c[i].fps);MEDIA_SYS_Exit();return n?0:2;}
