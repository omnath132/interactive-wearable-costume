#include "../costume_system/Engine.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace costume;
int main(){
 Diagnostics ring;for(unsigned i=0;i<70;i++)ring.record(i,"event=%u",i);
 assert(ring.size()==64&&ring.overwritten==6&&ring.at(0).sequence==7&&ring.at(63).sequence==70);
 assert(!ring.find(1)&&ring.find(7)&&!strcmp(ring.find(7)->message,"event=6"));
 char large[400];memset(large,'x',399);large[399]=0;ring.record(0xFFFFFFF0u,"%s",large);
 assert(strlen(ring.at(63).message)==143&&ring.at(63).ms==0xFFFFFFF0u);
 Engine e;e.touch(0,0,true,0);unsigned baseline=e.log.size();
 e.touch(0,5,true,100);assert(e.log.size()==baseline+2);
 assert(strstr(e.log.at(baseline).message,"source=physical arm=left electrode=0"));
 assert(strstr(e.log.at(baseline+1).message,"electrode=2"));
 e.touch(0,5,true,500);assert(e.log.size()==baseline+2); // Stable holds do not flood the log.
 e.touch(0,4,true,1100);assert(strstr(e.log.at(e.log.size()-1).message,"TOUCH_UP")&&strstr(e.log.at(e.log.size()-1).message,"held_ms=1000"));
 e.touch(0,0,false,1200);assert(strstr(e.log.at(e.log.size()-1).message,"TOUCH_LOST"));
 unsigned count=e.log.size();e.touch(0,0,false,1300);assert(e.log.size()==count);
 e.touch(0,0,true,1400);assert(strstr(e.log.at(e.log.size()-1).message,"TOUCH_BASELINE"));
 e.touch(1,1,true,0xFFFFFF00u,true);e.touch(1,0,true,0x100u,true);
 assert(strstr(e.log.at(e.log.size()-1).message,"source=simulated")&&strstr(e.log.at(e.log.size()-1).message,"held_ms=512"));
 puts("Diagnostic checks passed: bounded history, truncation, electrode edges, disconnect/reconnect, simulation labels, timer rollover");
}
