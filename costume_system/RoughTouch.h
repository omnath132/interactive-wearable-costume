#pragma once
#include "Config.h"
namespace costume {
// Require a normal reading before accepting a low reading. This avoids treating
// a stopped sensor returning all zeroes at startup as twelve touches.
class RoughTouchDetector {
  uint16_t armed=0,held=0,pending=0,history[12][3]={},median[12]={};
  uint32_t candidateAt[12]={},lastSample=0;
  unsigned index=0,count=0;bool sampled=false;
public:
  void reset(){armed=held=pending=0;index=count=0;sampled=false;}
  uint16_t pendingMask()const{return pending;}
  uint16_t filtered(unsigned e)const{return median[e];}
  uint16_t sample(const uint16_t* readings,uint32_t now){
    // A long sampling gap is not evidence of continuous touch/release stability.
    if(sampled&&uint32_t(now-lastSample)>250){pending=0;count=index=0;}
    sampled=true;lastSample=now;
    for(unsigned e=0;e<12;e++)history[e][index]=readings[e];
    index=(index+1)%3;if(count<3)count++;
    if(count<3)return held;
    for(unsigned e=0;e<12;e++){
      uint16_t a=history[e][0],b=history[e][1],c=history[e][2];
      if(a>b){uint16_t t=a;a=b;b=t;}if(b>c){uint16_t t=b;b=c;c=t;}if(a>b)b=a;
      median[e]=b;
      const uint16_t bit=uint16_t(1u<<e);
      if(b>=ROUGH_TOUCH_IDLE)armed|=bit;
      bool releasing=(held&bit)!=0;
      bool candidate=releasing?b>=ROUGH_TOUCH_IDLE:((armed&bit)&&b<ROUGH_TOUCH_BELOW);
      if(!candidate){pending&=~bit;continue;}
      if(!(pending&bit)){pending|=bit;candidateAt[e]=now;continue;}
      if(uint32_t(now-candidateAt[e])>=(releasing?ROUGH_TOUCH_RELEASE_MS:ROUGH_TOUCH_PRESS_MS)){
        if(releasing)held&=~bit;else held|=bit;
        pending&=~bit;
      }
    }return held;
  }
};
}
