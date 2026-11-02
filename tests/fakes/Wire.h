#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
// Host-only bus model. No Arduino headers or physical hardware are used.
inline uint32_t fakeNow=0;
inline uint32_t millis(){return fakeNow;}
namespace fake {
struct Device {
 bool present=false;uint8_t id=0x68;uint16_t touches=0;int16_t x=0,y=0,z=16384,gx=0,gy=0,gz=0;
 uint8_t values[128]={};uint16_t filtered[12]={};
 uint32_t ir=40000,red=30000;
 Device(){reset();}
 void reset(){memset(values,0,sizeof(values));values[0x5D]=0x24;for(unsigned e=0;e<12;e++){values[0x1E + e]=100;filtered[e]=400;}}
};
inline Device devices[2][8][128];
inline bool muxPresent[2]={true,true};
inline int sda[2]={-1,-1},scl[2]={-1,-1};
inline uint8_t mask[2]={},reg[2][128]={};
}
class TwoWire {
 unsigned index;uint8_t target=0,sent[8]={},received[32]={};size_t sentCount=0,receivedCount=0,next=0;
 int channel()const {for(int i=0;i<8;i++)if(fake::mask[index]==(1u<<i))return i;return -1;}
 bool responds()const {
  if(target==0x70)return fake::muxPresent[index];
  int c=channel();return c>=0&&fake::devices[index][c][target].present;
 }
public:
 explicit TwoWire(unsigned i=0):index(i){}
 bool begin(int sda,int scl,unsigned){fake::sda[index]=sda;fake::scl[index]=scl;return true;}
 void setTimeOut(unsigned){}
 void beginTransmission(int address){assert(address>=0&&address<128);target=address;sentCount=0;}
 void write(uint8_t byte){assert(sentCount<8);sent[sentCount++]=byte;}
 unsigned endTransmission(bool=true){
  if(!responds())return 2;
  if(target==0x70&&sentCount){uint8_t m=sent[0];assert(m==0||(m&(m-1))==0);fake::mask[index]=m;}
  else if(sentCount){
   fake::reg[index][target]=sent[0];
   if(sentCount>=2){auto& d=fake::devices[index][channel()][target];if(sent[0]==0x80&&sent[1]==0x63)d.reset();else d.values[sent[0]]=sent[1];}
  }return 0;
 }
 size_t requestFrom(uint8_t address,size_t count){
  assert(count<=32);target=address;receivedCount=0;memset(received,0,sizeof(received));next=0;if(!responds())return 0;
  if(target==0x70){received[0]=fake::mask[index];receivedCount=1;return 1;}
  auto& d=fake::devices[index][channel()][target];uint8_t r=fake::reg[index][target];receivedCount=count;
  for(unsigned k=0;k<count;k++)received[k]=d.values[(r+k)%128];
  if(target==0x57){
   if(r==0xFF)received[0]=0x15;
   else if(r==0x09)received[0]=d.values[0x09]&~0x40;
   else if(r==0x04&&count==3){received[0]=1;received[1]=received[2]=0;}
   else if(r==0x07&&count==6){received[0]=d.red>>16;received[1]=d.red>>8;received[2]=d.red;received[3]=d.ir>>16;received[4]=d.ir>>8;received[5]=d.ir;}
  }
  else if(r==0x75)received[0]=d.id;
  else if(r==0&&(count==2||count==4)){received[0]=uint8_t(d.touches);received[1]=d.touches>>8;}
  else if(r==0x04&&count==24)for(unsigned e=0;e<12;e++){received[e*2]=uint8_t(d.filtered[e]);received[e*2+1]=d.filtered[e]>>8;}
  else if(r==0x3B&&count==14){int16_t axes[]={d.x,d.y,d.z,0,d.gx,d.gy,d.gz};for(unsigned k=0;k<7;k++){received[k*2]=uint16_t(axes[k])>>8;received[k*2+1]=uint8_t(axes[k]);}}
  return count;
 }
 int read(){return next<receivedCount?received[next++]:-1;}
};
inline TwoWire Wire{0};
