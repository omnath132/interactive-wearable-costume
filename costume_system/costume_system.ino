/* Integrated costume controller. Arduino-ESP32 3.x, classic ESP32, Core 1.
   Defaults: physical sensor monitoring, LEDs off, open Wi-Fi AP, locked website. */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include "Engine.h"
#include "Access.h"
#include "Sensors.h"
#include "Outputs.h"
#include "Page.h"
#include "RenderState.h"
using namespace costume;
Engine engine; Access accessControl; Sensors sensors(engine); Outputs outputs;
WebServer server(80); DNSServer dns; Preferences prefs;
String salt, passwordHash, serialLine;
uint32_t renderBootId=0;
uint32_t lastRender=0, loginAt=0, blockedAt=0; unsigned failures=0;
bool blocked=false, attemptedLogin=false;
const bool SIMULATION=!ENABLE_SENSORS;

// Drain serial incrementally so a full USB/UART buffer cannot stall animations.
uint32_t serialLogNext=1,lastDiagnosticSummary=0;
char serialLogLine[200]={};size_t serialLogLength=0,serialLogSent=0;
void serviceDiagnostics(uint32_t now){
 if(uint32_t(now-lastDiagnosticSummary)>=5000){
  lastDiagnosticSummary=now;unsigned present=0,tx=0;
  for(unsigned i=0;i<5;i++)if(engine.sensorPresent[i])present|=1u<<i;
  for(unsigned i=0;i<SECTIONS;i++)if(outputs.ready(i))tx|=1u<<i;
  engine.log.record(now,"STATUS mode=%s sensor_mask=0x%02X touch_left=0x%03X touch_right=0x%03X tx_mask=0x%02X",SIMULATION?"simulated":"physical",present,unsigned(engine.touchMask(0)),unsigned(engine.touchMask(1)),tx);
 }
 if(serialLogSent==serialLogLength){
  serialLogSent=serialLogLength=0;
  if(serialLogNext==engine.log.next())return;
  const LogEntry* entry=engine.log.find(serialLogNext);
  if(!entry && engine.log.size()){
   serialLogNext=engine.log.at(0).sequence;
   serialLogLength=snprintf(serialLogLine,sizeof(serialLogLine),"LOG_GAP serial fell behind; resume_sequence=%lu\n",(unsigned long)serialLogNext);
  }else if(entry){
   serialLogLength=snprintf(serialLogLine,sizeof(serialLogLine),"[%lu +%lu ms] %s\n",(unsigned long)entry->sequence,(unsigned long)entry->ms,entry->message);serialLogNext++;
  }
 }
 int available=Serial.availableForWrite();
 if(available<=0)return;
 size_t bytes=serialLogLength-serialLogSent;if(bytes>96)bytes=96;if(bytes>size_t(available))bytes=available;
 if(bytes)serialLogSent+=Serial.write(reinterpret_cast<const uint8_t*>(serialLogLine+serialLogSent),bytes);
}
String jsonText(const char* text){
 String result="\"";
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;p++){
  if(*p=='"'||*p=='\\'){result+='\\';result+=char(*p);}
  else if(*p<32){char escaped[7];snprintf(escaped,sizeof(escaped),"\\u%04x",unsigned(*p));result+=escaped;}
  else result+=char(*p);
 }return result+'"';
}
void diagnosticSnapshot(){
 String j;j.reserve(12500);j="{\"overwritten\":"+String(engine.log.overwritten)+",\"entries\":[";
 for(unsigned i=0;i<engine.log.size();i++){
  const auto& e=engine.log.at(i);if(i)j+=',';
  j+="{\"sequence\":"+String(e.sequence)+",\"ms\":"+String(e.ms)+",\"message\":"+jsonText(e.message)+"}";
 }j+="]}";reply(200,j);
}

