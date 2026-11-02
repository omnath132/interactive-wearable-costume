#pragma once
const char MAPPER_PAGE[] PROGMEM=R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Costume LED mapper</title>
<style>body{font:17px system-ui;background:#101724;color:#e9edf5;max-width:760px;margin:24px auto;padding:16px}section{background:#1c283b;padding:20px;border-radius:16px;margin:16px 0}label{display:block;margin:12px 0}input,select,button{font:inherit;padding:10px;border-radius:8px;border:1px solid #60718b;max-width:100%;box-sizing:border-box}input[type=number]{width:110px}button{cursor:pointer;background:#b6f3ce;color:#12231c;margin:4px}#off{background:#ffb4b4}small{display:block;color:#bfcede}article{border-top:1px solid #60718b;padding:12px 0}#status{position:sticky;top:0;background:#101724;padding:12px;white-space:pre-wrap}</style>
<h1>Costume LED mapper</h1><p>LED 1 is nearest the data input. Test one strip at a time. Saved lengths and named ranges survive restarts.</p>
<p id="status" role="status">Connecting…</p><button id="off">All LEDs off</button>
<section><h2>Strip setup</h2><label>Output <select id="strip"></select></label><label>Strip length <input id="length" type="number" min="1" max="256"></label><button id="saveLength">Save length</button><small>Maximum 256 LEDs per output. Defaults are starting guesses; measure each strip.</small></section>
<section><h2>Find an LED or range</h2><label>First LED <input id="first" type="number" min="1" value="1"></label><label>Last LED <input id="last" type="number" min="1" value="1"></label><label>Color <input id="color" type="color" value="#00ff00"></label><label>Brightness <input id="brightness" type="number" min="1" max="64" value="16"> / 255</label><button id="test">Light range</button><button id="previous">← Previous LED</button><button id="next">Next LED →</button><button id="whole">Light whole strip</button><small>Previous/next lights one LED. Changing fields takes effect when you press Light range.</small></section>
<section><h2>Record a part</h2><label>Name <input id="name" maxlength="39" placeholder="Touch A electrode 0"></label><small>Letters, numbers, spaces, underscore and hyphen. Reusing a name updates its saved range. Up to 32 records.</small><button id="record">Save selected range</button><button id="download">Download mapping JSON</button><div id="records"></div></section>
<script>
const $=id=>document.getElementById(id);let state;
async function action(path,data){const r=await fetch('/api/'+path,{method:'POST',body:new URLSearchParams(data)});const text=await r.text();if(!r.ok)throw Error(text);$('status').textContent=text;}
function selected(){return {strip:$('strip').value,first:$('first').value,last:$('last').value};}
async function test(){await action('test',{...selected(),brightness:$('brightness').value,color:$('color').value.slice(1)});}
function selection(){const s=state.strips[+$('strip').value];$('length').value=s.length;$('first').max=$('last').max=s.length;$('first').value=$('last').value=1;}
async function load(){
 const r=await fetch('/api/state');if(!r.ok)throw Error('Could not read mappings');state=await r.json();
 const old=$('strip').value;$('strip').replaceChildren();
 state.strips.forEach((s,i)=>{const o=document.createElement('option');o.value=i;o.textContent=`${s.name} — GPIO ${s.pin}${s.ready?'':' — OUTPUT ERROR'}`;$('strip').append(o);});
 $('strip').value=old||'0';selection();$('records').replaceChildren();
 state.records.forEach(m=>{const row=document.createElement('article');const label=document.createElement('div');label.textContent=`${m.name}: ${state.strips[m.strip].name}, LEDs ${m.first}–${m.last}`;row.append(label);
 const use=document.createElement('button');use.textContent='Load range';use.onclick=()=>{$('strip').value=m.strip;selection();$('first').value=m.first;$('last').value=m.last;$('name').value=m.name;};row.append(use);
 const del=document.createElement('button');del.textContent='Delete';del.onclick=()=>run(async()=>{await action('delete',{slot:m.slot});await load();});row.append(del);$('records').append(row);});
}
async function run(fn){try{await fn();}catch(e){$('status').textContent=e.message;}}
$('strip').onchange=selection;
$('off').onclick=()=>run(()=>action('off',{}));
$('test').onclick=()=>run(test);
$('saveLength').onclick=()=>run(async()=>{await action('length',{strip:$('strip').value,length:$('length').value});await load();});
for(const [id,step] of [['previous',-1],['next',1]])$(id).onclick=()=>run(async()=>{const n=Math.max(1,Math.min(state.strips[+$('strip').value].length,+$('first').value+step));$('first').value=$('last').value=n;await test();});
$('whole').onclick=()=>run(async()=>{$('first').value=1;$('last').value=state.strips[+$('strip').value].length;await test();});
$('record').onclick=()=>run(async()=>{await action('record',{...selected(),name:$('name').value});await load();});
$('download').onclick=()=>run(async()=>{const r=await fetch('/api/state');if(!r.ok)throw Error('Download failed');const data=await r.json();const blob=new Blob([JSON.stringify({...data,numbering:'1-based inclusive, from data input'},null,2)],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='costume-led-mapping.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
run(async()=>{await load();$('status').textContent='Ready. Outputs start off.';});
</script></html>)HTML";
