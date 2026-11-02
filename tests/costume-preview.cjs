// Local preview of the real embedded page, backed by simulated costume state.
// Run: node tests/costume-preview.cjs — open http://127.0.0.1:8765 (password: demo).
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const html=fs.readFileSync(path.join(__dirname,'../costume_system/Page.h'),'utf8').split('R"HTML(')[1].split(')HTML"')[0];
const lengths=[40,40,50,50,60,18],names=['Left arm','Right arm','Left leg','Right leg','Spine','Heart'];
const sections=names.map((name,s)=>({name,manual:false,color:'#4696ff',brightness:120,pattern:0,speed:1200,reverse:false,tx:'simulated',frames:0,error:'ESP_OK',pixels:Array(lengths[s]).fill('#000000')}));
const paint=lengths.map(n=>Array(n).fill(null)),touch=[0,0],steps=[0,0],valid=[true,true],pulses=[],flares=[],heldAt=[Array(12).fill(0),Array(12).fill(0)];
const sessions=new Map();let open=false,controller=null,lease=0,flash=-100000,lastReal=-100000,anchor=null,period=0,nextBeat=0,intervals=[],dropped=0,collisions=0,pulseID=0,shot=0,shotAt=0;
const start=Date.now(),now=()=>Date.now()-start;
const diagnosticEntries=[];let diagnosticSequence=1,diagnosticOverwritten=0;
function record(t,message){diagnosticEntries.push({sequence:diagnosticSequence++,ms:t,message});if(diagnosticEntries.length>64){diagnosticEntries.shift();diagnosticOverwritten++;}}
record(0,'BOOT mode=simulated; no physical sensors are connected to this preview');
const clamp=v=>Math.max(0,Math.min(160,Math.round(v))),rgb=s=>s.match(/[a-f\d]{2}/gi).map(n=>parseInt(n,16)),hex=c=>'#'+c.map(v=>clamp(v).toString(16).padStart(2,'0')).join('');
function beat(t){if(anchor!==null){const dt=t-anchor;if(dt<333)return;if(dt>1714){anchor=t;return;}if(period&&(dt<period*.65||dt>period*1.35)){anchor=t;return;}intervals.push(dt);if(intervals.length>4)intervals.shift();if(intervals.length===4)period=intervals.reduce((a,b)=>a+b)/4;}anchor=t;lastReal=t;if(t-flash>140)flash=t;if(period)nextBeat=t+period;}
const charge=(arm,e,t)=>Math.max(0,Math.min(1,(t-heldAt[arm][e])/2500));
function launch(s,pos,t,energy=0){if(sections[s].manual)return;if(pulses.length>=24){dropped++;return;}pulses.push({s,pos,t,energy,id:++pulseID,hit:new Set()});}
function touchEvent(s,mask,t){
 if(valid[s])for(let e=0;e<12;e++)if((touch[s]&~mask)&(1<<e)){record(t,`TOUCH_UP source=simulated arm=${s?'right':'left'} electrode=${e} held_ms=${t-heldAt[s][e]}`);launch(s,Math.floor(e*lengths[s]/12),t,charge(s,e,t));}
 for(let e=0;e<12;e++)if((mask&(1<<e))&&(!valid[s]||!(touch[s]&(1<<e)))){heldAt[s][e]=t;record(t,`TOUCH_DOWN source=simulated arm=${s?'right':'left'} electrode=${e} mask=0x${mask.toString(16).padStart(3,'0')}`);}
 touch[s]=mask;valid[s]=true;
}
function stepEvent(s,t){steps[s]++;launch(s+2,0,t,.35);record(t,`STEP source=simulated leg=${s?'right':'left'}`);}
function back(s){sections[s].manual=false;paint[s].fill(null);for(let i=pulses.length-1;i>=0;i--)if(s===4||pulses[i].s===s)pulses.splice(i,1);if(s===4)flares.length=0;}
function oneShot(effect,t){shot=effect;shotAt=t;record(t,`ONE_SHOT effect=${effect}`);}
function render(){const t=now();if(period&&t>=nextBeat){nextBeat=t+period;if(t-flash>140)flash=t;}
 const breath=.025+.075*(.5-.5*Math.cos((t%4500)/4500*2*Math.PI));
 const pixels=lengths.map((n,s)=>Array.from({length:n},()=>s===5?[0,0,0]:[45,120,160].map(v=>Math.floor(v*breath))));
 const age=t-flash,heart=age<140?10+150*(1-age/140):age>=180&&age<320?10+85*(1-(age-180)/140):10;
 pixels[5].forEach(c=>c[0]=heart);
 const add=(s,i,f)=>{if(i>=0&&i<lengths[s]&&f>0)pixels[s][i]=pixels[s][i].map((v,k)=>clamp(v+[45,120,160][k]*Math.min(1,f)));};
 for(let arm=0;arm<2;arm++)for(let e=0;e<12;e++)if(touch[arm]&(1<<e))for(let i=Math.floor(e*lengths[arm]/12);i<Math.floor((e+1)*lengths[arm]/12);i++)add(arm,i,.2+.8*charge(arm,e,t));
 for(let p=pulses.length-1;p>=0;p--){const q=pulses[p],d=q.pos+(t-q.t)*.035,j=q.s<2?40:0,radius=5+7*q.energy,gain=.5+.5*q.energy;if(d>lengths[q.s]+lengths[4]-j+radius){pulses.splice(p,1);continue;}for(let i=0;i<lengths[q.s];i++)add(q.s,i,gain*(1-Math.abs(i-d)/radius));if(d>=lengths[q.s])for(let i=0;i<lengths[4];i++)add(4,i,gain*(1-Math.abs(i-(d-lengths[q.s]+j))/radius));}
 for(let i=flares.length-1;i>=0;i--)if(t-flares[i].t>=320)flares.splice(i,1);
 if(!sections[4].manual)for(let i=0;i<pulses.length;i++)for(let j=i+1;j<pulses.length;j++){
  const a=pulses[i],b=pulses[j],da=a.pos+(t-a.t)*.035,db=b.pos+(t-b.t)*.035,ca=da-lengths[a.s]+(a.s<2?40:0),cb=db-lengths[b.s]+(b.s<2?40:0);
  if(a.s!==b.s&&!a.hit.has(b.id)&&da>=lengths[a.s]&&db>=lengths[b.s]&&ca>=0&&ca<lengths[4]&&cb>=0&&cb<lengths[4]&&Math.abs(ca-cb)<=4){a.hit.add(b.id);collisions++;if(flares.length<8)flares.push({t,center:(ca+cb)/2});else dropped++;}
 }
 for(const f of flares)for(let i=0;i<lengths[4];i++){const gain=Math.max(0,(1-(t-f.t)/320)*(1-Math.abs(i-f.center)/6));pixels[4][i]=pixels[4][i].map(v=>clamp(v+160*gain));}
 sections.forEach((s,k)=>{if(s.manual)for(let i=0;i<lengths[k];i++){
  let c=[0,0,0],f=1;if(s.pattern===3){const p=paint[k][i];if(p){f=p.fade?Math.max(0,1-(t-p.t)/2500):1;c=p.color;}}
  else{c=rgb(s.color);if(s.pattern===1)f=.5+.5*Math.sin((t%s.speed)/s.speed*2*Math.PI);if(s.pattern===2){let pos=Math.floor(t*lengths[k]/s.speed)%lengths[k];if(s.reverse)pos=lengths[k]-1-pos;f=Math.abs(i-pos)<3?1:0;}}
  pixels[k][i]=c.map(v=>v*f*s.brightness/255);
 }});
 if(shot){const age=t-shotAt,duration=shot===1?700:shot===2?2200:2400;if(age>=duration)shot=0;else for(let s=0;s<6;s++)for(let i=0;i<lengths[s];i++){
  if(shot===1){const gain=age<110?1-age/110:age>=260&&age<400?.8*(1-(age-260)/140):0;pixels[s][i]=pixels[s][i].map(v=>clamp(v+160*gain));}
  else if(shot===2){const route=s<4?i/lengths[s]*.55:s===4?.55+i/lengths[s]*.45:.82,gain=Math.max(0,1-Math.abs(route-age/duration*1.25)/.12);pixels[s][i]=pixels[s][i].map((v,k)=>clamp(v+[80,160,160][k]*gain));}
  else {let c=[0,0,0];if(s===5&&age>=900){const b=(age-900)%650,gain=age>=2200?0:b<140?1-b/140:b>=180&&b<320?.55*(1-(b-180)/140):0;c=[10+150*gain,0,0];}else if(s!==5&&age>=1900)c=[45,120,160].map(v=>v*.1*(age-1900)/500);pixels[s][i]=c;}
 }}
 sections.forEach((s,k)=>{s.pixels=pixels[k].map(hex);s.frames++;});
}
const simulationTimer=setInterval(render,20);
const server=http.createServer(async(req,res)=>{
 res.setHeader('Cache-Control','no-store');const url=new URL(req.url,'http://127.0.0.1');
 const reply=(code,data)=>{res.writeHead(code,{'Content-Type':'application/json'});res.end(JSON.stringify(data));};
 const error=(code,error)=>reply(code,{error});
 if(url.pathname==='/'){res.writeHead(200,{'Content-Type':'text/html'});res.end(html);return;}
 let body='';for await(const chunk of req){body+=chunk;if(body.length>8192){error(413,'Request too large');return;}}
 const data=Object.fromEntries(new URLSearchParams(body)),token=(req.headers.cookie||'').split(';').map(s=>s.trim()).find(s=>s.startsWith('costume='))?.slice(8),session=sessions.get(token);
 const t=now();if(t-lease>=30000)controller=null;
 const authorized=!!session&&(open||session.owner);
 if(url.pathname==='/api/access'){reply(200,{open,authorized,csrf:session?token:'entry'});return;}
 if(req.method==='POST'){
  const entry=['/api/login','/api/visitor'].includes(url.pathname);
  if(req.headers['x-csrf']!==(entry?'entry':token)||(!entry&&!session)){error(403,'Reload the access page');return;}
 }
 if(url.pathname==='/api/login'||url.pathname==='/api/visitor'){
  const owner=url.pathname==='/api/login';if(owner?data.password!=='demo':!open){error(403,owner?'Preview password is demo':'Access is locked');return;}
  const id=crypto.randomBytes(16).toString('hex');sessions.set(id,{owner});res.setHeader('Set-Cookie',`costume=${id}; HttpOnly; SameSite=Strict; Path=/`);reply(200,{});return;
 }
 if(!authorized){error(403,'Access is locked');return;}
 if(url.pathname==='/api/logs'){reply(200,{overwritten:diagnosticOverwritten,entries:diagnosticEntries});return;}
 if(url.pathname==='/api/state'){
  render();reply(200,{owner:session.owner,mine:controller===token,controller:!!controller,open,simulation:true,leds:false,heart:{mode:t-lastReal<2500?'live':period?'predicted':'waiting',bpm:period?Math.round(60000/period):0},dropped,collisions,effect:shot,sections,sensors:['Left touch','Right touch','Left motion','Right motion','Heart'].map(name=>({name,present:false,age:null,faults:0})),touch,steps});return;
 }
 if(url.pathname==='/api/logout'){sessions.delete(token);if(controller===token)controller=null;reply(200,{});return;}
 if(url.pathname==='/api/open'){
  if(!session.owner){error(403,'Owner access required');return;}open=data.value==='1';if(!open)for(const[id,s]of sessions)if(!s.owner){sessions.delete(id);if(controller===id)controller=null;}reply(200,{});return;
 }
 if(url.pathname==='/api/claim'){if(controller&&controller!==token&&!session.owner){error(409,'Another controller is active');return;}controller=token;lease=t;reply(200,{});return;}
 if(controller!==token){error(409,'Claim control first');return;}lease=t;
 if(url.pathname==='/api/renew'){reply(200,{});return;}
 if(url.pathname!=='/api/command'){error(404,'Unknown endpoint');return;}
 const s=Number(data.s),op=data.op;
 if(op==='all'){oneShot(0,t);sections.forEach((_,i)=>back(i));}
 else if(op==='oneshot'&&Number.isInteger(Number(data.effect))&&Number(data.effect)>=0&&Number(data.effect)<=3)oneShot(Number(data.effect),t);
 else if(op==='beat')beat(t);
 else if(op==='loss'){anchor=null;touch.fill(0);valid.fill(false);}
 else if(op==='calibrate'){} // Simulated sensor calibration has no physical baseline.
 else if(op==='bothSteps'){stepEvent(0,t);stepEvent(1,t);}
 else if(op==='step'&&(s===0||s===1))stepEvent(s,t);
 else if(op==='touch'&&(s===0||s===1)){
  touchEvent(s,Number(data.mask)&4095,t);
 }else if(Number.isInteger(s)&&s>=0&&s<6){
  const section=sections[s];
  if(op==='sensors')back(s);
  else if(op==='clear'){paint[s].fill(null);section.manual=true;section.pattern=3;}
  else if(op==='manual'){Object.assign(section,{manual:true,color:'#'+data.color,brightness:Number(data.brightness),pattern:Number(data.pattern),speed:Math.max(100,Number(data.speed)),reverse:data.reverse==='1'});}
  else if(op==='brush'){
   section.manual=true;section.pattern=3;const from=Number(data.from),to=Number(data.to),width=Math.max(1,Math.min(96,Number(data.width))),color=rgb(data.color);
   for(let i=Math.min(from,to);i<=Math.max(from,to);i++)for(let k=i-Math.floor(width/2);k<i-Math.floor(width/2)+width;k++)if(k>=0&&k<lengths[s])paint[s][k]={color,t,fade:data.fade==='1'};
  }else{error(400,'Unknown command');return;}
 }else{error(400,'Invalid command');return;}
 reply(200,{});
});
server.listen(8765,'127.0.0.1',()=>console.log('Costume preview: http://127.0.0.1:8765 — password: demo'));
server.on('error',e=>{clearInterval(simulationTimer);console.error(e.message);process.exitCode=1;});
