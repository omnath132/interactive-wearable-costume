// Optional visual smoke check: requires Playwright and its Chromium browser.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const html=fs.readFileSync(path.join(__dirname,'../costume_system/Page.h'),'utf8').split('R"HTML(')[1].split(')HTML"')[0];
const sections=['Left arm','Right arm','Left leg','Right leg','Spine','Heart'].map((name,s)=>({name,manual:false,color:'#4696ff',brightness:120,pattern:0,speed:1200,reverse:false,tx:'disabled',frames:0,error:'ESP_OK',pixels:Array.from({length:[40,40,50,50,60,18][s]},()=>s===5?'#400000':'#2466a0')}));
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.COSTUME_BROWSER||undefined});try{
 const page=await browser.newPage({viewport:{width:390,height:844}});let authorized=false;const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.route('http://costume.test/**',route=>{
  const url=new URL(route.request().url());if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
  let result={};if(url.pathname==='/api/access')result={open:false,authorized,csrf:authorized?'test-session':'entry'};
  if(url.pathname==='/api/login')authorized=true;
  if(url.pathname==='/api/state')result={owner:true,mine:true,controller:true,open:false,simulation:false,leds:false,heart:{mode:'predicted',bpm:60},dropped:0,sections,sensors:[{present:true,channel:6,age:0},{present:true,channel:7,age:0},{present:true,channel:1,address:104,age:0},{present:true,channel:5,address:105,age:0}],buses:[{sda:21,scl:22,ready:true,mux:112},{sda:26,scl:25,ready:true,mux:112}],motion:[{ax:0,ay:0,az:1,magnitude:1,lastStepAge:50,calibrationRemaining:0},{ax:0,ay:0,az:1,magnitude:1,lastStepAge:null,calibrationRemaining:0}],touch:[5,0],steps:[2,3]};
  return route.fulfill({contentType:'application/json',body:JSON.stringify(result)});
 });
 await page.goto('http://costume.test/');assert(await page.locator('#gate').isVisible());assert(!await page.locator('#main').isVisible());
 await page.locator('#password').fill('example-password');await page.locator('#login button').click();await page.locator('#main').waitFor({state:'visible'});
 await page.locator('#pixels circle').first().waitFor();assert.equal(await page.locator('#pixels circle').count(),258);
  assert(!await page.locator('#simulation').isVisible());assert.match(await page.locator('#sensorMonitor').innerText(),/electrodes 0, 2/);assert.match(await page.locator('#sensorMonitor').innerText(),/steps 2/);
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.screenshot({path:path.join(__dirname,'costume-preview.png'),fullPage:true});
 await page.locator('#section').selectOption('5');assert.equal(await page.locator('#sectionTitle').innerText(),'Heart');
 await page.locator('#apply').click();assert.deepEqual(errors,[]);console.log('Costume browser smoke check passed (mobile layout and page interactions)');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