String randomToken(){uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));String s;s.reserve(32);for(auto b:bytes){char h[3];snprintf(h,sizeof(h),"%02x",b);s+=h;}return s;}
String digest(const String& value){
  uint8_t result[32];mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),reinterpret_cast<const unsigned char*>(value.c_str()),value.length(),result);
  String s;s.reserve(64);for(auto b:result){char h[3];snprintf(h,sizeof(h),"%02x",b);s+=h;}return s;
}
bool same(const String& a,const String& b){if(a.length()!=b.length())return false;unsigned diff=0;for(unsigned i=0;i<a.length();i++)diff|=a[i]^b[i];return diff==0;}
void reply(int code,const String& data){server.sendHeader("Cache-Control","no-store");server.sendHeader("X-Content-Type-Options","nosniff");server.send(code,"application/json",data);}
void error(int code,const char* text){reply(code,String("{\"error\":\"")+text+"\"}");}
String cookie(){
 String c=server.header("Cookie");int start=0;
 while(start<int(c.length())){int end=c.indexOf(';',start);if(end<0)end=c.length();String part=c.substring(start,end);part.trim();if(part.startsWith("costume="))return part.substring(8);start=end+1;}return "";
}
int session(){String token=cookie();return accessControl.find(token.c_str(),millis());}
bool mutation(int id,bool entry=false){
 // Browser mutations require a custom header; no permissive CORS or OPTIONS handler.
 String expected=entry?"entry":(id>=0?String(accessControl.sessions[id].token):String());
 if(!expected.length() || !same(server.header("X-CSRF"),expected)){error(403,"Reload the access page");return false;}return true;
}
void newSession(bool owner){
 int old=session();if(old>=0)accessControl.logout(old);
 // Valid owner credentials must still allow recovery when visitors fill the table.
 if(owner){bool room=false;for(auto& s:accessControl.sessions)if(!s.token[0])room=true;
  if(!room){int victim=0;for(int i=0;i<8;i++)if(!accessControl.sessions[i].owner){victim=i;break;}accessControl.logout(victim);}}
 String token=randomToken();int id=accessControl.create(token.c_str(),owner,millis());
 if(id<0){error(503,"Session capacity reached; try again later");return;}
 server.sendHeader("Set-Cookie",String("costume=")+token+"; HttpOnly; SameSite=Strict; Path=/");reply(200,"{}");
}
bool number(const char* key,int low,int high,int& out){
 String s=server.arg(key);if(!s.length()||s.length()>8)return false;
 for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return false;
 long n=s.toInt();if(n<low||n>high)return false;out=int(n);return true;
}
bool color(RGB& c){String s=server.arg("color");if(s.length()!=6)return false;for(unsigned i=0;i<6;i++)if(!isxdigit(s[i]))return false;uint32_t v=strtoul(s.c_str(),nullptr,16);c={uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};return true;}
String hexColor(RGB c){char s[8];snprintf(s,sizeof(s),"#%02x%02x%02x",c.r,c.g,c.b);return s;}
void snapshot(int id){
 String j;j.reserve(14000);
 j="{\"owner\":"+String(accessControl.owner(id)?"true":"false")+",\"mine\":"+(accessControl.controller==id?"true":"false")+",\"controller\":"+(accessControl.controller>=0?"true":"false")+",\"open\":"+(accessControl.open?"true":"false")+",\"simulation\":"+(SIMULATION?"true":"false")+",\"leds\":"+(ENABLE_LEDS?"true":"false");
 j+=",\"idleGlow\":"+String(engine.idleGlow?"true":"false");
 const char* mode=engine.heart.live(millis())?"live":(engine.heart.period?"predicted":"waiting");
 j+=",\"heart\":{\"mode\":\""+String(mode)+"\",\"bpm\":"+String(engine.heart.period?60000.0f/engine.heart.period:0,1)+"},\"dropped\":"+String(engine.dropped)+",\"collisions\":"+String(engine.collisions)+",\"effect\":"+String(int(engine.activeShot()))+",\"sections\":[";
 for(unsigned s=0;s<SECTIONS;s++){
  if(s)j+=',';auto& m=engine.manual[s];
  const char* tx=!ENABLE_LEDS?"disabled":(!outputs.configured?"configuration incomplete":outputs.ready(s)?"ready":"fault");
  j+="{\"name\":\""+String(NAMES[s])+"\",\"manual\":"+(m.active?"true":"false")+",\"color\":\""+hexColor(m.color)+"\",\"brightness\":"+String(m.brightness)+",\"pattern\":"+String(int(m.pattern))+",\"speed\":"+String(m.speed)+",\"reverse\":"+(m.reverse?"true":"false")+",\"tx\":\""+tx+"\",\"frames\":"+String(outputs.frames(s))+",\"error\":\""+outputs.error(s)+"\",\"length\":"+String(LENGTHS[s])+",\"paint\":[";
  bool comma=false;
  if(m.active&&m.pattern==PAINT)for(unsigned i=0;i<LENGTHS[s];i++)if(engine.paint[s][i].used){
   const auto& p=engine.paint[s][i];if(comma)j+=',';comma=true;
   j+="["+String(i)+",\""+hexColor(p.color)+"\","+String(p.at)+","+(p.fade?"true":"false")+"]";
  }
  j+="]}";
 }
 j+="],\"sensors\":[";
 for(unsigned i=0;i<5;i++){
  if(i)j+=',';Branch b=sensors.branch(i);
  j+="{\"name\":"+jsonText(sensors.name(i))+",\"present\":"+(engine.sensorPresent[i]?"true":"false")+",\"age\":"+(engine.sensorPresent[i]?String(uint32_t(millis()-engine.sensorAt[i])):String("null"))+",\"faults\":"+String(engine.faults[i])+",\"samples\":"+String(sensors.samples[i])+",\"bus\":"+String(b.mux+1)+",\"channel\":"+String(b.channel)+",\"address\":"+String(b.address)+",\"status\":"+jsonText(sensors.problem(i))+"}";
 }
 j+="],\"buses\":[";
 for(unsigned i=0;i<2;i++){
  if(i)j+=',';j+="{\"sda\":"+String(SDA_PINS[i])+",\"scl\":"+String(SCL_PINS[i])+",\"ready\":"+(sensors.busReady(i)?"true":"false")+",\"mux\":"+String(sensors.muxAddress(i))+"}";
 }
 j+="],\"motion\":[";
 for(unsigned i=0;i<2;i++){
  if(i)j+=',';j+="{\"ax\":"+String(sensors.motionG[i][0],3)+",\"ay\":"+String(sensors.motionG[i][1],3)+",\"az\":"+String(sensors.motionG[i][2],3)+",\"magnitude\":"+String(sensors.magnitude[i],3)+",\"rotationDegrees\":"+String(sensors.rotationDegrees[i],1)+",\"gx\":"+String(sensors.gyroDps[i][0],1)+",\"gy\":"+String(sensors.gyroDps[i][1],1)+",\"gz\":"+String(sensors.gyroDps[i][2],1)+",\"calibrationRemaining\":"+String(sensors.calibrationRemaining(i))+",\"lastStepAge\":"+(engine.steps[i]?String(uint32_t(millis()-sensors.stepAt[i])):String("null"))+"}";
 }
 j+="],\"touchDebug\":[";
 for(unsigned a=0;a<2;a++){
  if(a)j+=',';j+="{\"oor\":"+String(sensors.touchOutOfRange[a])+",\"hardwareMask\":"+String(sensors.touchHardwareMask[a])+",\"filtered\":[";
  for(unsigned e=0;e<12;e++){if(e)j+=',';j+=String(sensors.touchFiltered[a][e]);}
  j+="],\"baseline\":[";for(unsigned e=0;e<12;e++){if(e)j+=',';j+=String(sensors.touchBaseline[a][e]);}j+="]}";
 }
 j+="],\"heartRaw\":{\"ir\":"+String(sensors.latestIR)+",\"red\":"+String(sensors.latestRed)+",\"finger\":"+(sensors.fingerPresent?"true":"false")+",\"signal\":"+String(sensors.heartSignal,1)+",\"accepted\":"+String(sensors.acceptedBeats)+",\"rejected\":"+String(sensors.rejectedBeats)+"}";
 j+=",\"render\":"+renderState(engine,renderBootId);
 j+=",\"touch\":["+String(engine.touchMask(0))+","+String(engine.touchMask(1))+"],\"steps\":["+String(engine.steps[0])+","+String(engine.steps[1])+"]}";reply(200,j);
}
void command(int id){
 if(!accessControl.command(id,millis())){error(409,"Claim control first");return;}
 String op=server.arg("op");uint32_t now=millis();int s=0,a=0,b=0,c=0,d=0;RGB rgb;
 if(op=="all"){engine.oneShot(NO_SHOT,now);for(unsigned i=0;i<SECTIONS;i++)engine.sensors(i);}
 else if(op=="idleGlow"){
  if(!number("enabled",0,1,a)){error(400,"Invalid idle setting");return;}
  if(prefs.putBool("idleGlow",a!=0)!=1){error(500,"Could not save idle setting");return;}
  engine.idleGlow=a!=0;engine.log.record(now,"IDLE_GLOW enabled=%u",a);
 }
 else if(op=="oneshot"){
  if(!number("effect",0,3,a)){error(400,"Invalid one-shot effect");return;}engine.oneShot(OneShot(a),now);engine.log.record(now,"ONE_SHOT effect=%d",a);
 }
 else if(op=="calibrate"){
  String left=server.arg("left"),right=server.arg("right");char* tail;
  float l=strtof(left.c_str(),&tail);if(!left.length()||*tail||!isfinite(l)||l<0.03f||l>2){error(400,"Invalid sensitivity");return;}
  float r=strtof(right.c_str(),&tail);if(!right.length()||*tail||!isfinite(r)||r<0.03f||r>2){error(400,"Invalid sensitivity");return;}
  engine.sensitivity[0]=l;engine.sensitivity[1]=r;sensors.calibrate();
 }else if(op=="beat"||op=="loss"||op=="touch"||op=="step"||op=="bothSteps"){
  if(!SIMULATION){error(403,"Simulation is disabled");return;}
  if(op=="beat"){bool accepted=engine.heart.beat(now);engine.log.record(now,"BEAT source=simulated accepted=%u",accepted?1u:0u);}
  if(op=="bothSteps"){engine.step(0,now);engine.step(1,now);engine.log.record(now,"STEP source=simulated leg=both");}
  if(op=="loss"){engine.heart.disconnect();for(unsigned i=0;i<2;i++)engine.touch(i,0,false,now,true);}
  if(op=="step"||op=="touch"){
   if(!number("s",0,1,s)){error(400,"Invalid side");return;}
   if(op=="step"){engine.step(s,now);engine.log.record(now,"STEP source=simulated leg=%s",s?"right":"left");}
   else {if(!number("mask",0,4095,a)){error(400,"Invalid touch mask");return;}engine.touch(s,a,true,now,true);}
  }
 }else{
  if(!number("s",0,SECTIONS-1,s)){error(400,"Invalid section");return;}
  if(op=="sensors")engine.sensors(s);
  else if(op=="clear"){engine.clear(s);engine.manual[s].active=true;engine.manual[s].pattern=PAINT;}
  else if(op=="manual"){
   if(!color(rgb)||!number("brightness",0,255,a)||!number("pattern",0,2,b)||!number("speed",100,10000,c)||!number("reverse",0,1,d)){error(400,"Invalid manual settings");return;}
   auto& m=engine.manual[s];m.active=true;m.color=rgb;m.brightness=a;m.pattern=Pattern(b);m.speed=c;m.reverse=d;
  }else if(op=="brush"){
   if(!color(rgb)||!number("from",0,LENGTHS[s]-1,a)||!number("to",0,LENGTHS[s]-1,b)||!number("width",1,MAX_PIXELS,c)||!number("fade",0,1,d)){error(400,"Invalid brush");return;}
   if(a>b){int swap=a;a=b;b=swap;}for(int i=a;i<=b;i++)engine.brush(s,i,c,rgb,d,now);
  }else{error(400,"Unknown command");return;}
 }
 reply(200,"{}");
}
void provision(){
 // USB-only recovery. Never print passwords or hashes.
 unsigned budget=64;
 while(budget-- && Serial.available()){
  char c=Serial.read();if(c=='\r')continue;
  if(c=='\n'){
   if(serialLine.startsWith("password ") && serialLine.length()>=17){
    salt=randomToken();passwordHash=digest(salt+serialLine.substring(9));prefs.putString("salt",salt);prefs.putString("hash",passwordHash);
    accessControl=Access();blocked=false;failures=0;engine.log.record(millis(),"ACCESS password saved; website locked; all sessions revoked");
   }else engine.log.record(millis(),"HELP set/recover access: password YOUR_PASSWORD (at least 8 characters)");
   serialLine="";
  }else if(serialLine.length()<128)serialLine+=c;
 }
}
void setup(){
 renderBootId=esp_random();Serial.begin(115200);prefs.begin("costume",false);engine.idleGlow=prefs.getBool("idleGlow",false);salt=prefs.getString("salt","");passwordHash=prefs.getString("hash","");
 Serial.println("Costume: use USB command 'password YOUR_PASSWORD' to set/recover owner access.");
 engine.log.record(millis(),"BOOT fw=measured-leds-1 mode=%s sensors_enabled=%u leds_enabled=%u",SIMULATION?"simulated":"physical",ENABLE_SENSORS?1u:0u,ENABLE_LEDS?1u:0u);
 engine.log.record(millis(),"DETECTION step_extra_g=%.2f rest_initial_g=1.0 rough_touch=%u idle=%u press_below=%u",double(DEFAULT_STEP_THRESHOLD_G),USE_ROUGH_TOUCH?1u:0u,unsigned(ROUGH_TOUCH_IDLE),unsigned(ROUGH_TOUCH_BELOW));
 bool sensorsReady=sensors.begin();outputs.begin();
 engine.log.record(millis(),"MONITOR state=%s IMU_A_B_sides=unassigned touch_A_B_sides=left_right",!ENABLE_SENSORS?"simulated":sensorsReady?"physical_discovery":"I2C_begin_failed");
 for(unsigned i=0;i<SECTIONS;i++)engine.log.record(millis(),"LED section=%s pin=%d length=%u state=%s error=%s",NAMES[i],LED_PINS[i],LENGTHS[i],!ENABLE_LEDS?"disabled":outputs.ready(i)?"ready":"not_ready",outputs.error(i));
 WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME);dns.start(53,"*",WiFi.softAPIP());
 const char* headers[]={"Cookie","X-CSRF"};server.collectHeaders(headers,2);
 server.on("/",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.sendHeader("X-Frame-Options","DENY");server.send_P(200,"text/html",COSTUME_PAGE);});
 server.on("/api/access",HTTP_GET,[]{int id=session();reply(200,String("{\"open\":")+(accessControl.open?"true":"false")+",\"authorized\":"+(accessControl.view(id)?"true":"false")+",\"csrf\":\""+(id>=0?String(accessControl.sessions[id].token):String("entry"))+"\"}");});
 server.on("/api/login",HTTP_POST,[]{
  if(!mutation(-1,true))return;uint32_t now=millis();
  if(blocked&&uint32_t(now-blockedAt)<30000){error(429,"Wait 30 seconds before retrying");return;}blocked=false;
  if(attemptedLogin&&uint32_t(now-loginAt)<1000){error(429,"Wait before retrying");return;}attemptedLogin=true;loginAt=now;
  if(!passwordHash.length()||!same(digest(salt+server.arg("password")),passwordHash)){
   if(++failures>=5){blocked=true;blockedAt=now;failures=0;}error(403,"Sign-in failed; initial setup or recovery uses USB");return;
  }failures=0;newSession(true);
 });
 server.on("/api/visitor",HTTP_POST,[]{if(!mutation(-1,true))return;if(!accessControl.open){error(403,"Access is locked");return;}newSession(false);});
 server.on("/api/state",HTTP_GET,[]{int id=session();if(!accessControl.view(id)){error(403,"Access is locked");return;}snapshot(id);});
 server.on("/api/logs",HTTP_GET,[]{int id=session();if(!accessControl.view(id)){error(403,"Access is locked");return;}diagnosticSnapshot();});
 server.on("/api/claim",HTTP_POST,[]{int id=session();if(!mutation(id))return;if(!accessControl.claim(id,millis())){error(409,"Control is unavailable");return;}reply(200,"{}");});
 server.on("/api/renew",HTTP_POST,[]{int id=session();if(!mutation(id))return;if(!accessControl.command(id,millis())){error(409,"Control lease expired");return;}reply(200,"{}");});
 server.on("/api/open",HTTP_POST,[]{int id=session();if(!mutation(id))return;int value;if(!number("value",0,1,value)){error(400,"Invalid access setting");return;}if(!accessControl.setOpen(id,value)){error(403,"Owner access required");return;}reply(200,"{}");});
 server.on("/api/logout",HTTP_POST,[]{int id=session();if(!mutation(id))return;accessControl.logout(id);server.sendHeader("Set-Cookie","costume=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/");reply(200,"{}");});
 server.on("/api/command",HTTP_POST,[]{int id=session();if(!mutation(id))return;command(id);});
 server.onNotFound([]{if(server.uri().startsWith("/api/"))error(404,"Unknown endpoint");else{server.sendHeader("Location","/");server.send(302,"text/plain","");}});
 if(SIMULATION){engine.touch(0,0,true,millis(),true);engine.touch(1,0,true,millis(),true);}server.begin();
}
void loop(){
 uint32_t now=millis();provision();accessControl.tick(now);sensors.tick(now);
 if(uint32_t(now-lastRender)>=20){lastRender=now;engine.render(now);}outputs.tick(engine,now);
 dns.processNextRequest();server.handleClient();serviceDiagnostics(millis());delay(1);
}
