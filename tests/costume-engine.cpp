#include "../costume_system/Engine.h"
#include "../costume_system/Access.h"
#include "../costume_system/RoughTouch.h"
#include <assert.h>
#include <stdio.h>
using namespace costume;
// Count visible blue effect pixels.
static unsigned lit(const Engine& e,unsigned s){unsigned n=0;for(unsigned i=0;i<LENGTHS[s];i++)if(e.pixels[s][i].b>20)n++;return n;}
int main(){
 Engine e;e.touch(0,0,true,0);e.touch(0,1,true,100);e.render(100);assert(lit(e,0)>0);
 e.touch(0,1,true,500);e.render(500);assert(lit(e,SPINE)==0);
 e.touch(0,0,true,600);e.render(610);assert(lit(e,0)>0);
 e.render(3900);assert(lit(e,SPINE)>0);
 Engine loss;loss.touch(0,1,true,0);loss.touch(0,0,false,10);loss.render(600);assert(lit(loss,0)==0&&lit(loss,SPINE)==0);
 loss.touch(0,0,true,700);loss.render(1000);assert(lit(loss,0)==0);
 Engine manual;manual.manual[0].active=true;manual.touch(0,1,true,0);manual.touch(0,0,true,100);manual.sensors(0);manual.render(500);assert(lit(manual,0)==0);
 manual.step(0,600);manual.manual[SPINE].active=true;manual.manual[SPINE].color={0,0,0};manual.render(1000);assert(lit(manual,LEFT_LEG)>0&&lit(manual,SPINE)==0);
 manual.brush(HEART,9,10,{0,0,255},false,100);manual.render(10000);assert(manual.manual[HEART].active&&lit(manual,HEART)==10);
 manual.sensors(HEART);manual.render(10001);assert(manual.pixels[HEART][0].r==0&&lit(manual,HEART)==0);
 manual.brush(1,10,10,{0,0,255},true,0);manual.render(FADE_MS+1);assert(lit(manual,1)==0);
 for(unsigned i=0;i<MAX_PULSES+2;i++)manual.step(1,100);assert(manual.dropped>=2);
 HeartClock h;assert(h.level(0)==0);assert(h.beat(0));assert(h.flashes==1&&h.level(0)==160);
 assert(!h.beat(100));assert(h.rejected==1);
 for(unsigned i=1;i<4;i++)assert(h.beat(i*1000));assert(h.period==0);
 assert(h.beat(4000));assert(h.period==1000);assert(h.flashes==5);
 h.tick(5000);assert(h.flashes==6);assert(h.beat(5060));assert(h.flashes==6);
 h.disconnect();h.tick(h.nextBeat);assert(h.flashes==7);h.tick(100000);assert(h.flashes==8&&!h.live(100000));
 HeartClock tempo;for(unsigned i=0;i<5;i++)tempo.beat(i*1000);
 assert(!tempo.beat(4500));assert(!tempo.beat(5000));assert(tempo.beat(5500));
 for(unsigned i=1;i<=3;i++)tempo.beat(5500+i*500);assert(tempo.period==500);
 HeartClock wrap;uint32_t base=0xFFFFF000u;for(unsigned i=0;i<5;i++)wrap.beat(base+i*1000u);
 wrap.tick(base+5000u);assert(wrap.flashes==6&&wrap.period==1000);
 Access a;const char* owner="11111111111111111111111111111111";const char* guest="22222222222222222222222222222222";
 int o=a.create(owner,true,0),v=a.create(guest,false,0);assert(a.owner(o)&&!a.view(v)&&!a.command(v,0));
 assert(a.setOpen(o,true)&&a.view(v));assert(a.claim(v,0));assert(!a.claim(a.create("33333333333333333333333333333333",false,0),0));
 assert(a.claim(o,10)&&!a.command(v,10));assert(a.setOpen(o,false)&&!a.view(v));assert(a.find(guest,11)==-1);
 a.tick(LEASE_MS+11);assert(a.controller==-1);assert(a.claim(o,LEASE_MS+12));a.tick(SESSION_MS+1);assert(!a.view(o));
 Access rollover;int r=rollover.create(owner,true,0xFFFFFFF0u);assert(rollover.claim(r,0xFFFFFFF0u));rollover.tick(40);assert(rollover.controller==r);rollover.tick(LEASE_MS+40);assert(rollover.controller==-1);
 StepDetector detector;for(unsigned i=0;i<60;i++)assert(!detector.sample(1,0.18f,i*20));
 assert(detector.sample(1.5f,0.18f,1300));assert(!detector.sample(1.6f,0.18f,1320));
 detector.sample(1,0.18f,1500);assert(detector.sample(1.6f,0.18f,1700));
 RoughTouchDetector rough;uint16_t readings[12]={};uint32_t touchTime=0;
 auto feed=[&](uint16_t value,unsigned count){uint16_t mask=0;readings[0]=value;for(unsigned i=0;i<count;i++){touchTime+=50;mask=rough.sample(readings,touchTime);}return mask;};
 assert(feed(0,10)==0);assert(feed(10,3)==0);assert(feed(5,3)==0);
 assert(feed(4,3)==0);assert(feed(4,1)==1); // 100 ms stable low after median.
 assert(feed(14,1)==1);assert(feed(3,3)==1); // A single spike cannot release.
 assert(feed(12,3)==1);assert(feed(3,3)==1); // A short burst cannot release.
 assert(feed(5,3)==1);assert(feed(6,8)==1);assert(feed(6,2)==0);
 assert(feed(3,1)==0);assert(feed(10,3)==0); // A single low spike cannot press.
 rough.reset();assert(feed(0,10)==0); // No manufactured touch after reconnect.
 RoughTouchDetector wrapTouch;uint16_t wrapReadings[12]={};uint32_t wrapBase=0xFFFFFF00u;wrapReadings[0]=10;
 for(unsigned i=0;i<3;i++)assert(wrapTouch.sample(wrapReadings,wrapBase+i*50)==0);
 wrapReadings[0]=3;for(unsigned i=3;i<=6;i++)wrapTouch.sample(wrapReadings,wrapBase+i*50);
 assert(wrapTouch.sample(wrapReadings,wrapBase+350)==1);
 wrapReadings[0]=8;assert(wrapTouch.sample(wrapReadings,wrapBase+2000)==1); // Gap is not stability.
 for(unsigned i=1;i<=10;i++)wrapTouch.sample(wrapReadings,wrapBase+2000+i*50);
 assert(wrapTouch.sample(wrapReadings,wrapBase+2550)==0);
 Engine idle;for(unsigned t=0;t<=5000;t+=2500){idle.render(t);for(unsigned s=0;s<SECTIONS;s++)for(unsigned i=0;i<LENGTHS[s];i++)assert(idle.pixels[s][i].r==0&&idle.pixels[s][i].g==0&&idle.pixels[s][i].b==0);}
 Engine shortHold,longHold;shortHold.touch(0,0,true,0);shortHold.touch(0,1,true,100);shortHold.render(100);unsigned initial=shortHold.pixels[0][shortHold.regionStart(0,0)].b;
 shortHold.touch(0,0,true,200);shortHold.render(210);
 longHold.touch(0,0,true,0);longHold.touch(0,1,true,100);longHold.render(2600);assert(longHold.pixels[0][longHold.regionStart(0,0)].b>initial);
 longHold.touch(0,0,true,2600);longHold.render(2610);assert(longHold.pixels[0][longHold.regionStart(0,0)].b>shortHold.pixels[0][shortHold.regionStart(0,0)].b);
 Engine stagger;stagger.touch(0,1,true,100);stagger.touch(0,3,true,2100);assert(stagger.charge(0,0,2600)==1&&stagger.charge(0,1,2600)<0.21f);
 Engine collision;collision.step(0,0);collision.step(1,0);collision.render(2060);assert(collision.collisions==1&&collision.pixels[SPINE][3].r>collision.pixels[SPINE][3].b&&collision.pixels[SPINE][3].g>collision.pixels[SPINE][3].b);
 collision.render(2080);assert(collision.collisions==1);collision.render(10000);collision.step(0,11000);collision.step(1,11000);collision.render(13060);assert(collision.collisions==2);
 Engine masked;masked.manual[SPINE].active=true;masked.step(0,0);masked.step(1,0);masked.render(2060);assert(masked.collisions==0);
 Engine shot;shot.manual[0].active=true;shot.manual[0].color={0,100,0};shot.oneShot(LIGHTNING,0);shot.render(0);assert(shot.pixels[0][0].r==160);
 shot.render(700);assert(shot.activeShot()==NO_SHOT&&shot.manual[0].active&&shot.pixels[0][0].r==0);
 shot.oneShot(HEART_RESTART,1000);shot.render(1100);for(unsigned s=0;s<SECTIONS;s++)for(unsigned i=0;i<LENGTHS[s];i++)assert(shot.pixels[s][i].r==0&&shot.pixels[s][i].g==0&&shot.pixels[s][i].b==0);
 shot.render(1900);assert(shot.pixels[HEART][0].r==160&&shot.heart.flashes==0);
 shot.render(3400);assert(shot.activeShot()==NO_SHOT&&shot.manual[0].active);
 shot.oneShot(ENERGY_WAVE,100);shot.render(300);assert(shot.pixels[0][8].r>0&&shot.manual[0].active);
 shot.oneShot(ENERGY_WAVE,0xFFFFFF00u);shot.render(uint32_t(0xFFFFFF00u+2200u));assert(shot.activeShot()==NO_SHOT);
 shot.oneShot(LIGHTNING,10);shot.oneShot(HEART_RESTART,20);assert(shot.activeShot()==HEART_RESTART);shot.oneShot(NO_SHOT,30);shot.render(30);assert(shot.pixels[0][0].r==0);
 for(unsigned arm=0;arm<2;arm++)for(unsigned electrode=0;electrode<12;electrode++){
  Engine mapped;mapped.touch(arm,0,true,0);mapped.touch(arm,1u<<electrode,true,100);mapped.render(2600);
  unsigned start=mapped.regionStart(arm,electrode),end=mapped.regionEnd(arm,electrode);
  assert(end<=LENGTHS[arm]&&start<=end);
  for(unsigned pixel=0;pixel<LENGTHS[arm];pixel++)assert((mapped.pixels[arm][pixel].b>20)==(pixel>=start&&pixel<end));
  mapped.touch(arm,0,true,2700);
  if(start==end){mapped.render(2710);assert(lit(mapped,arm)==0&&lit(mapped,SPINE)==0);}
 }
 Engine direction;direction.launch(LEFT_ARM,20,0);direction.render(200);
 assert(direction.pixels[LEFT_ARM][13].b>direction.pixels[LEFT_ARM][20].b&&lit(direction,SPINE)==0);
 Engine boundary;boundary.launch(LEFT_ARM,7,100);boundary.render(299);assert(lit(boundary,SPINE)==0);
 boundary.render(300);assert(lit(boundary,SPINE)==0); // Center is exactly LED 0.
 boundary.render(301);assert(lit(boundary,SPINE)>0); // Only after passing zero.
 Engine idleSpine;idleSpine.render(GHOST_BREATH_MS/2);
 for(unsigned i=0;i<LENGTHS[SPINE];i++)assert(idleSpine.pixels[SPINE][i].r==0&&idleSpine.pixels[SPINE][i].g==0&&idleSpine.pixels[SPINE][i].b==0);
 Engine linkage;linkage.launch(LEFT_ARM,7,100);linkage.render(301);
 assert(JUNCTION[LEFT_ARM]==LENGTHS[SPINE]-12);
 assert(linkage.pixels[SPINE][LENGTHS[SPINE]-12].b>0);
 for(unsigned i=0;i<LENGTHS[SPINE]-24;i++)assert(linkage.pixels[SPINE][i].b==0);
 linkage.render(450);assert(linkage.pixels[SPINE][90].b>linkage.pixels[SPINE][85].b);
 Engine yellow;yellow.step(0,0);yellow.step(1,0);yellow.render(2060);
 assert(yellow.pixels[SPINE][3].r>yellow.pixels[SPINE][3].b&&yellow.pixels[SPINE][3].g>yellow.pixels[SPINE][3].b);
 yellow.render(2400);assert(yellow.pixels[SPINE][15].b>yellow.pixels[SPINE][15].g&&yellow.pixels[SPINE][15].b>yellow.pixels[SPINE][15].r);
 Engine toggle;toggle.idleGlow=true;toggle.render(0);
 for(unsigned s=0;s<SPINE;s++)assert(toggle.pixels[s][0].b>toggle.pixels[s][0].g&&toggle.pixels[s][0].b>0);
 assert(toggle.pixels[SPINE][0].b==0&&toggle.pixels[HEART][0].r==0);
 toggle.idleGlow=false;toggle.render(10);
 for(unsigned s=0;s<SECTIONS;s++)for(unsigned i=0;i<LENGTHS[s];i++)assert(toggle.pixels[s][i].r==0&&toggle.pixels[s][i].g==0&&toggle.pixels[s][i].b==0);
 ThighStepDetector thigh;uint32_t tt=0;
 for(unsigned n=0;n<60;n++){tt+=50;assert(!thigh.sample(1,0,5,0,0.4f,tt));} // Learn gyro bias.
 assert(!thigh.sample(1.5f,0,5,0,0.4f,tt+=50)); // Acceleration alone.
 for(unsigned n=0;n<20;n++)assert(!thigh.sample(1,0,5,0,0.4f,tt+=50)); // Expire the impact.
 for(unsigned n=0;n<4;n++)assert(!thigh.sample(1,0,105,0,0.4f,tt+=50)); // Rotation alone.
 assert(thigh.rotationDegrees()>=15);
 assert(thigh.sample(1.5f,0,105,0,0.4f,tt+=50)); // Both: rotation first.
 for(unsigned n=0;n<10;n++)assert(!thigh.sample(1,0,5,0,0.4f,tt+=50));
 assert(!thigh.sample(1.5f,0,5,0,0.4f,tt+=50));
 bool combined=false;for(unsigned n=0;n<4;n++)combined|=thigh.sample(1,0,105,0,0.4f,tt+=50);
 assert(combined); // Both: acceleration first.
 assert(!thigh.sample(1,0,205,0,0.4f,tt+=1000)&&thigh.rotationDegrees()==0); // Gap adds no rotation.
 thigh.calibrate();assert(thigh.calibrationRemaining()==50);
 ThighStepDetector gentle;uint32_t gentleTime=0;
 for(unsigned n=0;n<60;n++)assert(!gentle.sample(1,0,0,0,DEFAULT_STEP_THRESHOLD_G,gentleTime+=50));
 assert(!gentle.sample(1.20f,0,0,0,DEFAULT_STEP_THRESHOLD_G,gentleTime+=50));
 bool gentleStep=false;for(unsigned n=0;n<4;n++)gentleStep|=gentle.sample(1,0,40,0,DEFAULT_STEP_THRESHOLD_G,gentleTime+=50);
 assert(gentleStep); // A small 0.2 g bump and 8-degree swing now register.
 Engine widePulse,footPulse;widePulse.launch(LEFT_ARM,20,0,0.35f);footPulse.step(0,0);
 widePulse.render(0);footPulse.render(1400); // Both centers are at LED 20.
 assert(lit(footPulse,LEFT_LEG)*2>=lit(widePulse,LEFT_ARM)-2&&lit(footPulse,LEFT_LEG)*2<=lit(widePulse,LEFT_ARM)+2);
 puts("Costume engine/access/step checks passed");
}
