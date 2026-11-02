#include "../costume_system/Sensors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace costume;
static void run(Sensors& sensors,unsigned duration){uint32_t end=fakeNow+duration;while(fakeNow<end){fakeNow+=10;sensors.tick(fakeNow);}}
int main(){
 fake::devices[0][6][0x5A].present=true;fake::devices[0][7][0x5A].present=true;
 fake::devices[1][2][0x68].present=true;fake::devices[1][2][0x68].id=0x70;
 fake::devices[1][7][0x69].present=true;
 // A device ACK alone must not identify an IMU; reject a different WHO_AM_I.
 fake::devices[1][7][0x68].present=true;fake::devices[1][7][0x68].id=0x42;
 Engine engine;Sensors sensors(engine);assert(sensors.begin());
 assert(fake::sda[0]==21&&fake::scl[0]==22&&fake::sda[1]==26&&fake::scl[1]==25);
 run(sensors,6000);
 assert(sensors.muxAddress(0)==0x70&&sensors.muxAddress(1)==0x70);
 assert(engine.sensorPresent[0]&&engine.sensorPresent[1]&&engine.sensorPresent[2]&&engine.sensorPresent[3]);
 assert(!engine.sensorPresent[4]); // Missing heart must not block touch or IMUs.
 assert(sensors.branch(2).channel==2&&sensors.branch(3).channel==7&&sensors.branch(3).address==0x69);
 assert(sensors.imuIdentity[0]==0x70&&fake::devices[1][2][0x68].values[0x1D]==3);
 fake::devices[0][6][0x5A].filtered[0]=3;fake::devices[0][6][0x5A].filtered[2]=4;
 run(sensors,300);assert(engine.touchMask(0)==5&&engine.touchMask(1)==0&&sensors.touchHardwareMask[0]==0);
 fake::devices[0][6][0x5A].filtered[0]=8;fake::devices[0][6][0x5A].filtered[2]=8;
 run(sensors,100);assert(engine.touchMask(0)==5);run(sensors,500);assert(engine.touchMask(0)==0);
 assert(sensors.calibrationRemaining(0)==0&&sensors.calibrationRemaining(1)==0);
 assert(engine.sensitivity[0]==0.15f&&engine.sensitivity[1]==0.15f&&engine.steps[0]==0&&engine.steps[1]==0);
 fake::devices[1][2][0x68].x=0;fake::devices[1][2][0x68].z=24576;
 run(sensors,60);assert(engine.steps[0]==0); // Acceleration alone is insufficient.
 fake::devices[1][2][0x68].gy=13100;run(sensors,200);assert(engine.steps[0]==1&&engine.steps[1]==0);
 assert(sensors.motionG[0][2]>1.4f&&sensors.rotationDegrees[0]>=0);fake::devices[1][2][0x68].gy=0;fake::devices[1][2][0x68].x=0;fake::devices[1][2][0x68].z=16384;run(sensors,400);
 fake::devices[1][2][0x68].gy=13100;run(sensors,200);fake::devices[1][2][0x68].z=24576;run(sensors,60);assert(engine.steps[0]==2);fake::devices[1][2][0x68].gy=0;
 fake::devices[0][6][0x5A].filtered[2]=380;run(sensors,1100);
 assert(sensors.touchFiltered[0][2]==380&&sensors.touchBaseline[0][2]==400);
 assert(fake::devices[0][6][0x5A].values[0x5E]==0x8C&&fake::devices[0][6][0x5A].values[0x41]==12);
 bool rawLog=false;for(unsigned i=0;i<engine.log.size();i++)if(strstr(engine.log.at(i).message,"TOUCH_RAW")&&strstr(engine.log.at(i).message,"2=380/400/20"))rawLog=true;assert(rawLog);
 fake::devices[0][6][0x5A].present=false;run(sensors,100);assert(!engine.sensorPresent[0]&&engine.sensorPresent[1]&&engine.sensorPresent[2]);
 fake::devices[0][6][0x5A].present=true;run(sensors,5500);assert(engine.sensorPresent[0]);
 sensors.calibrate();assert(sensors.calibrationRemaining(0)==50);
 // A missing mux on the second bus cannot stop touch reads on the first bus.
 fake::muxPresent[1]=false;Engine partial;Sensors partialSensors(partial);assert(partialSensors.begin());run(partialSensors,5000);
 assert(partial.sensorPresent[0]&&partial.sensorPresent[1]&&!partial.sensorPresent[2]);
 // Probe diagnostics distinguish an absent address from a responding wrong chip.
 fake::muxPresent[1]=true;fake::devices[1][2][0x68].present=false;fake::devices[1][2][0x69].present=false;
 fake::devices[1][7][0x69].present=false;Engine missing;Sensors missingSensors(missing);assert(missingSensors.begin());run(missingSensors,1000);
 bool noAck=false,badId=false;for(unsigned i=0;i<missing.log.size();i++){
  const char* m=missing.log.at(i).message;if(strstr(m,"result=no_ack"))noAck=true;if(strstr(m,"WHO_AM_I=0x42 result=unsupported_identity"))badId=true;
 }assert(noAck&&badId&&!missing.sensorPresent[2]&&!missing.sensorPresent[3]);
 fake::devices[0][2][0x57].present=true;Engine heart;Sensors heartSensors(heart);assert(heartSensors.begin());run(heartSensors,5000);
 assert(heart.sensorPresent[4]&&heartSensors.latestIR==40000&&heartSensors.latestRed==30000&&heartSensors.fingerPresent);
 bool heartRaw=false;for(unsigned i=0;i<heart.log.size();i++)if(strstr(heart.log.at(i).message,"HEART_RAW red=30000 IR=40000 finger=1"))heartRaw=true;assert(heartRaw);
 fake::devices[0][2][0x57].present=false;run(heartSensors,100);assert(!heart.sensorPresent[4]&&!heartSensors.fingerPresent);
 puts("Physical sensor adapter checks passed: two buses, mux/IMU discovery, IDs, touches, steps, calibration, independent faults/reconnect");
}
