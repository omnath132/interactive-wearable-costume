// Build an offline interactive preview from the real firmware page.
const fs=require('node:fs'),path=require('node:path');
const target=process.argv[2];if(!target)throw Error('Supply an absolute output path');
let html=fs.readFileSync(path.join(__dirname,'../costume_system/Page.h'),'utf8').split('R"HTML(')[1].split(')HTML"')[0];
const model=fs.readFileSync(path.join(__dirname,'costume-preview.cjs'),'utf8').split('const lengths=')[1].split('const server=http.createServer')[0];
const local=`const previewAPI=(()=>{const lengths=${model}
let authorized=true,mine=true,owner=true;
beat(0);beat(1000);beat(2000);beat(3000);beat(4000);anchor=null;lastReal=-100000;flash=-100000;nextBeat=now()+1000;
return async(path,data={})=>{
 const t=now(),op=data.op,s=Number(data.s);
 if(path==='/api/access')return {open,authorized,csrf:authorized?'preview':'entry'};
 if(path==='/api/login'){if(data.password!=='demo')throw Error('Preview password: demo');authorized=true;owner=true;return {};}
 if(path==='/api/visitor'){authorized=true;owner=false;mine=false;return {};}
 if(!authorized)throw Error('Sign in to continue');
 if(path==='/api/logs')return {overwritten:diagnosticOverwritten,entries:diagnosticEntries};
 if(path==='/api/logout'){authorized=false;mine=false;return {};}
 if(path==='/api/claim'){mine=true;return {};}
 if(path==='/api/open'){open=String(data.value)==='1';return {};}
 if(path==='/api/renew')return {};
 if(path==='/api/state'){render();return {owner,mine,controller:mine,open,simulation:true,leds:false,heart:{mode:t-lastReal<2500?'live':period?'predicted':'waiting',bpm:period?Math.round(60000/period):0},dropped,collisions,effect:shot,sections,sensors:['Left touch','Right touch','Left motion','Right motion','Heart'].map(name=>({name,present:false,age:null,faults:0})),touch,steps};}
 if(!mine)throw Error('Claim control first');
 if(op==='all'){oneShot(0,t);sections.forEach((_,i)=>back(i));}
 else if(op==='oneshot')oneShot(Number(data.effect),t);
 else if(op==='beat')beat(t);
 else if(op==='loss'){anchor=null;touch.fill(0);valid.fill(false);}
 else if(op==='bothSteps'){stepEvent(0,t);stepEvent(1,t);}
 else if(op==='step')stepEvent(s,t);
 else if(op==='touch')touchEvent(s,Number(data.mask)&4095,t);
 else if(op==='calibrate'){}
 else if(s>=0&&s<6){const section=sections[s];
 if(op==='sensors')back(s);
 else if(op==='clear'){paint[s].fill(null);section.manual=true;section.pattern=3;}
 else if(op==='manual')Object.assign(section,{manual:true,color:'#'+data.color,brightness:Number(data.brightness),pattern:Number(data.pattern),speed:Number(data.speed),reverse:String(data.reverse)==='1'});
 else if(op==='brush'){section.manual=true;section.pattern=3;const from=Number(data.from),to=Number(data.to),width=Number(data.width),color=rgb(data.color);for(let i=Math.min(from,to);i<=Math.max(from,to);i++)for(let k=i-Math.floor(width/2);k<i-Math.floor(width/2)+width;k++)if(k>=0&&k<lengths[s])paint[s][k]={color,t,fade:String(data.fade)==='1'};}}
 return {};
};})();`;
html=html.replace(/<!doctype html>/i,'').replace(/<html[^>]*>/,'').replace('</html>','').replace(/<meta[^>]*>/g,'').replace(/<title>.*?<\/title>/,'');
html=html.replace(':root{','#costume-preview{').replace('body{','#costume-preview{');
html=html.replace('<script>',`<script>(()=>{const root=window.document.getElementById('costume-preview');const document={getElementById:id=>root.querySelector('#'+id),querySelector:s=>root.querySelector(s),querySelectorAll:s=>root.querySelectorAll(s),createElementNS:(...args)=>window.document.createElementNS(...args),get hidden(){return window.document.hidden;}};${local}`);
html=html.replace(/async function api\(path,data\)\{[^\n]+\}/,'async function api(path,data){return previewAPI(path,data);}');
html=html.replace('</script>','})();</script>');
// Use a useful animated default and reconcile more frequently in this preview.
html=html.replace('},1000);refresh();','},100);refresh();');
html=html.replace('<h1>Costume</h1>','<h1>Costume</h1><p>Interactive preview · simulated lights · sign-in password: demo</p>');
fs.writeFileSync(target,'<div id="costume-preview">\n'+html+'\n</div>\n');
console.log(target);
