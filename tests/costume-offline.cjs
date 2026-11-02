// Optional browser regression check for the interactive in-chat preview.
const {chromium}=require('playwright');
const fs=require('node:fs'),assert=require('node:assert/strict');
const file=process.argv[2];if(!file)throw Error('Supply the generated preview path');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.COSTUME_BROWSER||undefined});
 try{
  const page=await browser.newPage({viewport:{width:1000,height:900}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.clock.install();await page.setContent(fs.readFileSync(file,'utf8'));
  await page.locator('#main').waitFor({state:'visible'});
  const colors=()=>page.locator('#pixels circle').evaluateAll(nodes=>nodes.map(n=>n.getAttribute('fill')));
  await page.locator('#lightning').click();assert.equal(await page.locator('#effectStatus').innerText(),'Lightning');
  assert(parseInt((await colors())[0].slice(1,3),16)>100);await page.clock.runFor(800);assert.equal(await page.locator('#effectStatus').innerText(),'No effect active');
  await page.locator('#energyWave').click();assert.equal(await page.locator('#effectStatus').innerText(),'Energy wave');
  await page.locator('#stopEffect').click();assert.equal(await page.locator('#effectStatus').innerText(),'No effect active');
  await page.locator('#heartRestart').click();assert((await colors()).every(c=>c==='#000000'));
  await page.clock.runFor(1000);assert((await colors()).slice(240).some(c=>c!=='#000000'));
  await page.locator('#all').click();assert.equal(await page.locator('#effectStatus').innerText(),'No effect active');
  const arm=page.locator('#leftTouch');await arm.scrollIntoViewIfNeeded();const box=await arm.boundingBox();
  await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();
  await page.clock.runFor(100);let early=(await colors())[0];await page.clock.runFor(2600);let charged=(await colors())[0];
  assert(parseInt(charged.slice(5,7),16)>parseInt(early.slice(5,7),16));
  await page.mouse.up();await page.clock.runFor(100);
  await page.locator('#bothSteps').click();await page.clock.runFor(1500);
  await page.locator('#stopEffect').click(); // Reconcile after advancing the virtual clock.
  assert.match(await page.locator('#status').innerText(),/Collision flares: [1-9]/);
  await page.setViewportSize({width:390,height:844});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  assert.deepEqual(errors,[]);console.log('Offline preview passed: charging, collision flare, all one-shots, cancellation, mobile layout');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
