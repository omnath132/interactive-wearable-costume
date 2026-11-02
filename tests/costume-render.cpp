#include "../costume_system/Engine.h"
#include "fakes/RenderString.h"
#include <stdio.h>
uint32_t fakeTime=0;
uint32_t millis(){return fakeTime;}
#include "../costume_system/RenderState.h"
using namespace costume;
String color(RGB c){char b[8];snprintf(b,8,"#%02x%02x%02x",c.r,c.g,c.b);return String(b);}
String snapshot(Engine& e,uint32_t now){
 fakeTime=now;
 String j="{\"idleGlow\":"+String(e.idleGlow?"true":"false")+",\"effect\":"+String(unsigned(e.activeShot()))+",\"touch\":["+String(unsigned(e.touchMask(0)))+","+String(unsigned(e.touchMask(1)))+"],\"render\":"+renderState(e,123)+",\"sections\":[";
 for(unsigned s=0;s<SECTIONS;s++){
  if(s)j+=',';const auto& m=e.manual[s];
  j+="{\"length\":"+String(LENGTHS[s])+",\"manual\":"+String(m.active?"true":"false")+",\"color\":\""+color(m.color)+"\",\"brightness\":"+String(unsigned(m.brightness))+",\"pattern\":"+String(unsigned(m.pattern))+",\"speed\":"+String(m.speed)+",\"reverse\":"+String(m.reverse?"true":"false")+",\"paint\":[";
  bool comma=false;for(unsigned i=0;i<LENGTHS[s];i++)if(e.paint[s][i].used){const auto& p=e.paint[s][i];if(comma)j+=',';comma=true;j+="["+String(i)+",\""+color(p.color)+"\","+String(p.at)+","+String(p.fade?"true":"false")+"]";}
  j+="]}";
 }return j+"]}";
}
void check(Engine& e,const String& state,uint32_t now){
 e.render(now);printf("{\"state\":%s,\"now\":%u,\"pixels\":[",state.c_str(),now);
 for(unsigned s=0;s<SECTIONS;s++){if(s)printf(",");printf("[");for(unsigned i=0;i<LENGTHS[s];i++){if(i)printf(",");auto c=e.pixels[s][i];printf("[%u,%u,%u]",c.r,c.g,c.b);}printf("]");}printf("]}\n");
}
int main(){
 Engine e;e.idleGlow=true;auto idle=snapshot(e,100);check(e,idle,100);check(e,idle,800);
 e.touch(0,0,true,100);e.touch(0,1,true,150);auto held=snapshot(e,150);check(e,held,150);check(e,held,1000);check(e,held,2650);
 e.touch(0,0,true,2700);auto released=snapshot(e,2700);check(e,released,2800);check(e,released,5900);check(e,released,6000);check(e,released,6200);
 Engine collision;collision.step(0,0);collision.step(1,0);auto before=snapshot(collision,1950);check(collision,before,1980);check(collision,before,2001);check(collision,before,2100);
 Engine post;post.step(0,0);post.step(1,0);post.render(2060);auto after=snapshot(post,2060);check(post,after,2100);check(post,after,2400);
 Engine controls;controls.manual[0].active=true;controls.manual[0].pattern=CHASE;controls.manual[0].reverse=true;controls.manual[1].active=true;controls.manual[1].pattern=BREATHE;controls.brush(HEART,9,8,{0,100,255},true,100);auto manual=snapshot(controls,100);check(controls,manual,700);check(controls,manual,2600);
 for(unsigned fx=1;fx<=3;fx++){Engine shot;shot.oneShot(OneShot(fx),100);auto state=snapshot(shot,100);for(unsigned t=100;t<2800;t+=130)check(shot,state,t);}
 Engine heart;for(unsigned i=0;i<=4;i++)heart.heart.beat(i*1000);auto beats=snapshot(heart,4100);check(heart,beats,4180);check(heart,beats,5000);check(heart,beats,5200);
 Engine wrap;wrap.launch(LEFT_ARM,7,0xFFFFFF00u);auto rollover=snapshot(wrap,0xFFFFFF00u);check(wrap,rollover,0xFFFFFFC8u);check(wrap,rollover,0x00000020u);
}
