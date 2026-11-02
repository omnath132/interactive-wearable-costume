// Tests the actual embedded page script, without third-party dependencies.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../costume_system/Page.h'),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes=new Map(),requests=[],intervals=[],downloads=[],animationFrames=[];let clientTime=1000;
function element(){return {value:'',textContent:'',hidden:false,children:[],click(){this.clicked=true;},setAttribute(k,v){this[k]=v;},add(x){this.children.push(x);},append(x){this.children.push(x);},replaceChildren(){this.children=[];},setPointerCapture(){this.captured=true;},hasPointerCapture(){return this.captured;},releasePointerCapture(){this.captured=false;}};}
function $(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id);}
for(const [id,value] of Object.entries({color:'#4696ff',bright:'120',speed:'1200',pattern:'0',direction:'0',width:'10',brushMode:'select',leftSensitivity:'0.18',rightSensitivity:'0.18'}))$(id).value=value;
const lines=Array.from({length:6},()=>({getTotalLength:()=>100,getPointAtLength:n=>({x:n,y:0})}));
let authorized=false,open=false,mine=true,owner=true,commands=[];
const snapshot={owner:true,mine:true,controller:true,open:false,simulation:true,leds:false,heart:{mode:'waiting',bpm:0},dropped:0,touch:[0,0],steps:[0,0],sensors:[],sections:Array.from({length:6},(_,i)=>({name:'Section '+i,manual:false,color:'#4696ff',brightness:120,pattern:0,speed:1200,reverse:false,tx:'disabled',frames:0,error:'ESP_OK',length:20,paint:[]}))};
snapshot.render={clock:1000,boot:1,effectAt:0,cap:160,chargeMs:2500,collisionMs:320,fadeMs:2500,speed:35,junction:[16,16,0,0],regions:Array.from({length:2},()=>Array.from({length:12},()=>[2,5])),touchAt:Array.from({length:2},()=>Array(12).fill(0)),touchValid:[true,true],heart:{flashAt:0,flashed:false,nextBeat:0,period:0},pulses:[{id:0,limb:0,at:1000,start:14,charge:0,collided:0}],flares:[]};
const document={hidden:false,getElementById:$,querySelectorAll:()=>[],querySelector:s=>lines[+s.match(/\d+/)[0]],createElementNS:element};
$('body').createSVGPoint=()=>({x:0,y:0,matrixTransform(){return this;}});$('body').getScreenCTM=()=>({inverse:()=>({})});
const context=vm.createContext({document,Option:function(n,i){this.text=n;this.value=i;},Blob,URL:{createObjectURL(blob){downloads.push(blob);return 'blob:test';},revokeObjectURL(){}},URLSearchParams,console,performance:{now:()=>clientTime},requestAnimationFrame(fn){animationFrames.push(fn);},setInterval(fn){intervals.push(fn);return intervals.length;},clearInterval(){},fetch:async(path,opts={})=>{
 const data=Object.fromEntries(new URLSearchParams(opts.body));requests.push({path,opts,data});
 let result={};if(path==='/api/access')result={authorized,open,csrf:authorized?'session-token':'entry'};
 if(path==='/api/login'||path==='/api/visitor')authorized=true;
 if(path==='/api/logout')authorized=false;
 if(path==='/api/state')result={...snapshot,mine,owner,open};
 if(path==='/api/logs')result={overwritten:2,entries:[{sequence:3,ms:1234,message:'TOUCH_DOWN source=physical arm=left electrode=2'}]};
 if(path==='/api/command')commands.push(data);
 return {ok:true,json:async()=>result};
}});
vm.runInContext(script,context);
const flush=async()=>{for(let i=0;i<8;i++)await new Promise(r=>setImmediate(r));};
(async()=>{
 await flush();assert.equal($('main').hidden,true);assert.equal($('gate').hidden,false);assert(!requests.some(r=>r.path==='/api/state'||r.path==='/api/logs'));
 $('password').value='owner-secret';await $('login').onsubmit({preventDefault(){}});await flush();
 assert.equal($('main').hidden,false);assert.equal($('gate').hidden,true);assert.equal($('password').value,'');assert.equal($('pixels').children.length,120);
 const firstDot=$('pixels').children[0],beforeMotion=$('pixels').children[14].fill;
 clientTime+=100;vm.runInContext('paintVisual()',context);
 assert.notEqual($('pixels').children[14].fill,beforeMotion);assert.equal($('visualSync').textContent,'Live');
 clientTime+=2000;vm.runInContext('paintVisual()',context);const frozen=$('pixels').children[0].fill;
 clientTime+=1000;vm.runInContext('paintVisual()',context);assert.equal($('pixels').children[0].fill,frozen);assert.match($('visualSync').textContent,/paused/);
 clientTime=1000;await intervals[0]();await flush();assert.equal($('pixels').children[0],firstDot); // Reuse SVG nodes.
 assert.match($('diagnosticLog').textContent,/electrode=2/);assert.match($('logInfo').textContent,/2 older events/);
 $('downloadLog').onclick();assert.match(await downloads[0].text(),/\[3 \+1234 ms\] TOUCH_DOWN/);
 Object.assign(snapshot,{simulation:false,touch:[5,0],steps:[7,2],buses:[{sda:21,scl:22,ready:true,mux:112},{sda:26,scl:25,ready:true,mux:112}],motion:[{ax:0.1,ay:0.2,az:1,magnitude:1.025,lastStepAge:150,calibrationRemaining:0},{ax:0,ay:0,az:1,magnitude:1,lastStepAge:null,calibrationRemaining:20}],touchDebug:[{oor:0,filtered:[380],baseline:[400]}],heartRaw:{ir:40000,red:30000,finger:true,signal:12.5,accepted:5,rejected:1},sensors:[{present:true,channel:6,age:10},{present:true,channel:7,age:10},{present:true,channel:2,address:104,age:10},{present:true,channel:7,address:105,age:10},{present:true,channel:2,address:87,age:10}]});
 await intervals[0]();await flush();assert.equal($('simulation').hidden,true);assert.match($('mode').textContent,/Physical sensors/);
 assert.match($('sensorMonitor').textContent,/electrodes 0, 2/);assert.match($('sensorMonitor').textContent,/steps 7/);assert.match($('sensorMonitor').textContent,/0x69/);assert.match($('sensorMonitor').textContent,/Calibrating: stand still for 20/);
 assert.match($('sensorMonitor').textContent,/Baseline − filtered: 0:20/);assert.match($('sensorMonitor').textContent,/IR 40000/);assert.match($('sensorMonitor').textContent,/accepted 5 \/ rejected 1/);
 snapshot.simulation=true;await intervals[0]();await flush();
 assert.equal(requests.find(r=>r.path==='/api/login').opts.headers['X-CSRF'],'entry');
 $('section').value='5';$('section').onchange();$('apply').onclick();await flush();assert.equal(commands.at(-1).op,'manual');assert.equal(commands.at(-1).s,'5');
 $('all').onclick();await flush();assert.equal(commands.at(-1).op,'all');
 for(const [id,effect]of [['lightning','1'],['energyWave','2'],['heartRestart','3'],['stopEffect','0']]){
  $(id).onclick();await flush();assert.equal(commands.at(-1).op,'oneshot');assert.equal(commands.at(-1).effect,effect);
 }
 $('bothSteps').onclick();await flush();assert.equal(commands.at(-1).op,'bothSteps');
 $('leftTouch').onpointerdown({preventDefault(){},pointerId:1});await flush();assert.equal(commands.at(-1).mask,'1');
 $('leftTouch').onpointerup();await flush();assert.equal(commands.at(-1).mask,'0');
 $('idleGlow').checked=true;await $('idleGlow').onchange();await flush();assert.equal(commands.at(-1).op,'idleGlow');assert.equal(commands.at(-1).enabled,'1');
 $('idleGlow').checked=false;await $('idleGlow').onchange();await flush();assert.equal(commands.at(-1).enabled,'0');
 $('brushMode').value='paint';$('body').onpointerdown({pointerId:3,clientX:20,clientY:0});await flush();
 $('body').onpointermove({pointerId:3,clientX:70,clientY:0});await flush();
 let brush=commands.at(-1);assert.equal(brush.op,'brush');assert.equal(brush.width,'10');assert(+brush.to<+brush.from);assert.equal(brush.fade,'0');
 $('body').onpointerup({pointerId:3});
 assert.equal(requests.filter(r=>r.path==='/api/command').at(-1).opts.headers['X-CSRF'],'session-token');
 authorized=false;await intervals[0]();await flush();assert.equal($('main').hidden,true);assert.equal($('pixels').children.length,0);assert.equal($('status').textContent,'');
 assert.equal($('diagnosticLog').textContent,'');
 assert.equal($('sensorMonitor').textContent,'');
 console.log('Costume page checks passed: locked view, sign-in, overrides, touch release, interpolated painting, revocation');
})().catch(e=>{console.error(e);process.exitCode=1;});
