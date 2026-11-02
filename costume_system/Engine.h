#pragma once
#include "Config.h"
#include "Diagnostics.h"
#include <math.h>
#include <string.h>
namespace costume {
struct RGB { uint8_t r=0,g=0,b=0; };
inline RGB scaled(RGB c, float x) { if(x<0)x=0; if(x>1)x=1; return {uint8_t(c.r*x),uint8_t(c.g*x),uint8_t(c.b*x)}; }
inline void blend(RGB& a, RGB b) {
  a.r=uint8_t(fminf(BRIGHTNESS_CAP,unsigned(a.r)+b.r));
  a.g=uint8_t(fminf(BRIGHTNESS_CAP,unsigned(a.g)+b.g));
  a.b=uint8_t(fminf(BRIGHTNESS_CAP,unsigned(a.b)+b.b));
}
class HeartClock {
  uint32_t anchor=0, intervals[4]={}, candidate=0;
  unsigned count=0,next=0,candidates=0;
  bool anchored=false, flashed=false;
public:
  uint32_t period=0, nextBeat=0, flashAt=0, lastReal=0, rejected=0, flashes=0;
  bool haveReal=false;
  void disconnect() { anchored=false; candidates=0; }
  void flash(uint32_t now) { flashAt=now; flashed=true; flashes++; }
  bool beat(uint32_t now) { return beatAt(now,now); }
  bool beatAt(uint32_t now,uint32_t displayNow) {
    if(anchored) {
      uint32_t dt=now-anchor;
      if(dt<333) { rejected++; return false; }
      if(dt>1714) { anchor=now; candidates=0; rejected++; return false; }
      if(period && (dt<period*0.65f || dt>period*1.35f)) {
        // Three mutually consistent changed-tempo intervals establish a new rhythm.
        anchor=now; rejected++;
        if(candidate && fabsf(float(dt)-candidate)<candidate*0.15f) candidates++;
        else { candidate=dt; candidates=1; }
        if(candidates<3) return false;
        count=next=0; period=0;
      }
      candidates=0;
      intervals[next]=dt; next=(next+1)%4; if(count<4)count++;
      uint32_t sum=0; for(unsigned i=0;i<count;i++)sum+=intervals[i];
      // Prediction starts after five accepted beats, not the first interval.
      if(count==4)period=sum/4;
    }
    anchor=now; anchored=true; haveReal=true; lastReal=displayNow;
    if(!flashed || uint32_t(displayNow-flashAt)>MATCH_MS)flash(displayNow);
    if(period)nextBeat=now+period;
    return true;
  }
  void tick(uint32_t now) {
    if(period && int32_t(now-nextBeat)>=0) {
      // Coalesce delayed work; never replay a backlog of missed flashes.
      nextBeat=now+period;
      if(!flashed || uint32_t(now-flashAt)>MATCH_MS)flash(now);
    }
  }
  bool live(uint32_t now) const { return haveReal && uint32_t(now-lastReal)<LIVE_MS; }
  uint8_t level(uint32_t now) const {
    if(!flashed)return 0;
    uint32_t age=now-flashAt;
    if(age<140)return uint8_t(160*(1.0f-age/140.0f));
    if(age>=180 && age<320)return uint8_t(95*(1.0f-(age-180)/140.0f));
    return 0;
  }
};
enum Pattern { SOLID, BREATHE, CHASE, PAINT };
struct Manual {
  bool active=false; RGB color={10,50,255}; uint8_t brightness=120;
  Pattern pattern=SOLID; uint32_t speed=1200; bool reverse=false;
};
struct Stroke { RGB color; uint32_t at=0; bool used=false, fade=false; };
struct Pulse { bool used=false; unsigned limb=0; uint32_t at=0; float start=0, charge=0; uint32_t collided=0; };
inline float pulseRadius(const Pulse& p){return (5+7*p.charge)*(p.limb>=2?0.5f:1.0f);}
struct Flare { bool used=false; uint32_t at=0; float center=0; };
enum OneShot { NO_SHOT, LIGHTNING, ENERGY_WAVE, HEART_RESTART };
static_assert(MAX_PULSES<=32,"Collision tracking uses a 32-bit pulse mask");
class Engine {
  Pulse pulses[MAX_PULSES];
  Flare flares[MAX_FLARES];
  uint32_t heldAt[2][12]={};
  uint16_t held[2]={}; bool touchValid[2]={};
  OneShot shot=NO_SHOT; uint32_t shotAt=0;
  float pulsePosition(const Pulse& p,uint32_t now) const { return p.start-float(uint32_t(now-p.at))*35.0f/1000.0f; }
  void flare(float center,uint32_t now) {
    collisions++;
    for(auto& f:flares)if(!f.used){f={true,now,center};return;}
    dropped++;
  }
public:
  bool idleGlow=false;
  Diagnostics log;
  HeartClock heart;
  Manual manual[SECTIONS];
  Stroke paint[SECTIONS][MAX_PIXELS];
  RGB pixels[SECTIONS][MAX_PIXELS];
  uint32_t dropped=0, collisions=0, steps[2]={}, touchAt[2]={};
  bool sensorPresent[5]={}; uint32_t sensorAt[5]={}, faults[5]={};
  float sensitivity[2]={DEFAULT_STEP_THRESHOLD_G,DEFAULT_STEP_THRESHOLD_G};
  const Pulse& pulse(unsigned i)const{return pulses[i];}
  const Flare& flareState(unsigned i)const{return flares[i];}
  uint32_t touchStarted(unsigned arm,unsigned electrode)const{return heldAt[arm][electrode];}
  bool touchIsValid(unsigned arm)const{return touchValid[arm];}
  uint32_t effectStarted()const{return shotAt;}
  uint16_t touchMask(unsigned arm) const { return held[arm]; }
  unsigned regionStart(unsigned arm,unsigned e) const {
    return ELECTRODES_CONFIRMED ? unsigned(ELECTRODES[arm][e].first) : e*LENGTHS[arm]/12;
  }
  unsigned regionEnd(unsigned arm,unsigned e) const {
    return ELECTRODES_CONFIRMED ? unsigned(ELECTRODES[arm][e].last) : (e+1)*LENGTHS[arm]/12;
  }
  float charge(unsigned arm,unsigned electrode,uint32_t now) const {
    return fminf(1.0f,float(uint32_t(now-heldAt[arm][electrode]))/CHARGE_MS);
  }
  void launch(unsigned limb,float pos,uint32_t now,float energy=0) {
    if(limb>=4 || manual[limb].active)return;
    for(unsigned i=0;i<MAX_PULSES;i++)if(!pulses[i].used){
      // Reused pool slots must not inherit old pair-collision identities.
      for(auto& other:pulses)other.collided&=~(uint32_t(1)<<i);
      pulses[i]={true,limb,now,pos,fmaxf(0,fminf(1,energy)),0};return;
    }
    dropped++; // Drop newest event when the fixed pool is full.
  }
  void touch(unsigned arm,uint16_t mask,bool valid,uint32_t now,bool simulated=false) {
    if(arm>1)return;
    const char* source=simulated?"simulated":"physical";
    if(!valid) {
      if(touchValid[arm])log.record(now,"TOUCH_LOST source=%s arm=%s mask=0x%03X no_release_trigger",source,arm?"right":"left",unsigned(held[arm]));
      held[arm]=0; touchValid[arm]=false; return;
    }
    if(!touchValid[arm])log.record(now,"TOUCH_BASELINE source=%s arm=%s mask=0x%03X",source,arm?"right":"left",unsigned(mask&4095));
    if(touchValid[arm]) {
      uint16_t released=held[arm]&~mask;
      for(unsigned e=0;e<12;e++)if(released&(1u<<e)){
        log.record(now,"TOUCH_UP source=%s arm=%s electrode=%u held_ms=%lu mask=0x%03X override=%u",source,arm?"right":"left",e,(unsigned long)uint32_t(now-heldAt[arm][e]),unsigned(mask&4095),manual[arm].active?1u:0u);
        if(regionEnd(arm,e)>regionStart(arm,e))launch(arm,regionStart(arm,e),now,charge(arm,e,now));
        else log.record(now,"TOUCH_UNMAPPED arm=%s electrode=%u log_only=1",arm?"right":"left",e);
      }
    }
    for(unsigned e=0;e<12;e++)if((mask&(1u<<e)) && (!touchValid[arm] || !(held[arm]&(1u<<e)))){
      heldAt[arm][e]=now;
      log.record(now,"TOUCH_DOWN source=%s arm=%s electrode=%u mask=0x%03X leds=[%u,%u) override=%u",source,arm?"right":"left",e,unsigned(mask&4095),regionStart(arm,e),regionEnd(arm,e),manual[arm].active?1u:0u);
    }
    held[arm]=mask&4095; touchValid[arm]=true; touchAt[arm]=now;
  }
  void step(unsigned leg,uint32_t now) { if(leg<2){steps[leg]++;launch(leg+2,LENGTHS[leg+2]-1,now,0.35f);} }
  void oneShot(OneShot effect,uint32_t now) { shot=effect;shotAt=now; }
  OneShot activeShot() const { return shot; }
  void clear(unsigned s) { if(s<SECTIONS)for(auto& p:paint[s])p.used=false; }
  void sensors(unsigned s) {
    if(s>=SECTIONS)return;
    manual[s].active=false; clear(s);
    for(auto& p:pulses)if(p.used && (s==SPINE || p.limb==s))p.used=false;
    if(s==SPINE)for(auto& f:flares)f.used=false;
  }
  void brush(unsigned s,unsigned index,unsigned width,RGB color,bool fade,uint32_t now) {
    if(s>=SECTIONS || index>=LENGTHS[s])return;
    manual[s].active=true; manual[s].pattern=PAINT;
    int first=int(index)-int(width/2);
    for(unsigned i=0;i<width;i++){int k=first+int(i);if(k>=0 && k<int(LENGTHS[s]))paint[s][k]={color,now,true,fade};}
  }
  void render(uint32_t now) {
    heart.tick(now); memset(pixels,0,sizeof(pixels));
    const RGB blue={8,35,160};
    if(idleGlow)for(unsigned s=0;s<SPINE;s++)for(unsigned i=0;i<LENGTHS[s];i++)pixels[s][i]=scaled(blue,0.06f);
    for(unsigned i=0;i<LENGTHS[HEART];i++)pixels[HEART][i]={heart.level(now),0,0};
    for(unsigned arm=0;arm<2;arm++)if(touchValid[arm])for(unsigned e=0;e<12;e++)if(held[arm]&(1u<<e))
      for(unsigned i=regionStart(arm,e);i<regionEnd(arm,e) && i<LENGTHS[arm];i++)blend(pixels[arm][i],scaled(blue,0.2f+0.8f*charge(arm,e,now)));
    for(auto& f:flares)if(f.used && uint32_t(now-f.at)>=COLLISION_MS)f.used=false;
    for(auto& p:pulses)if(p.used){
      float distance=pulsePosition(p,now);
      float spineDistance=JUNCTION[p.limb]-distance;
      float radius=pulseRadius(p);
      if(distance<0 && spineDistance>LENGTHS[SPINE]+radius){p.used=false;continue;}
      for(unsigned s=0;s<SECTIONS;s++)if(s==p.limb || s==SPINE){
        if(s==SPINE && distance>=0)continue;
        float center=s==SPINE ? spineDistance : distance;
        for(unsigned i=0;i<LENGTHS[s];i++)blend(pixels[s][i],scaled(blue,(0.5f+0.5f*p.charge)*(1-fabsf(float(i)-center)/radius)));
      }
    }
    if(!manual[SPINE].active)for(unsigned i=0;i<MAX_PULSES;i++)if(pulses[i].used){
      auto& a=pulses[i];float da=pulsePosition(a,now);
      if(da>=0)continue;
      float ca=JUNCTION[a.limb]-da;
      if(ca<0||ca>=LENGTHS[SPINE])continue;
      for(unsigned j=i+1;j<MAX_PULSES;j++)if(pulses[j].used){
        auto& b=pulses[j];if(a.limb==b.limb || (a.collided&(uint32_t(1)<<j)))continue;
        float db=pulsePosition(b,now),cb=JUNCTION[b.limb]-db;
        if(db>=0||cb<0||cb>=LENGTHS[SPINE])continue;
        if(fabsf(ca-cb)<=4){a.collided|=uint32_t(1)<<j;b.collided|=uint32_t(1)<<i;flare((ca+cb)*0.5f,now);}
      }
    }
    for(auto& f:flares)if(f.used)for(unsigned i=0;i<LENGTHS[SPINE];i++) {
      float mix=(1-float(uint32_t(now-f.at))/COLLISION_MS)*(1-fabsf(float(i)-f.center)/6);
      if(mix<=0)continue;
      RGB& pixel=pixels[SPINE][i];
      // Tint the collision yellow, reducing blue rather than adding white.
      pixel={uint8_t(pixel.r*(1-mix)+160*mix),uint8_t(pixel.g*(1-mix)+135*mix),uint8_t(pixel.b*(1-mix)+5*mix)};
    }
    for(unsigned s=0;s<SECTIONS;s++)if(manual[s].active){
      auto& m=manual[s];
      for(unsigned i=0;i<LENGTHS[s];i++){
        RGB c={};
        if(m.pattern==PAINT){auto& p=paint[s][i];if(p.used){
          uint32_t age=now-p.at;if(p.fade && age>=FADE_MS)p.used=false;
          else c=scaled(p.color,p.fade ? 1-float(age)/FADE_MS : 1);
        }}else{
          float factor=1;
          if(m.pattern==BREATHE)factor=0.5f+0.5f*sinf(float(now%m.speed)/m.speed*6.283185f);
          if(m.pattern==CHASE){unsigned pos=(uint64_t(now)*LENGTHS[s]/m.speed)%LENGTHS[s];if(m.reverse)pos=LENGTHS[s]-1-pos;factor=fabsf(float(i)-pos)<3 ? 1:0;}
          c=scaled(m.color,factor);
        }
        pixels[s][i]=scaled(c,float(m.brightness)/255);
        pixels[s][i].r=uint8_t(fminf(BRIGHTNESS_CAP,pixels[s][i].r));
        pixels[s][i].g=uint8_t(fminf(BRIGHTNESS_CAP,pixels[s][i].g));
        pixels[s][i].b=uint8_t(fminf(BRIGHTNESS_CAP,pixels[s][i].b));
      }
    }
    // Temporary global effects render last, without changing section ownership,
    // paint, or heartbeat timing. A newer one-shot replaces the current one.
    if(shot!=NO_SHOT){
      uint32_t age=now-shotAt;
      const uint32_t duration=shot==LIGHTNING?700:(shot==ENERGY_WAVE?2200:2400);
      if(age>=duration){shot=NO_SHOT;return;}
      for(unsigned s=0;s<SECTIONS;s++)for(unsigned i=0;i<LENGTHS[s];i++){
        if(shot==LIGHTNING){
          float flash=age<110?1-float(age)/110:(age>=260&&age<400?0.8f*(1-float(age-260)/140):0);
          blend(pixels[s][i],scaled({160,160,160},flash));
        }else if(shot==ENERGY_WAVE){
          float route=s<4?float(i)/LENGTHS[s]*0.55f:(s==SPINE?0.55f+float(i)/LENGTHS[s]*0.45f:0.82f);
          blend(pixels[s][i],scaled({80,160,160},1-fabsf(route-float(age)/duration*1.25f)/0.12f));
        }else{
          // Blackout, two heart double-pulses, then a blue fade-in.
          RGB c={};
          if(s==HEART && age>=900){
            uint32_t beatAge=(age-900)%650;
            float envelope=age>=2200?0:(beatAge<140?1-float(beatAge)/140:(beatAge>=180&&beatAge<320?0.55f*(1-float(beatAge-180)/140):0));
            c={uint8_t(160*envelope),0,0};
          }else if(s!=HEART && age>=1900)c=scaled(blue,0.1f*float(age-1900)/500);
          pixels[s][i]=c;
        }
      }
    }
  }
};
// Gravity-independent magnitude high-pass with recovery and a retrigger lockout.
class StepDetector {
  float baseline=1; unsigned warm=0; bool armed=false,seen=false; uint32_t last=0;
public:
  void calibrate(){baseline=1;warm=0;armed=false;seen=false;}
  unsigned calibrationRemaining()const{return 50-warm;}
  bool sample(float magnitude,float threshold,uint32_t now){
    baseline+=(magnitude-baseline)*0.04f;
    if(warm<50){warm++;return false;}
    float impact=magnitude-baseline;
    if(impact<threshold*0.35f)armed=true;
    if(armed && impact>threshold && (!seen || uint32_t(now-last)>300)){
      armed=false;seen=true;last=now;return true;
    }return false;
  }
};
// Pair an acceleration event with recent bias-corrected gyro rotation.
class ThighStepDetector {
  StepDetector acceleration;
  struct Turn { uint32_t at=0; float axis[3]={}; bool used=false; } turns[32];
  float bias[3]={},angle_=0;unsigned warm=0,next=0;
  uint32_t previous=0,impactAt=0,lastStep=0;
  bool sampled=false,pending=false,stepped=false;
  void clearTurns(){for(auto& t:turns)t.used=false;next=0;}
public:
  void calibrate(){acceleration.calibrate();for(auto& b:bias)b=0;warm=0;clearTurns();sampled=pending=stepped=false;angle_=0;}
  unsigned calibrationRemaining()const{return 50-warm;}
  float rotationDegrees()const{return angle_;}
  bool sample(float magnitude,float gx,float gy,float gz,float threshold,uint32_t now){
    float rates[3]={gx,gy,gz};
    bool impact=acceleration.sample(magnitude,threshold,now);
    uint32_t dt=sampled?uint32_t(now-previous):0;previous=now;sampled=true;
    if(warm<50){++warm;for(unsigned k=0;k<3;k++)bias[k]+=(rates[k]-bias[k])/warm;return false;}
    if(dt>200){clearTurns();pending=false;angle_=0;return false;}
    Turn& t=turns[next];next=(next+1)%32;t.at=now;t.used=true;
    for(unsigned k=0;k<3;k++)t.axis[k]=(rates[k]-bias[k])*float(dt)/1000.0f;
    float sum[3]={};
    for(auto& turn:turns)if(turn.used){
      if(uint32_t(now-turn.at)>=STEP_MOTION_WINDOW_MS){turn.used=false;continue;}
      for(unsigned k=0;k<3;k++)sum[k]+=turn.axis[k];
    }
    angle_=sqrtf(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);
    if(pending&&uint32_t(now-impactAt)>STEP_MOTION_WINDOW_MS)pending=false;
    if(impact){pending=true;impactAt=now;}
    if(pending&&angle_>=STEP_ROTATION_DEGREES&&(!stepped||uint32_t(now-lastStep)>350)){
      pending=false;stepped=true;lastStep=now;clearTurns();return true;
    }
    return false;
  }
};

}
