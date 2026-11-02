#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "CostumeRmtOutput.h"
#include "Page.h"

constexpr unsigned STRIPS=6, LIMIT=256, RECORDS=32;
const int pins[STRIPS]={16,17,32,33,4,18};
const char* names[STRIPS]={"Left arm","Right arm","Left leg","Right leg","Spine","Heart"};
struct Mapping { char name[40]; uint16_t strip,first,last; };
struct Settings { uint32_t version; uint16_t lengths[STRIPS]; Mapping records[RECORDS]; };
Settings settings={1,{120,120,60,60,100,120},{}};
Preferences prefs;
WebServer server(80);
CostumeRmtOutput<LIMIT> outputs[STRIPS];
uint8_t frame[LIMIT*3]={};
int active=-1,firstLed=1,lastLed=1,brightness=16;
uint32_t color=0x00ff00;
bool dirty=true;

bool number(const char* key,int low,int high,int& result){
  if(!server.hasArg(key))return false;
  String value=server.arg(key); if(!value.length()||value.length()>6)return false;
  for(unsigned i=0;i<value.length();i++)if(value[i]<'0'||value[i]>'9')return false;
  result=value.toInt();return result>=low&&result<=high;
}
void bad(){server.send(400,"text/plain","Invalid value or range. LED numbers start at 1.");}
bool range(int& strip,int& first,int& last){
 return number("strip",0,5,strip)&&number("first",1,settings.lengths[strip],first)&&number("last",first,settings.lengths[strip],last);
}
bool save(){return prefs.putBytes("settings",&settings,sizeof(settings))==sizeof(settings);}
void state(){
 String out="{\"limit\":256,\"strips\":[";
 for(unsigned i=0;i<STRIPS;i++){
  if(i)out+=',';
  out+="{\"name\":\""+String(names[i])+"\",\"pin\":"+String(pins[i])+",\"length\":"+String(settings.lengths[i])+",\"ready\":"+(outputs[i].ready()?String("true"):String("false"))+"}";
 }
 out+="],\"records\":[";bool comma=false;
 for(unsigned i=0;i<RECORDS;i++)if(settings.records[i].name[0]){
  const Mapping& m=settings.records[i];if(comma)out+=',';comma=true;
  out+="{\"slot\":"+String(i)+",\"name\":\""+String(m.name)+"\",\"strip\":"+String(m.strip)+",\"first\":"+String(m.first)+",\"last\":"+String(m.last)+"}";
 }
 out+="]}";server.send(200,"application/json",out);
}
void setup(){
 Serial.begin(115200);
 prefs.begin("led-mapper",false);
 if(prefs.getBytesLength("settings")==sizeof(settings)){
  Settings stored; prefs.getBytes("settings",&stored,sizeof(stored));bool valid=stored.version==1;
  for(unsigned i=0;i<STRIPS;i++)if(stored.lengths[i]<1||stored.lengths[i]>LIMIT)valid=false;
  for(unsigned i=0;i<RECORDS;i++){
   const Mapping& m=stored.records[i];
   if(m.name[0]&&(m.name[39]!=0||m.strip>=STRIPS||m.first<1||m.last<m.first||m.last>stored.lengths[m.strip]))valid=false;
  }
  if(valid)settings=stored;
 }
 for(unsigned i=0;i<STRIPS;i++)Serial.printf("LED_OUTPUT pin=%d ready=%d\n",pins[i],outputs[i].begin(pins[i]));
 WiFi.mode(WIFI_AP);WiFi.softAP("Costume-LED-Test");
 server.on("/",HTTP_GET,[]{server.send_P(200,"text/html",MAPPER_PAGE);});
 server.on("/api/state",HTTP_GET,state);
 server.on("/api/off",HTTP_POST,[]{active=-1;dirty=true;Serial.println("LED_OFF");server.send(200,"text/plain","All off");});
 server.on("/api/test",HTTP_POST,[]{
  int s,f,l,b;if(!range(s,f,l)||!number("brightness",1,64,b)){bad();return;}
  String hex=server.arg("color");if(hex.length()!=6){bad();return;}
  for(unsigned i=0;i<6;i++)if(!isxdigit(hex[i])){bad();return;}
  if(!outputs[s].ready()){server.send(503,"text/plain","LED output unavailable; check USB log.");return;}
  active=s;firstLed=f;lastLed=l;brightness=b;color=strtoul(hex.c_str(),nullptr,16);dirty=true;
  Serial.printf("LED_TEST pin=%d first=%d last=%d brightness=%d\n",pins[s],f,l,b);
  server.send(200,"text/plain","Testing selected range");
 });
 server.on("/api/length",HTTP_POST,[]{
  int s,n;if(!number("strip",0,5,s)||!number("length",1,LIMIT,n)){bad();return;}
  for(const auto& m:settings.records)if(m.name[0]&&m.strip==s&&m.last>n){server.send(409,"text/plain","Delete saved ranges beyond the new length first.");return;}
  settings.lengths[s]=n;active=-1;dirty=true;
  if(!save()){server.send(500,"text/plain","Could not save to flash");return;}
  Serial.printf("LED_LENGTH pin=%d length=%d\n",pins[s],n);server.send(200,"text/plain","Length saved");
 });
 server.on("/api/record",HTTP_POST,[]{
  int s,f,l;if(!range(s,f,l)){bad();return;}
  String name=server.arg("name");name.trim();if(!name.length()||name.length()>39){bad();return;}
  for(unsigned i=0;i<name.length();i++)if(!isalnum(name[i])&&name[i]!=' '&&name[i]!='_'&&name[i]!='-'){bad();return;}
  int slot=-1;
  for(unsigned i=0;i<RECORDS;i++)if(name==settings.records[i].name){slot=i;break;}
  if(slot<0)for(unsigned i=0;i<RECORDS;i++)if(!settings.records[i].name[0]){slot=i;break;}
  if(slot<0){server.send(409,"text/plain","All 32 records are used. Delete one first.");return;}
  Mapping& m=settings.records[slot];memset(&m,0,sizeof(m));name.toCharArray(m.name,sizeof(m.name));m.strip=s;m.first=f;m.last=l;
  if(!save()){server.send(500,"text/plain","Could not save to flash");return;}
  Serial.printf("LED_RECORD name=%s pin=%d first=%d last=%d\n",m.name,pins[s],f,l);server.send(200,"text/plain","Mapping saved");
 });
 server.on("/api/delete",HTTP_POST,[]{
  int slot;if(!number("slot",0,RECORDS-1,slot)){bad();return;}
  memset(&settings.records[slot],0,sizeof(Mapping));
  server.send(save()?200:500,"text/plain","Record deleted; check status if saving failed");
 });
 server.begin();Serial.println("LED mapper: connect to Costume-LED-Test, open http://192.168.4.1");
}
void loop(){
 server.handleClient();
 for(unsigned s=0;s<STRIPS;s++){
  memset(frame,0,sizeof(frame));
  if(active==int(s))for(int n=firstLed;n<=lastLed;n++){
   frame[(n-1)*3]=((color>>8)&255)*brightness/255;
   frame[(n-1)*3+1]=((color>>16)&255)*brightness/255;
   frame[(n-1)*3+2]=(color&255)*brightness/255;
  }
  if(dirty)outputs[s].markDirty();outputs[s].service(frame);
 }
 dirty=false;delay(1);
}
