#pragma once
#include <Wire.h>
#include "Engine.h"
#include "BeatFilter.h"
#include "RoughTouch.h"
namespace costume {
class Sensors {
  Engine& engine; BeatFilter filter{20000}; ThighStepDetector step[2];
  RoughTouchDetector roughTouch[2];
  TwoWire secondWire{1};
  bool controllerReady[2]={}; int muxAddress_[2]={-1,-1};
  uint8_t muxCandidate[2]={0x70,0x70},motionCandidate[2]={};
  bool muxWaiting[2]={},motionWaiting[2]={};
  uint32_t muxRetry[2]={},motionRetry[2]={},touchDiagnosticAt[2]={},heartDiagnosticAt=0;
  Branch branches[5]={TOUCH[0],TOUCH[1],MOTION[0],MOTION[1],HEART_BUS};
  const char* problems[5]={"starting","starting","discovering","discovering","starting"};
  unsigned cursor=0, init[5]={}; uint32_t last=0,retry[5]={}; bool bus=false;
  TwoWire& wire(unsigned index){return index?secondWire:Wire;}
  static bool supportedIMU(uint8_t id){return (id&0x7E)==0x68||id==0x70;}
  bool muxWrite(unsigned index,int addr,uint8_t mask){auto& w=wire(index);w.beginTransmission(addr);w.write(mask);return w.endTransmission()==0;}
  bool discoverMux(unsigned index,uint32_t now){
    if(!controllerReady[index])return false;
    if(muxWaiting[index]&&uint32_t(now-muxRetry[index])<3000)return false;
    muxWaiting[index]=false;
    int address=MUX_ADDRESS[index]>=0?MUX_ADDRESS[index]:muxCandidate[index];
    auto& w=wire(index);
    if(muxWrite(index,address,0)&&w.requestFrom(uint8_t(address),size_t(1))==1&&w.read()==0){
      muxAddress_[index]=address;
      engine.log.record(now,"MUX_READY bus=%u SDA=%d SCL=%d address=0x%02X",index+1,SDA_PINS[index],SCL_PINS[index],address);
      return true;
    }
    if(MUX_ADDRESS[index]>=0||++muxCandidate[index]>0x77){
      muxCandidate[index]=0x70;muxWaiting[index]=true;muxRetry[index]=now;
      engine.log.record(now,"MUX_MISSING bus=%u SDA=%d SCL=%d retry_ms=3000",index+1,SDA_PINS[index],SCL_PINS[index]);
    }return false;
  }
  bool select(Branch b){
    // Independent controllers isolate the muxes even if both use 0x70.
    // Within a mux, disconnect all branches before selecting exactly one.
    if(b.mux<0||b.mux>1||b.channel<0||b.channel>7||muxAddress_[b.mux]<0)return false;
    auto& w=wire(b.mux);int address=muxAddress_[b.mux];uint8_t mask=1u<<b.channel;
    if(!muxWrite(b.mux,address,0)||w.requestFrom(uint8_t(address),size_t(1))!=1||w.read()!=0)return false;
    if(!muxWrite(b.mux,address,mask))return false;
    return w.requestFrom(uint8_t(address),size_t(1))==1&&w.read()==mask;
  }
  bool read(Branch b,uint8_t reg,uint8_t* data,size_t count){
    auto& w=wire(b.mux);w.beginTransmission(b.address);w.write(reg);unsigned status=w.endTransmission(false);
    if(status){engine.log.record(millis(),"I2C_READ_POINTER bus=%d channel=%d address=0x%02X reg=0x%02X error=%u",b.mux+1,b.channel,b.address,reg,status);return false;}
    size_t got=w.requestFrom(uint8_t(b.address),count);
    if(got!=count){engine.log.record(millis(),"I2C_READ_SHORT bus=%d channel=%d address=0x%02X reg=0x%02X got=%u expected=%u",b.mux+1,b.channel,b.address,reg,unsigned(got),unsigned(count));return false;}
    for(size_t i=0;i<count;i++)data[i]=w.read();return true;
  }
  bool write(Branch b,uint8_t reg,uint8_t val){
    auto& w=wire(b.mux);w.beginTransmission(b.address);w.write(reg);w.write(val);unsigned status=w.endTransmission();
    if(status)engine.log.record(millis(),"I2C_WRITE bus=%d channel=%d address=0x%02X reg=0x%02X value=0x%02X error=%u",b.mux+1,b.channel,b.address,reg,val,status);
    return status==0;
  }
  void discoverMotion(unsigned slot,uint32_t now){
    unsigned i=slot+2;
    if(motionWaiting[slot]&&uint32_t(now-motionRetry[slot])<3000)return;
    motionWaiting[slot]=false;
    bool fixed=MOTION[slot].channel>=0;
    Branch b={MOTION[slot].mux,fixed?MOTION[slot].channel:int(motionCandidate[slot]/2),int(0x68+(motionCandidate[slot]%2))};
    const char* result="mux_select_failed";uint8_t id=0;
    if(select(b)){
      auto& w=wire(b.mux);w.beginTransmission(b.address);unsigned ack=w.endTransmission();
      if(ack){
        engine.log.record(now,"IMU_PROBE sensor=%u bus=%d channel=%d address=0x%02X ack=%u result=no_ack",i,b.mux+1,b.channel,b.address,ack);
        result="no_ack";
      }else if(!read(b,0x75,&id,1))result="identity_read_failed";
      else{
        result=id==0x70?"MPU6500":supportedIMU(id)?"MPU6050":"unsupported_identity";
        engine.log.record(now,"IMU_ID sensor=%u bus=%d channel=%d address=0x%02X WHO_AM_I=0x%02X result=%s",i,b.mux+1,b.channel,b.address,id,result);
        if(supportedIMU(id)){branches[i]=b;imuIdentity[slot]=id;problems[i]="initializing";return;}
      }
    }
    if(result[0]!='n')engine.log.record(now,"IMU_PROBE sensor=%u channel=%d address=0x%02X result=%s",i,b.channel,b.address,result);
    problems[i]=result;
    if(++motionCandidate[slot]>=(fixed?2:16)){
      motionCandidate[slot]=0;motionWaiting[slot]=true;motionRetry[slot]=now;
      engine.log.record(now,"IMU_MISSING sensor=%u bus=%d channel=%d addresses=0x68,0x69 retry_ms=3000",i,b.mux+1,MOTION[slot].channel);
    }
  }
  bool touchDiagnostics(unsigned i,Branch b,uint32_t now){
    if(!RAW_DIAGNOSTIC_MS||uint32_t(now-touchDiagnosticAt[i])<RAW_DIAGNOSTIC_MS)return true;
    touchDiagnosticAt[i]=now;
    uint8_t baseline[12],config[3],thresholds[2];
    if(!read(b,0x1E,baseline,12)||!read(b,0x5C,config,3)||!read(b,0x41,thresholds,2))return false;
    engine.log.record(now,"TOUCH_STATUS sensor=%u ch=%d mask=0x%03X hw=0x%03X ECR=0x%02X cfg=0x%02X/0x%02X hw_th=%u/%u OOR=0x%03X",i,b.channel,unsigned(engine.touchMask(i)),unsigned(touchHardwareMask[i]),config[2],config[0],config[1],thresholds[0],thresholds[1],unsigned(touchOutOfRange[i]));
    for(unsigned e=0;e<12;e++){
      touchBaseline[i][e]=uint16_t(baseline[e])<<2;
    }
    for(unsigned e=0;e<12;e+=4)engine.log.record(now,"TOUCH_RAW sensor=%u ch=%d f/b/d %u=%u/%u/%d %u=%u/%u/%d %u=%u/%u/%d %u=%u/%u/%d",i,b.channel,
      e,touchFiltered[i][e],touchBaseline[i][e],int(touchBaseline[i][e])-touchFiltered[i][e],
      e+1,touchFiltered[i][e+1],touchBaseline[i][e+1],int(touchBaseline[i][e+1])-touchFiltered[i][e+1],
      e+2,touchFiltered[i][e+2],touchBaseline[i][e+2],int(touchBaseline[i][e+2])-touchFiltered[i][e+2],
      e+3,touchFiltered[i][e+3],touchBaseline[i][e+3],int(touchBaseline[i][e+3])-touchFiltered[i][e+3]);
    if(USE_ROUGH_TOUCH)engine.log.record(now,"TOUCH_STABILITY sensor=%u mask=0x%03X pending=0x%03X median_e0=%u press_ms=%lu release_ms=%lu",i,unsigned(engine.touchMask(i)),unsigned(roughTouch[i].pendingMask()),unsigned(roughTouch[i].filtered(0)),(unsigned long)ROUGH_TOUCH_PRESS_MS,(unsigned long)ROUGH_TOUCH_RELEASE_MS);
    return (config[2]&0x3F)==12;
  }
  void fail(unsigned i,uint32_t now,const char* reason){
    Branch b=branches[i];problems[i]=reason;
    engine.faults[i]++;engine.sensorPresent[i]=false;init[i]=0;retry[i]=now;
    engine.log.record(now,"SENSOR_FAULT sensor=%u mux=%d channel=%d address=0x%02X reason=%s faults=%lu",i,b.mux+1,b.channel,b.address,reason,(unsigned long)engine.faults[i]);
    if(i<2){roughTouch[i].reset();engine.touch(i,0,false,now);}
    if(i==4){filter.reset();engine.heart.disconnect();fingerPresent=false;heartSignal=0;}
  }
  bool initialize(unsigned i,Branch b){
    unsigned n=init[i];uint8_t v=0;
    if(i<2){
      // MPR121 stop, baseline/filter defaults, thresholds, 12-electrode run.
      if(n==0){if(!write(b,0x80,0x63))return false;}
      else if(n==1){if(!read(b,0x5D,&v,1)||v!=0x24){engine.log.record(millis(),"TOUCH_RESET_CHECK sensor=%u expected_CONFIG2=0x24 got=0x%02X",i,v);return false;}}
      else if(n==2){if(!write(b,0x5E,0))return false;}
      else if(n<=26){unsigned electrode=(n-3)/2,offset=(n-3)%2;if(!write(b,0x41+electrode*2+offset,offset?6:12))return false;}
      else {
        const uint8_t regs[]={0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x5B,0x5C,0x5D,0x7B,0x5E};
        const uint8_t vals[]={1,1,0x0E,0,1,5,1,0,0,0,0,0,0x10,0x20,0x0A,0x8C};
        unsigned k=n-27;if(k>=sizeof(regs)){
          if(!read(b,0x5E,&v,1))return false;
          if((v&0x3F)!=12){
            uint8_t status[2]={};bool got=read(b,0,status,2);
            engine.log.record(millis(),"TOUCH_RUN_CHECK sensor=%u channel=%d ECR=0x%02X expected=0x8C status_read=%u status_high=0x%02X",i,b.channel,v,got?1u:0u,status[1]);return false;
          }
          engine.sensorPresent[i]=true;return true;
        }
        if(!write(b,regs[k],vals[k]))return false;
      }
    }else if(i<4){
      if(n==0){if(!read(b,0x75,&v,1) || !supportedIMU(v))return false;imuIdentity[i-2]=v;}
      else if(n==1){if(!write(b,0x6B,1))return false;}
      else if(n==2){if(!write(b,0x1C,0))return false;}
      else if(n==3){if(!write(b,0x1A,3))return false;}
      else if(n==4&&imuIdentity[i-2]==0x70){if(!write(b,0x1D,3))return false;}
      else if(n==4){}
      else if(n==5){if(!write(b,0x1B,0))return false;}
      else {step[i-2].calibrate();engine.sensorPresent[i]=true;return true;}
    }else{
      if(n==0){if(!read(b,0xFF,&v,1)||v!=0x15)return false;}
      else if(n==1){if(!write(b,0x09,0x40))return false;}
      else if(n==2){if(!read(b,0x09,&v,1))return false;if(v&0x40)return false;}
      else {
        const uint8_t regs[]={0x08,0x04,0x05,0x06,0x0A,0x0C,0x0D,0x09};
        const uint8_t vals[]={0x70,0,0,0,0x2B,0x3C,0x3C,3};
        unsigned k=n-3;if(k>=sizeof(regs)){engine.sensorPresent[i]=true;engine.sensorAt[i]=millis();return true;}
        if(!write(b,regs[k],vals[k]))return false;
      }
    }
    init[i]++;return true;
  }
public:
  uint8_t imuIdentity[2]={};
  uint16_t touchFiltered[2][12]={},touchBaseline[2][12]={},touchOutOfRange[2]={},touchHardwareMask[2]={};
  uint32_t latestIR=0,latestRed=0,acceptedBeats=0,rejectedBeats=0;bool fingerPresent=false;float heartSignal=0;
  float motionG[2][3]={},magnitude[2]={},gyroDps[2][3]={},rotationDegrees[2]={};uint32_t samples[5]={},stepAt[2]={},motionDiagnosticAt[2]={};
  explicit Sensors(Engine& e):engine(e){}
  bool validBranch(Branch b)const{return b.mux>=0&&b.mux<2&&b.channel>=0&&b.channel<8&&b.address>=8&&b.address<0x70;}
  bool begin(){
    if(!ENABLE_SENSORS)return false;
    for(unsigned i=0;i<2;i++){
      if(SDA_PINS[i]<0||SCL_PINS[i]<0||SDA_PINS[i]==SCL_PINS[i]||(MUX_ADDRESS[i]!=-1&&(MUX_ADDRESS[i]<0x70||MUX_ADDRESS[i]>0x77)))continue;
      controllerReady[i]=wire(i).begin(SDA_PINS[i],SCL_PINS[i],100000);wire(i).setTimeOut(5);bus|=controllerReady[i];
      engine.log.record(millis(),"I2C_BEGIN bus=%u SDA=%d SCL=%d ready=%u",i+1,SDA_PINS[i],SCL_PINS[i],controllerReady[i]?1u:0u);
    }return bus;
  }
  bool ready() const {return bus;}
  bool busReady(unsigned i)const{return controllerReady[i];}
  int muxAddress(unsigned i)const{return muxAddress_[i];}
  Branch branch(unsigned i)const{return branches[i];}
  const char* problem(unsigned i)const{return problems[i];}
  unsigned calibrationRemaining(unsigned i)const{return step[i].calibrationRemaining();}
  const char* name(unsigned i)const{const char* names[]={"Touch A","Touch B","IMU A","IMU B","Heart"};return names[i];}
  void calibrate(){for(auto& d:step)d.calibrate();engine.log.record(millis(),"IMU_CALIBRATION stand_still samples=50 threshold_A_extra_g=%.2f threshold_B_extra_g=%.2f rotation_deg=6 window_ms=800",double(engine.sensitivity[0]),double(engine.sensitivity[1]));}
  void tick(uint32_t now){
    if(!bus||uint32_t(now-last)<10)return;last=now;
    unsigned i=cursor;cursor=(cursor+1)%5;
    Branch b=branches[i];
    if(b.mux<0||b.mux>1){problems[i]="invalid bus";return;}
    if(!controllerReady[b.mux]){problems[i]="I2C controller unavailable";return;}
    if(muxAddress_[b.mux]<0){problems[i]="mux not found";discoverMux(b.mux,now);return;}
    if(!validBranch(b)){if(i==2||i==3)discoverMotion(i-2,now);else problems[i]="invalid branch";return;}
    if(!engine.sensorPresent[i] && init[i]==0 && engine.faults[i] && uint32_t(now-retry[i])<3000)return;
    if(!select(b)){fail(i,now,"mux_select");return;}
    if(!engine.sensorPresent[i]){
      unsigned phase=init[i];
      if(!initialize(i,b)){engine.log.record(now,"SENSOR_INIT_FAILED sensor=%u channel=%d phase=%u",i,b.channel,phase);fail(i,now,"initialize");}
      else if(engine.sensorPresent[i]){problems[i]="ready";engine.sensorAt[i]=now;engine.log.record(now,"SENSOR_READY sensor=%u mux=%d channel=%d address=0x%02X",i,b.mux+1,b.channel,b.address);}
      return;
    }
    uint8_t data[14];
    if(i<2){
      if(!read(b,0,data,4)){fail(i,now,"touch_read");return;}
      if(data[1]&0x80){fail(i,now,"touch_overcurrent");return;}
      touchOutOfRange[i]=uint16_t(data[2])|(uint16_t(data[3]&15)<<8);
      touchHardwareMask[i]=uint16_t(data[0])|(uint16_t(data[1]&15)<<8);
      uint8_t filtered[24];if(!read(b,0x04,filtered,24)){fail(i,now,"touch_filtered_read");return;}
      for(unsigned e=0;e<12;e++)touchFiltered[i][e]=(uint16_t(filtered[e*2])|(uint16_t(filtered[e*2+1])<<8))&0x3FF;
      uint16_t mask=USE_ROUGH_TOUCH?roughTouch[i].sample(touchFiltered[i],now):touchHardwareMask[i];
      engine.touch(i,mask,true,now);
      if(!touchDiagnostics(i,b,now)){fail(i,now,"touch_diagnostic_read_or_stopped");return;}
    }else if(i<4){
      if(!read(b,0x3B,data,14)){fail(i,now,"motion_read");return;}
      float sum=0;for(unsigned k=0;k<3;k++){float v=int16_t((uint16_t(data[k*2])<<8)|data[k*2+1])/16384.0f;motionG[i-2][k]=v;sum+=v*v;}
      magnitude[i-2]=sqrtf(sum);
      for(unsigned k=0;k<3;k++)gyroDps[i-2][k]=int16_t((uint16_t(data[8+k*2])<<8)|data[9+k*2])/131.0f;
      bool detected=step[i-2].sample(magnitude[i-2],gyroDps[i-2][0],gyroDps[i-2][1],gyroDps[i-2][2],engine.sensitivity[i-2],now);
      rotationDegrees[i-2]=step[i-2].rotationDegrees();
      if(RAW_DIAGNOSTIC_MS&&uint32_t(now-motionDiagnosticAt[i-2])>=RAW_DIAGNOSTIC_MS){
        motionDiagnosticAt[i-2]=now;
        engine.log.record(now,"IMU_RAW imu=%s g=%.3f gyro_dps=%.1f/%.1f/%.1f turn_deg=%.1f need_g=%.2f need_deg=6",i==2?"A":"B",double(magnitude[i-2]),double(gyroDps[i-2][0]),double(gyroDps[i-2][1]),double(gyroDps[i-2][2]),double(rotationDegrees[i-2]),double(engine.sensitivity[i-2]));
      }
      if(detected){
        engine.step(i-2,now);stepAt[i-2]=now;engine.log.record(now,"STEP source=physical imu=%s channel=%d address=0x%02X magnitude_g=%.3f rotation_deg=%.1f",i==2?"A":"B",b.channel,b.address,double(magnitude[i-2]),double(rotationDegrees[i-2]));
      }
    }else{
      if(!read(b,4,data,3)){fail(i,now,"heart_fifo_read");return;}
      unsigned count=(data[0]-data[2])&31;
      if(data[1]&31 || count>8){fail(i,now,"heart_fifo_backlog");return;}
      if(!count){if(uint32_t(now-engine.sensorAt[i])>2000)fail(i,now,"heart_no_samples");return;}
      // Maximum two FIFO samples per visit; retain chronology across each batch.
      for(unsigned n=0;n<count && n<2;n++){
        if(!read(b,7,data,6)){fail(i,now,"heart_sample_read");return;}
        uint32_t ir=((uint32_t(data[3])<<16)|(uint32_t(data[4])<<8)|data[5])&0x3FFFF;
        latestIR=ir;latestRed=((uint32_t(data[0])<<16)|(uint32_t(data[1])<<8)|data[2])&0x3FFFF;
        if(filter.feed(ir)){
          bool accepted=engine.heart.beatAt(now-40*(count-1-n),now);
          if(accepted)acceptedBeats++;else rejectedBeats++;
          engine.log.record(now,"BEAT source=physical accepted=%u period_ms=%lu",accepted?1u:0u,(unsigned long)engine.heart.period);
        }
        if(!filter.finger)engine.heart.disconnect();
        fingerPresent=filter.finger;heartSignal=filter.signal;
      }
      if(RAW_DIAGNOSTIC_MS&&uint32_t(now-heartDiagnosticAt)>=RAW_DIAGNOSTIC_MS){heartDiagnosticAt=now;
        engine.log.record(now,"HEART_RAW red=%lu IR=%lu finger=%u signal=%.1f bpm=%.1f period_ms=%lu accepted=%lu rejected=%lu",(unsigned long)latestRed,(unsigned long)latestIR,fingerPresent?1u:0u,double(heartSignal),engine.heart.period?60000.0/engine.heart.period:0.0,(unsigned long)engine.heart.period,(unsigned long)acceptedBeats,(unsigned long)rejectedBeats);
      }
    }
    engine.sensorAt[i]=now;samples[i]++;
  }
};
}
