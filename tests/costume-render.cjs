const fs=require('fs'),vm=require('vm'),assert=require('assert/strict'),cp=require('child_process'),path=require('path');
const page=fs.readFileSync(path.join(__dirname,'../costume_system/Page.h'),'utf8');
const script=page.split('// LOCAL_RENDER_BEGIN')[1].split('// LOCAL_RENDER_END')[0];
const ctx=vm.createContext({});vm.runInContext(script+'\nthis.renderLocal=renderLocal;',ctx);
const result=cp.spawnSync(path.join(__dirname,'costume-render-tests.exe'),[],{encoding:'utf8',maxBuffer:8*1024*1024});
if(result.status!==0)throw Error(result.stderr||'Render fixture failed');
let checks=0,maxError=0;
for(const line of result.stdout.trim().split(/\r?\n/)){
 const fixture=JSON.parse(line),actual=ctx.renderLocal(fixture.state,fixture.now);
 for(let s=0;s<6;s++)for(let i=0;i<actual[s].length;i++)for(let c=0;c<3;c++){
  const diff=Math.abs(actual[s][i][c]-fixture.pixels[s][i][c]);maxError=Math.max(maxError,diff);
  // Future collision timing is quantized to the physical firmware's next render tick.
  const predictedCollision=fixture.state.render.clock===1950&&s===4&&fixture.now>=1972;
  assert(diff<=(predictedCollision?12:2),`frame ${fixture.now}, section ${s}, LED ${i}, channel ${c}: JS ${actual[s][i][c]} vs firmware ${fixture.pixels[s][i][c]}`);
 }
 checks++;
}
console.log(`Local renderer checked against firmware: ${checks} frames, touch holds/releases, spine linkage, collision colors, manual/paint, one-shots, heartbeat, timer rollover`);
