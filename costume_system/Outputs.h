#pragma once
#include "Engine.h"
#include "CostumeRmtOutput.h"
#include <driver/gpio.h>
namespace costume {
class Outputs {
  CostumeRmtOutput<MAX_PIXELS> tx[SECTIONS];
  uint8_t frame[MAX_PIXELS*3]={};uint32_t last=0;
public:
  bool configured=false;
  bool begin(){
    if(!ENABLE_LEDS)return false;
    if(!ELECTRODES_CONFIRMED)return false;
    for(unsigned s=0;s<SECTIONS;s++){
      if(LED_PINS[s]<0 || !GPIO_IS_VALID_OUTPUT_GPIO(LED_PINS[s]) || LED_PINS[s]==SDA_PIN || LED_PINS[s]==SCL_PIN || !LENGTHS[s] || LENGTHS[s]>MAX_PIXELS)return false;
      for(unsigned bus=0;bus<2;bus++)if(LED_PINS[s]==SDA_PINS[bus]||LED_PINS[s]==SCL_PINS[bus])return false;
      for(unsigned p=0;p<s;p++)if(LED_PINS[p]==LED_PINS[s])return false;
    }
    for(unsigned arm=0;arm<2;arm++)for(const auto& r:ELECTRODES[arm])
      if(r.first<0||r.last<r.first||unsigned(r.last)>LENGTHS[arm])return false;
    configured=true;bool ok=true;
    for(unsigned s=0;s<SECTIONS;s++)if(!tx[s].begin(LED_PINS[s]))ok=false;
    return ok;
  }
  bool ready(unsigned s)const{return tx[s].ready();}
  uint32_t frames(unsigned s)const{return tx[s].frames();}
  const char* error(unsigned s)const{return esp_err_to_name(tx[s].error());}
  void tick(const Engine& e,uint32_t now){
    if(!configured)return;
    bool fresh=uint32_t(now-last)>=20;if(fresh)last=now;
    for(unsigned s=0;s<SECTIONS;s++){
      if(fresh)tx[s].markDirty();
      memset(frame,0,sizeof(frame));
      for(unsigned i=0;i<LENGTHS[s];i++){
        unsigned physical=REVERSE[s]?LENGTHS[s]-1-i:i;
        RGB c=e.pixels[s][i];frame[physical*3]=c.g;frame[physical*3+1]=c.r;frame[physical*3+2]=c.b;
      }
      // service snapshots into each output's private symbol buffer before returning.
      tx[s].service(frame);
    }
  }
};
}
