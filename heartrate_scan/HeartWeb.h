#pragma once
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);
constexpr unsigned WAVE_SIZE = 125; // five seconds at 25 samples/s
float wave[WAVE_SIZE] = {};
bool waveBeat[WAVE_SIZE] = {};
uint32_t waveTime[WAVE_SIZE] = {};
unsigned waveNext = 0, waveCount = 0;

void rememberWave(float value, bool beat) {
  wave[waveNext] = value;
  waveBeat[waveNext] = beat;
  waveTime[waveNext] = millis();
  waveNext = (waveNext + 1) % WAVE_SIZE;
  if (waveCount < WAVE_SIZE) waveCount++;
}

const char HEART_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Heart monitor</title><style>
body{font:18px system-ui;background:#10151e;color:#edf2fa;max-width:760px;margin:30px auto;padding:16px}
.cards{display:flex;flex-wrap:wrap;gap:12px}.card{background:#202b3b;border-radius:12px;padding:18px;flex:1;min-width:130px}
strong{display:block;font-size:36px}small,p{color:#b8c6d9}canvas{width:100%;height:220px;background:#182231;border-radius:12px;margin-top:20px}
#pulse{display:inline-block;color:#ff6688;transition:transform .15s} .hit{transform:scale(1.5)}
</style><h1><span id="pulse">&#9829;</span> Heart monitor</h1>
<p id="status" role="status">Connecting...</p><div class="cards">
<div class="card">Approx. BPM<strong id="bpm">--</strong></div>
<div class="card">Red<strong id="red">--</strong></div>
<div class="card">Infrared<strong id="ir">--</strong></div></div>
<canvas id="wave" width="900" height="260" aria-label="Filtered infrared waveform"></canvas>
<small>Last 5 seconds · Pink: filtered IR · Yellow dots: detected beats</small>
<p id="details"></p><small>Rest your finger gently on the sensor. The waveform scales automatically. BPM is an approximate visual beat estimate.</small>
<script>
const el=id=>document.getElementById(id);let lastBeat=null;
function draw(values,ages=[],beats=[]){const c=el('wave'),x=c.getContext('2d');x.clearRect(0,0,c.width,c.height);
const left=45,right=c.width-30,top=18,bottom=c.height-42,height=bottom-top;
x.font='16px system-ui';x.textBaseline='top';
for(let seconds=0;seconds<=5;seconds++){const px=right-seconds/5*(right-left);
x.strokeStyle='#33445a';x.lineWidth=1;x.beginPath();x.moveTo(px,top);x.lineTo(px,bottom);x.stroke();
x.fillStyle='#b8c6d9';x.textAlign=seconds===0?'right':seconds===5?'left':'center';x.fillText(seconds?`-${seconds}s`:'Now',px,bottom+10)}
x.strokeStyle='#33445a';x.beginPath();x.moveTo(left,top+height/2);x.lineTo(right,top+height/2);x.stroke();
const points=values.map((v,i)=>({v,age:ages[i],beat:beats[i]})).filter(p=>p.age>=0&&p.age<=5000);
if(!points.length)return;const scale=Math.max(20,...points.map(p=>Math.abs(p.v)));
const px=p=>right-p.age/5000*(right-left),py=p=>top+height/2-p.v/scale*height*.44;
x.strokeStyle='#ff6688';x.lineWidth=3;x.beginPath();
points.forEach((p,i)=>{i&&points[i-1].age-p.age<200?x.lineTo(px(p),py(p)):x.moveTo(px(p),py(p))});x.stroke();
points.filter(p=>p.beat).forEach(p=>{x.fillStyle='#ffd166';x.beginPath();x.arc(px(p),py(p),6,0,Math.PI*2);x.fill()})}
async function refresh(){try{const r=await fetch('/data',{cache:'no-store'});if(!r.ok)throw Error();const d=await r.json();
const live=d.ready&&d.ageMs<1000;
el('status').textContent=d.status+(live?(d.finger?' · Finger detected':' · Place a finger on the sensor'):' · Waiting for fresh readings');
el('bpm').textContent=d.bpm>0?d.bpm.toFixed(1):'--';
el('red').textContent=live?d.red:'--';el('ir').textContent=live?d.ir:'--';
el('details').textContent=`Samples: ${d.samples} · Detected beats: ${d.beats} · Rejected intervals: ${d.rejected} · ${live&&d.finger?'Tracking':'Holding last rhythm'} · SDA ${d.sda} / SCL ${d.scl} · Mux port 7`;
if(lastBeat!==null&&d.beats!==lastBeat&&live){el('pulse').classList.add('hit');setTimeout(()=>el('pulse').classList.remove('hit'),150)}lastBeat=d.beats;
draw(live?d.wave:[],d.ages,d.beatMarks);
}catch(e){el('status').textContent='Connection lost. Reconnect to HeartMonitor Wi-Fi.';['bpm','red','ir'].forEach(id=>el(id).textContent='--');draw([])}
setTimeout(refresh,200)}refresh();
</script></html>)HTML";

void sendHeartData() {
  String json;
  json.reserve(4000);
  const uint32_t now = millis();
  json = "{\"status\":\""; json += sensorStatus;
  json += "\",\"ready\":"; json += sensorReady ? "true" : "false";
  json += ",\"finger\":"; json += filter.finger ? "true" : "false";
  json += ",\"ageMs\":"; json += String(millis() - lastSample);
  json += ",\"bpm\":"; json += String(rhythm.bpm(), 1);
  json += ",\"rejected\":"; json += String(rhythm.rejected);
  json += ",\"red\":"; json += String(latestRed);
  json += ",\"ir\":"; json += String(latestIR);
  json += ",\"samples\":"; json += String(sampleCount);
  json += ",\"beats\":"; json += String(beatCount);
  json += ",\"sda\":"; json += String(SDA_PIN);
  json += ",\"scl\":"; json += String(SCL_PIN);
  json += ",\"wave\":[";
  for (unsigned i = 0; i < waveCount; i++) {
    if (i) json += ',';
    json += String(wave[(waveNext + WAVE_SIZE - waveCount + i) % WAVE_SIZE], 1);
  }
  json += "],\"ages\":[";
  for (unsigned i = 0; i < waveCount; i++) {
    if (i) json += ',';
    json += String(now - waveTime[(waveNext + WAVE_SIZE - waveCount + i) % WAVE_SIZE]);
  }
  json += "],\"beatMarks\":[";
  for (unsigned i = 0; i < waveCount; i++) {
    if (i) json += ',';
    json += waveBeat[(waveNext + WAVE_SIZE - waveCount + i) % WAVE_SIZE] ? '1' : '0';
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void startWebsite() {
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP("HeartMonitor", "heartbeat123")) {
    Serial.println("ERROR: Wi-Fi access point could not start.");
    return;
  }
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", HEART_PAGE); });
  server.on("/data", HTTP_GET, sendHeartData);
  server.begin();
  Serial.print("Connect to HeartMonitor (password heartbeat123), then open http://");
  Serial.println(WiFi.softAPIP());
}
