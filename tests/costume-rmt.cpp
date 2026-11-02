#include "../costume_system/CostumeRmtOutput.h"
#include <assert.h>
#include <stdio.h>
int main(){
 CostumeRmtOutput<124> a,b;assert(a.begin(16)&&b.begin(17));
 uint8_t black[124*3]={},white[124*3];for(auto& byte:white)byte=255;
 a.service(black);assert(transmissions==1&&inFlight[0].duration0==4);
 b.service(white);assert(transmissions==1&&inFlight[0].duration0==4);
 a.markDirty();a.service(white);assert(transmissions==1&&inFlight[0].duration0==4);
 channels[0].done=true;a.service(white);assert(a.frames()==1&&transmissions==1);
 b.service(white);assert(transmissions==2&&inFlight[0].duration0==8);
 a.service(black);assert(transmissions==2&&inFlight[0].duration0==8);
 channels[1].done=true;b.service(white);assert(b.frames()==1);
 a.service(black);assert(transmissions==3&&inFlight[0].duration0==4);
 channels[0].done=true;a.service(black);b.service(white);assert(transmissions==3);
 puts("Shared RMT checks passed: no in-flight overwrite, nonblocking wait, fair output handoff, dirty frame retained");
}
