#pragma once
// Compact animation state. Colors are generated in each browser, not streamed as frames.
inline String renderState(const costume::Engine& e,uint32_t boot){
 using namespace costume;
 String j="{\"clock\":"+String(millis())+",\"boot\":"+String(boot)+",\"effectAt\":"+String(e.effectStarted())+",\"cap\":"+String(BRIGHTNESS_CAP)+",\"chargeMs\":"+String(CHARGE_MS)+",\"collisionMs\":"+String(COLLISION_MS)+",\"fadeMs\":"+String(FADE_MS)+",\"speed\":35,\"junction\":[";
 for(unsigned i=0;i<4;i++){if(i)j+=',';j+=String(JUNCTION[i]);}
 j+="],\"regions\":[";
 for(unsigned a=0;a<2;a++){if(a)j+=',';j+='[';for(unsigned i=0;i<12;i++){if(i)j+=',';j+="["+String(e.regionStart(a,i))+","+String(e.regionEnd(a,i))+"]";}j+=']';}
 j+="],\"touchAt\":[";
 for(unsigned a=0;a<2;a++){if(a)j+=',';j+='[';for(unsigned i=0;i<12;i++){if(i)j+=',';j+=String(e.touchStarted(a,i));}j+=']';}
 j+="],\"touchValid\":["+String(e.touchIsValid(0)?"true":"false")+","+(e.touchIsValid(1)?"true":"false")+"],\"heart\":{\"flashAt\":"+String(e.heart.flashAt)+",\"flashed\":"+(e.heart.flashes?"true":"false")+",\"nextBeat\":"+String(e.heart.nextBeat)+",\"period\":"+String(e.heart.period)+"},\"pulses\":[";
 bool comma=false;
 for(unsigned i=0;i<MAX_PULSES;i++){const auto& p=e.pulse(i);if(!p.used)continue;if(comma)j+=',';comma=true;
  j+="{\"id\":"+String(i)+",\"limb\":"+String(p.limb)+",\"at\":"+String(p.at)+",\"start\":"+String(p.start,3)+",\"charge\":"+String(p.charge,4)+",\"radius\":"+String(pulseRadius(p),4)+",\"collided\":"+String(p.collided)+"}";
 }
 j+="],\"flares\":[";comma=false;
 for(unsigned i=0;i<MAX_FLARES;i++){const auto& f=e.flareState(i);if(!f.used)continue;if(comma)j+=',';comma=true;j+="{\"at\":"+String(f.at)+",\"center\":"+String(f.center,3)+"}";}
 j+="]}";return j;
}
