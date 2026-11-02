// Run with: node tests/wifi-controls.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../ws2812_wifi_ap/ws2812_wifi_ap.ino'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
const panels = [], requests = [];
function element(value = '') {
  return { value, textContent:'', dataset:{}, listeners:{},
    addEventListener(name, callback) { this.listeners[name] = callback; },
    checkValidity() { return true; }
  };
}
function panel() {
  const elements = new Map();
  const commands = ['on','off','toggle','stop','fill red','fill green','fill blue','fill white','rainbow','dir fwd','dir rev'].map(command => {
    const el = element(); el.dataset.command = command; return el;
  });
  const patterns = ['pulse','wipe','chase'].map(pattern => {
    const el = element(); el.dataset.pattern = pattern; return el;
  });
  return {
    commands, patterns,
    querySelector(selector) {
      if (!elements.has(selector)) elements.set(selector, element(selector === '.color' ? '#4c8dff' : ''));
      return elements.get(selector);
    },
    querySelectorAll(selector) { return selector === '[data-command]' ? commands : patterns; }
  };
}
const document = {
  getElementById(id) {
    return id === 'strip-template'
      ? {content:{firstElementChild:{cloneNode:panel}}}
      : {appendChild(node) { panels.push(node); }};
  }
};
vm.runInNewContext(script, {
  document, setTimeout() {},
  fetch: async url => {
    const parsed = new URL(url, 'http://board');
    const channel = parsed.searchParams.get('channel');
    requests.push({channel, command:parsed.searchParams.get('c')});
    return {ok:true, text:async () => `channel=${channel} brightness=60 speed=40 active_leds=181`};
  }
});
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  await flush();
  assert.equal(panels.length, 3);
  assert.deepEqual(requests.map(r => r.channel), ['1','2','3']);
  requests.length = 0;
  panels[0].querySelector('.color').value = '#ff0000';
  panels[1].querySelector('.color').value = '#00ff00';
  panels[2].querySelector('.color').value = '#0000ff';
  panels.forEach(p => p.patterns[2].listeners.click());
  await flush();
  assert.deepEqual(requests, [
    {channel:'1', command:'chase 255 0 0'},
    {channel:'2', command:'chase 0 255 0'},
    {channel:'3', command:'chase 0 0 255'}
  ]);
  requests.length = 0;
  const brightness = panels[1].querySelector('.brightness');
  brightness.value = '25'; brightness.listeners.change();
  panels[1].commands.find(el => el.dataset.command === 'off').listeners.click();
  await flush();
  assert.deepEqual(requests, [{channel:'2',command:'bright 25'}, {channel:'2',command:'off'}]);
  requests.length = 0;
  panels[2].querySelector('.command').value = 'script loop0 fill blue ; wait 500';
  panels[2].querySelector('.send').listeners.click();
  await flush();
  assert.deepEqual(requests, [{channel:'3',command:'script loop0 fill blue ; wait 500'}]);
  console.log('PASS: separate panel colors, channel routing, command ordering, and script targeting');
})().catch(error => { console.error(error); process.exitCode = 1; });
