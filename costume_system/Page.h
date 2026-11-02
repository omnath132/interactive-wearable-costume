#pragma once
const char COSTUME_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Costume</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{font:16px system-ui;margin:auto;max-width:1000px;padding:20px;background:#0b111c;color:#e0efff}button,input,select{font:inherit;padding:10px;border:1px solid #455a76;border-radius:8px;background:#17253a;color:inherit}button{cursor:pointer}button:hover{background:#2a4569}button:disabled{opacity:.4;cursor:default}label{display:inline-flex;align-items:center;gap:8px;margin:6px}input[type=number]{width:90px}input[type=color]{padding:3px;width:60px;height:42px}.panel{background:#121f30;border-radius:14px;padding:16px;margin:12px 0}.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}.layout{display:grid;grid-template-columns:minmax(240px,1fr) minmax(260px,1fr);gap:16px}svg{width:100%;max-height:650px;touch-action:none;user-select:none}polyline{fill:none;stroke:#253c56;stroke-width:15;stroke-linecap:round}circle.pixel{pointer-events:none}#message{min-height:25px;color:#a9d8ff}pre{white-space:pre-wrap;font:13px system-ui}[hidden]{display:none!important}@media(max-width:650px){.layout{grid-template-columns:1fr}svg{height:400px}}
</style><h1>Costume</h1><p id="message" role="status"></p>
<section id="gate" class="panel"><h2>Access</h2><form id="login"><label>Owner password <input id="password" type="password" required autocomplete="current-password"></label><button>Sign in</button></form><button id="visitor" hidden>Enter as visitor</button></section>
<main id="main" hidden>
<div class="panel row"><strong id="ownership"></strong><button id="claim">Claim control</button><button id="logout">Sign out</button><button id="all" data-control>Return all to sensors</button></div>
<section class="panel"><label><input id="idleGlow" type="checkbox" data-control> Faint blue at rest</label><small>Off when unchecked. Applies to arms and legs; the spine waits for arriving pulses. Saved after restart.</small></section><p id="mode"></p><section class="panel"><h2>Sensor monitor</h2><pre id="sensorMonitor" aria-live="polite"></pre></section><div class="layout"><div class="panel"><svg id="body" viewBox="0 0 400 500" aria-label="Costume LED sections">
<circle cx="200" cy="45" r="26" fill="#253c56"/>
<polyline data-section="0" points="65,225 110,155 200,114"/>
<polyline data-section="1" points="335,225 290,155 200,114"/>
<polyline data-section="2" points="130,465 155,350 200,260"/>
<polyline data-section="3" points="270,465 245,350 200,260"/>
<polyline data-section="4" points="200,260 200,180 200,95"/>
<polyline data-section="5" points="180,165 172,145 180,135 190,143 200,135 208,145 200,165 190,175 180,165"/>
<g id="pixels"></g></svg><small>Live animation from controller timing and events. LED 0 meets the spine; arms join at its top 12 LEDs. <span id="visualSync" role="status"></span></small></div>
<div><section class="panel"><h2 id="sectionTitle">Left arm</h2><label>Section <select id="section"></select></label>
<div class="row"><label>Color <input id="color" type="color" value="#4696ff"></label><label>Brightness <input id="bright" type="range" min="0" max="255" value="120"></label></div>
<label>Animation <select id="pattern"><option value="0">Solid</option><option value="1">Breathe</option><option value="2">Chase</option></select></label>
<label>Cycle (ms) <input id="speed" type="number" min="100" max="10000" value="1200"></label>
<label>Direction <select id="direction"><option value="0">Forward</option><option value="1">Reverse</option></select></label>
<div class="row"><button id="apply" data-control>Apply override</button><button id="return" data-control>Return section to sensors</button><button id="clear" data-control>Clear section</button></div>
<h3>Draw on the body</h3><label>Brush <select id="brushMode"><option value="select">Select only</option><option value="paint">Paint</option><option value="fade">Fade trail</option></select></label><label>Width <input id="width" type="number" min="1" max="96" value="10"></label><p>Choose Paint or Fade, then drag along a section. Drawing activates its manual override.</p></section>
<section id="ownerPanel" class="panel" hidden><h3>Owner access</h3><button id="open">Open visitor access</button><button id="lock">Lock visitor access</button></section>
<section class="panel"><h3>Walking sensitivity</h3><p>Thigh steps need extra acceleration plus 6 deg rotation within 800 ms. Default: 0.15 g above rest (roughly 1.15 g total). Stand still when calibrating.</p><label>IMU A extra g <input id="leftSensitivity" type="number" value="0.15" min="0.03" max="2" step="0.01"></label><label>IMU B extra g <input id="rightSensitivity" type="number" value="0.15" min="0.03" max="2" step="0.01"></label><button id="calibrate" data-control>Calibrate at rest</button></section></div></div>
<section class="panel"><h2>One-shot effects</h2><div class="row"><button id="lightning" data-control>Lightning</button><button id="energyWave" data-control>Energy wave</button><button id="heartRestart" data-control>Heart restart</button><button id="stopEffect" data-control>Stop effect</button></div><p id="effectStatus" role="status">No effect active</p><small>Temporary effects cover all sections, then restore your controls. Return all to sensors also stops the effect.</small></section>
<section id="simulation" class="panel" hidden><h2>Sensor simulation</h2><p>Hold an arm button to charge its glow, then release to launch a larger pulse. Try both steps together for a yellow collision flare on the spine.</p><div class="row"><button id="beat" data-control>Real beat</button><button id="leftStep" data-control>Left step</button><button id="rightStep" data-control>Right step</button><button id="bothSteps" data-control>Both steps</button><button id="leftTouch" data-control>Hold left arm</button><button id="rightTouch" data-control>Hold right arm</button></div><button id="autoBeat" data-control>Start 60 BPM input</button><button id="loss" data-control>Stop input / signal loss</button></section>
<section class="panel"><h2>Live state</h2><pre id="status"></pre></section>
<section class="panel"><h2>Diagnostic log</h2><button id="downloadLog">Download recent log</button><p id="logInfo"></p><pre id="diagnosticLog" aria-label="Recent diagnostic events"></pre></section></main>
<script>
// LOCAL_RENDER_BEGIN
const elapsed=(now,then)=>(now-then)>>>0;
const stripLength=s=>s.length??s.pixels?.length??0;
function renderLocal(j,now){
 const r=j.render;if(!r)return j.sections.map(s=>(s.pixels||[]).map(c=>[1,3,5].map(i=>parseInt(c.slice(i,i+2),16))));
 const clamp=x=>Math.max(0,Math.min(1,x)),cap=r.cap,blue=[8,35,160];
 const scale=(c,x)=>c.map(v=>Math.trunc(v*clamp(x)));
 const add=(a,b)=>a.map((v,i)=>Math.min(cap,v+b[i]));
 const rgb=c=>[1,3,5].map(i=>parseInt(c.slice(i,i+2),16));
 const pixels=j.sections.map(s=>Array.from({length:stripLength(s)},()=>[0,0,0]));
 if(j.idleGlow)for(let s=0;s<4;s++)pixels[s]=pixels[s].map(()=>scale(blue,.06));
 const h=r.heart;let flashAt=h.flashAt,flashed=h.flashed;
 if(h.period&&((now-h.nextBeat)|0)>=0){const due=(h.nextBeat+Math.floor(elapsed(now,h.nextBeat)/h.period)*h.period)>>>0;if(!flashed||elapsed(due,flashAt)>140){flashAt=due;flashed=true;}}
 const heartAge=elapsed(now,flashAt),level=!flashed?0:heartAge<140?Math.trunc(160*(1-heartAge/140)):heartAge>=180&&heartAge<320?Math.trunc(95*(1-(heartAge-180)/140)):0;
 pixels[5]=pixels[5].map(()=>[level,0,0]);
 for(let a=0;a<2;a++)if(r.touchValid[a])for(let e=0;e<12;e++)if(j.touch[a]&(1<<e)){
  const [first,last]=r.regions[a][e],color=scale(blue,.2+.8*clamp(elapsed(now,r.touchAt[a][e])/r.chargeMs));
  for(let i=first;i<Math.min(last,pixels[a].length);i++)pixels[a][i]=add(pixels[a][i],color);
 }
 const position=(p,t)=>p.start-elapsed(t,p.at)*r.speed/1000;
 const radiusOf=p=>p.radius??((5+7*p.charge)*(p.limb>=2?.5:1));
 const active=r.pulses.filter(p=>position(p,now)>=0||r.junction[p.limb]-position(p,now)<=pixels[4].length+radiusOf(p));
 for(const p of active){
  const x=position(p,now),radius=radiusOf(p);
  for(const s of [p.limb,4]){
   if(s===4&&x>=0)continue;
   const center=s===4?r.junction[p.limb]-x:x;
   pixels[s]=pixels[s].map((v,i)=>add(v,scale(blue,(.5+.5*p.charge)*(1-Math.abs(i-center)/radius))));
  }
 }
 const flares=r.flares.slice();
 // Predict only collisions from already-known pulses between authoritative updates.
 if(!j.sections[4].manual)for(let i=0;i<active.length;i++)for(let k=i+1;k<active.length;k++){
  const a=active[i],b=active[k];if(a.limb===b.limb||(a.collided&(1<<b.id)))continue;
  const wait=Math.max(0,a.start*1000/r.speed-elapsed(r.clock,a.at),b.start*1000/r.speed-elapsed(r.clock,b.at))+.01;
  if(elapsed(now,r.clock)<wait)continue;
  const at=(r.clock+Math.ceil(wait))>>>0,ca=r.junction[a.limb]-position(a,at),cb=r.junction[b.limb]-position(b,at);
  if(ca>=0&&cb>=0&&ca<pixels[4].length&&cb<pixels[4].length&&Math.abs(ca-cb)<=4)flares.push({at,center:(ca+cb)/2});
 }
 for(const f of flares){const age=elapsed(now,f.at);if(age>=r.collisionMs)continue;
  pixels[4]=pixels[4].map((v,i)=>{const mix=(1-age/r.collisionMs)*(1-Math.abs(i-f.center)/6);return mix<=0?v:v.map((x,k)=>Math.trunc(x*(1-mix)+[160,135,5][k]*mix));});
 }
 j.sections.forEach((s,k)=>{if(!s.manual)return;
  const paint=new Map((s.paint||[]).map(p=>[p[0],p]));
  pixels[k]=pixels[k].map((v,i)=>{
   let c=rgb(s.color),factor=1;
   if(s.pattern===3){const p=paint.get(i);c=p?rgb(p[1]):[0,0,0];factor=p?(p[3]?1-elapsed(now,p[2])/r.fadeMs:1):0;}
   else if(s.pattern===1)factor=.5+.5*Math.sin((now%s.speed)/s.speed*2*Math.PI);
   else if(s.pattern===2){let pos=Math.floor(now*pixels[k].length/s.speed)%pixels[k].length;if(s.reverse)pos=pixels[k].length-1-pos;factor=Math.abs(i-pos)<3?1:0;}
   return scale(scale(c,factor),s.brightness/255).map(v=>Math.min(cap,v));
  });
 });
 const shot=j.effect,age=elapsed(now,r.effectAt),duration=shot===1?700:shot===2?2200:2400;
 if(shot&&age<duration)pixels.forEach((strip,s)=>{for(let i=0;i<strip.length;i++){
  if(shot===1){const flash=age<110?1-age/110:age>=260&&age<400?.8*(1-(age-260)/140):0;strip[i]=add(strip[i],scale([160,160,160],flash));}
  else if(shot===2){const route=s<4?i/strip.length*.55:s===4?.55+i/strip.length*.45:.82;strip[i]=add(strip[i],scale([80,160,160],1-Math.abs(route-age/duration*1.25)/.12));}
  else {let c=[0,0,0];if(s===5&&age>=900){const beat=(age-900)%650,envelope=age>=2200?0:beat<140?1-beat/140:beat>=180&&beat<320?.55*(1-(beat-180)/140):0;c=[Math.trunc(160*envelope),0,0];}else if(s!==5&&age>=1900)c=scale(blue,.1*(age-1900)/500);strip[i]=c;}
 }});
 return pixels;
}
// LOCAL_RENDER_END

const $=id=>document.getElementById(id),names=['Left arm','Right arm','Left leg','Right leg','Spine','Heart'];
let state=null,selected=0,csrf='',canControl=false,beatTimer=null,drag=null,queue=[],sending=false,recentLog='';
let renderReceived=0,renderLead=0,lastLogRead=0,dotSignature='',dotRows=[],visualRequest=0,visualApplied=0,localPaint=new Map();
const localNow=()=>typeof performance!=='undefined'?performance.now():Date.now();
function pointOnStrip(k,i,count){const line=document.querySelector(`[data-section="${k}"]`),fraction=count<2?0:i/(count-1);return line.getPointAtLength(line.getTotalLength()*(k<4?1-fraction:fraction));}
function buildDots(j){
 if(j.render){const spine=document.querySelector('[data-section="4"]'),total=spine.getTotalLength();
  for(let k=0;k<4;k++){const link=spine.getPointAtLength(total*j.render.junction[k]/Math.max(1,stripLength(j.sections[4])-1));
   const starts=['65,225 110,155','335,225 290,155','130,465 155,350','270,465 245,350'];
   document.querySelector(`[data-section="${k}"]`).setAttribute?.('points',starts[k]+` ${link.x},${link.y}`);
  }
 }
 const signature=j.sections.map(stripLength).join(',')+JSON.stringify(j.render?.junction||[]);if(signature===dotSignature&&dotRows.length)return;
 dotSignature=signature;dotRows=[];$('pixels').replaceChildren();
 j.sections.forEach((s,k)=>{const row=[];for(let i=0;i<stripLength(s);i++){
  const p=pointOnStrip(k,i,stripLength(s)),dot=document.createElementNS('http://www.w3.org/2000/svg','circle');
  dot.setAttribute('class','pixel');dot.setAttribute('cx',p.x);dot.setAttribute('cy',p.y);dot.setAttribute('r',k===5?3:2.8);dot.setAttribute('fill','#000000');
  row.push(dot);$('pixels').append(dot);
 }dotRows.push(row);});
}
function paintVisual(){if(!state)return;
 const age=Math.max(0,localNow()-renderReceived),clock=state.render?((state.render.clock+renderLead+Math.min(age,1500))>>>0):0;
 const colors=renderLocal(state,clock);
 colors.forEach((strip,s)=>strip.forEach((c,i)=>dotRows[s]?.[i]?.setAttribute('fill',`rgb(${c.join(',')})`)));
 for(const [key,p] of localPaint){if(localNow()-p.at>600)localPaint.delete(key);else dotRows[p.s]?.[p.i]?.setAttribute('fill',p.color);}
 $('visualSync').textContent=age>1500?'Updates paused — showing last known motion':'Live';
}
function animateVisual(){if(!document.hidden)paintVisual();if(typeof requestAnimationFrame==='function')requestAnimationFrame(animateVisual);}
if(typeof requestAnimationFrame==='function')requestAnimationFrame(animateVisual);
names.forEach((n,i)=>$('section').add(new Option(n,i)));
function message(s){$('message').textContent=s;}
function sensorMonitor(j){
 const hex=n=>n>=0?'0x'+n.toString(16).padStart(2,'0'):'not found';
 const lines=(j.buses||[]).map((b,i)=>`I2C ${i+1}: SDA ${b.sda} / SCL ${b.scl} · ${b.ready?'running':'unavailable'} · mux ${hex(b.mux)}`);
 for(let i=0;i<2;i++){
  const s=j.sensors[i],mask=j.touch[i],pressed=Array.from({length:12},(_,e)=>e).filter(e=>mask&(1<<e));
  lines.push(`Touch ${i?'B':'A'}: ${s?(s.present?'online':s.status||'unavailable'):'unavailable'} · channel ${s?.channel??'?'} · electrodes ${s?.present?(pressed.length?pressed.join(', '):'none'):'—'}`);
  const raw=j.touchDebug?.[i];
  if(raw&&s?.present)lines.push('  Filtered readings: '+raw.filtered.map((v,e)=>`${e}:${v}`).join(' / ')+` · hardware mask ${hex(raw.hardwareMask??0)}`);
  if(raw&&s?.present)lines.push('  Baseline − filtered: '+raw.filtered.map((v,e)=>`${e}:${raw.baseline[e]-v}`).join(' / ')+` · out-of-range ${hex(raw.oor)}`);
 }
 for(let i=0;i<2;i++){
  const s=j.sensors[i+2],m=j.motion?.[i],fresh=s?.present&&s.age!==null&&s.age<1000;
  lines.push(`IMU ${i?'B':'A'}: ${s?(s.present?'online':s.status||'unavailable'):'unavailable'} · channel ${s?.channel>=0?s.channel:'discovering'} · address ${hex(s?.address??-1)} · steps ${j.steps[i]}`);
  if(fresh&&m)lines.push(`  Accel (g): X ${m.ax.toFixed(3)} / Y ${m.ay.toFixed(3)} / Z ${m.az.toFixed(3)} · magnitude ${m.magnitude.toFixed(3)} · last step ${m.lastStepAge===null?'none':m.lastStepAge+' ms ago'}`);
  else lines.push('  Waiting for fresh accelerometer readings');
  if(fresh&&m)lines.push(`  Rotation ${Number(m.rotationDegrees||0).toFixed(1)}° / 6 deg · gyro (°/s) ${Number(m.gx||0).toFixed(1)} / ${Number(m.gy||0).toFixed(1)} / ${Number(m.gz||0).toFixed(1)}`);
  if(fresh&&m?.calibrationRemaining)lines.push(`  Calibrating: stand still for ${m.calibrationRemaining} more samples`);
 }
 if(!j.simulation)lines.push('A/B identify sensor slots; match the detected channels to your physical left/right placement.');
 if(j.heartRaw){const h=j.heartRaw;lines.push(`Heart: ${j.sensors[4]?.present?'online':'unavailable'} · finger ${h.finger?'detected':'not detected'} · IR ${h.ir} · red ${h.red} · signal ${h.signal.toFixed(1)} · beats accepted ${h.accepted} / rejected ${h.rejected}`);}
 $('sensorMonitor').textContent=lines.join('\n');
}
async function api(path,data){let r=await fetch(path,{method:data?'POST':'GET',headers:data?{'Content-Type':'application/x-www-form-urlencoded','X-CSRF':csrf}:{},body:data?new URLSearchParams(data):undefined,cache:'no-store'});let j=await r.json();if(!r.ok)throw Error(j.error||'Request failed');return j;}
async function action(data){try{await api('/api/command',data);await refresh();}catch(e){message(e.message);}}
async function endpoint(path,data={}){try{await api(path,data);await refresh();}catch(e){message(e.message);}}
function select(s){selected=s;$('section').value=s;$('sectionTitle').textContent=names[s];if(!state)return;let m=state.sections[s];$('color').value=m.color;$('bright').value=m.brightness;$('speed').value=m.speed;$('direction').value=+m.reverse;if(m.pattern<3)$('pattern').value=m.pattern;}
async function refresh(){try{
 let access=await api('/api/access');csrf=access.csrf||'';$('visitor').hidden=!access.open;
 if(!access.authorized){$('main').hidden=true;$('gate').hidden=false;state=null;localPaint.clear();visualApplied=++visualRequest;dotRows=[];dotSignature='';$('pixels').replaceChildren();$('visualSync').textContent='';$('status').textContent='';$('sensorMonitor').textContent='';$('diagnosticLog').textContent='';$('logInfo').textContent='';recentLog='';queue=[];if(beatTimer){clearInterval(beatTimer);beatTimer=null;}return;}
 const requestId=++visualRequest,sentAt=localNow();let j=await api('/api/state');if(requestId<visualApplied)return;visualApplied=requestId;renderReceived=localNow();renderLead=Math.min(250,(renderReceived-sentAt)/2);state=j;$('main').hidden=false;$('gate').hidden=true;canControl=j.mine;
 $('ownership').textContent=j.mine?'You control the costume':j.controller?'Another controller is active':'Control is available';$('claim').textContent=j.owner?'Take control':'Claim control';
 document.querySelectorAll('[data-control]').forEach(b=>b.disabled=!canControl);$('ownerPanel').hidden=!j.owner;$('simulation').hidden=!j.simulation;
 $('mode').textContent=(j.simulation?'Simulated sensors':'Physical sensors')+' · '+(j.leds?'LED outputs enabled; check status below':'LED outputs disabled');
 $('idleGlow').checked=!!j.idleGlow;
 sensorMonitor(j);
 $('effectStatus').textContent=['No effect active','Lightning','Energy wave','Heart restart'][j.effect||0];
 $('status').textContent=`Heart: ${j.heart.mode}, ${j.heart.bpm||'—'} BPM\nAccess: ${j.open?'open':'locked'} · controller lease: 30 seconds\nDropped pulses: ${j.dropped}\n`+j.sections.map(s=>`${s.name}: ${s.manual?'manual':'sensors'} · output ${s.tx} · frames ${s.frames} · ${s.error}`).join('\n')+'\nSensors:\n'+j.sensors.map(s=>`${s.name}: ${s.present?'present':'unavailable'} · age ${s.age===null?'—':s.age+' ms'} · faults ${s.faults}`).join('\n')+`\nTouches: ${j.touch.join(', ')} · Steps: ${j.steps.join(', ')}`;
 $('status').textContent+='\nCollision flares: '+(j.collisions||0);
 buildDots(j);paintVisual();
 if(localNow()-lastLogRead<1000&&lastLogRead)return;lastLogRead=localNow();
 let logs=await api('/api/logs');recentLog=(logs.entries||[]).map(e=>`[${e.sequence} +${e.ms} ms] ${e.message}`).join('\n');
 $('diagnosticLog').textContent=recentLog||'Waiting for diagnostic events';$('logInfo').textContent=`Latest 64 events · ${logs.overwritten||0} older events rolled off · USB serial can record the full test`;
 }catch(e){message(e.message);}}
$('downloadLog').onclick=()=>{if(!recentLog)return;const url=URL.createObjectURL(new Blob([recentLog+'\n'],{type:'text/plain'})),link=document.createElementNS('http://www.w3.org/1999/xhtml','a');link.href=url;link.download='costume-diagnostics.log';link.click();URL.revokeObjectURL(url);};
$('login').onsubmit=async e=>{e.preventDefault();try{await api('/api/login',{password:$('password').value});$('password').value='';message('');await refresh();}catch(e){message(e.message);}};
$('visitor').onclick=async()=>{try{await api('/api/visitor',{});await refresh();}catch(e){message(e.message);}};
$('claim').onclick=async()=>{try{await api('/api/claim',{});await refresh();}catch(e){message(e.message);}};
$('logout').onclick=()=>endpoint('/api/logout');
$('open').onclick=()=>endpoint('/api/open',{value:1});$('lock').onclick=()=>endpoint('/api/open',{value:0});
$('section').onchange=()=>select(+$('section').value);
$('idleGlow').onchange=()=>action({op:'idleGlow',enabled:$('idleGlow').checked?1:0});
$('apply').onclick=()=>action({op:'manual',s:selected,color:$('color').value.slice(1),brightness:$('bright').value,pattern:$('pattern').value,speed:$('speed').value,reverse:$('direction').value});
$('all').onclick=()=>action({op:'all'});$('return').onclick=()=>action({op:'sensors',s:selected});$('clear').onclick=()=>action({op:'clear',s:selected});
[['lightning',1],['energyWave',2],['heartRestart',3],['stopEffect',0]].forEach(([id,effect])=>$(id).onclick=()=>action({op:'oneshot',effect}));
$('calibrate').onclick=()=>action({op:'calibrate',left:$('leftSensitivity').value,right:$('rightSensitivity').value});
$('beat').onclick=()=>action({op:'beat'});$('leftStep').onclick=()=>action({op:'step',s:0});$('rightStep').onclick=()=>action({op:'step',s:1});
$('bothSteps').onclick=()=>action({op:'bothSteps'});
for(let arm=0;arm<2;arm++){let b=$(arm?'rightTouch':'leftTouch');const send=mask=>action({op:'touch',s:arm,mask});b.onpointerdown=e=>{if(!canControl)return;e.preventDefault();b.setPointerCapture(e.pointerId);send(1);};b.onpointerup=b.onpointercancel=()=>{if(canControl)send(0);};}
$('autoBeat').onclick=()=>{if(!beatTimer){action({op:'beat'});beatTimer=setInterval(()=>action({op:'beat'}),1000);}};
$('loss').onclick=()=>{clearInterval(beatTimer);beatTimer=null;action({op:'loss'});};
function locate(e){let p=$('body').createSVGPoint();p.x=e.clientX;p.y=e.clientY;p=p.matrixTransform($('body').getScreenCTM().inverse());let best=null;state.sections.forEach((s,k)=>{let line=document.querySelector(`[data-section="${k}"]`),len=line.getTotalLength();for(let i=0;i<stripLength(s);i++){let q=pointOnStrip(k,i,stripLength(s)),d=Math.hypot(q.x-p.x,q.y-p.y);if(d<25&&(!best||d<best.d))best={s:k,i,d};}});return best;}
async function drain(){if(sending)return;sending=true;while(queue.length){let d=queue.shift();try{await api('/api/command',d);}catch(e){queue=[];message(e.message);}}sending=false;}
function draw(e){if(!state)return;let hit=locate(e);if(!hit)return;if($('brushMode').value==='select'){select(hit.s);return;}if(!canControl)return;
 let from=drag&&drag.s===hit.s?drag.i:hit.i;
 queue.push({op:'brush',s:hit.s,from,to:hit.i,width:$('width').value,color:$('color').value.slice(1),fade:+($('brushMode').value==='fade')});
 if(queue.length>32)queue.shift();drag=hit;selected=hit.s;$('section').value=hit.s;$('sectionTitle').textContent=names[hit.s];
 // Immediate local feedback until the next commanded-output snapshot.
 let dots=$('pixels').children,offset=state.sections.slice(0,hit.s).reduce((n,s)=>n+stripLength(s),0),width=+$('width').value;
 for(let i=Math.min(from,hit.i)-Math.floor(width/2);i<=Math.max(from,hit.i)+Math.floor(width/2);i++)if(i>=0&&i<stripLength(state.sections[hit.s])){localPaint.set(`${hit.s}:${i}`,{s:hit.s,i,color:$('color').value,at:localNow()});dots[offset+i].setAttribute('fill',$('color').value);}
 drain();}
$('body').onpointerdown=e=>{if(!state)return;drag=null;$('body').setPointerCapture(e.pointerId);draw(e);};
$('body').onpointermove=e=>{if($('body').hasPointerCapture(e.pointerId))draw(e);};
$('body').onpointerup=$('body').onpointercancel=e=>{drag=null;if($('body').hasPointerCapture(e.pointerId))$('body').releasePointerCapture(e.pointerId);};
let refreshing=false,lastRenew=0;setInterval(async()=>{if(refreshing||document.hidden)return;refreshing=true;try{if(canControl&&!document.hidden&&localNow()-lastRenew>5000){await api('/api/renew',{});lastRenew=localNow();}await refresh();}catch(e){message(e.message);}finally{refreshing=false;}},200);refresh();
</script></html>)HTML";
