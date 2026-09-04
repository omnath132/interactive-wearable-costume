/*
 * WS2812 (NeoPixel) Wi-Fi control - data pin GPIO 23, up to 181 LEDs
 *
 * The ESP32 creates ITS OWN Wi-Fi network (no router needed) and hosts
 * the control webpage itself, with a captive portal:
 *
 *   1) Join the Wi-Fi network "ESP32-LEDs" from your phone/laptop
 *   2) The control page pops up automatically (captive portal),
 *      or browse to  http://192.168.4.1
 *
 * Libraries: Adafruit NeoPixel (Library Manager).
 *            WiFi / WebServer / DNSServer are built into the ESP32 core.
 *
 * Serial Monitor @ 115200 still works with the same commands. Type help.
 *
 * --- Basic commands ---
 *   color R G B / led I R G B / fill NAME / bright N
 *   on / off / clear / toggle / count N / status / help
 *
 * --- Patterns (built-in) ---
 *   pulse R G B [MS]     brightness swell up & down, whole strip
 *   wipe R G B [MS]      fill one LED at a time, end to end
 *   chase R G B [MS]     a lit dot runs along the strip
 *   rainbow              moving rainbow
 *   stop                 stop any running pattern (leaves last frame)
 *   speed MS / dir fwd|rev|flip
 *
 * --- Mini scripting language ---
 *   script loop0 wipe 0 0 255 40 ; wait 300 ; pulse 255 0 0 800 ; fill off
 *   'loop0' = repeat forever, 'loop3' = 3 times, omit = run once.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     4
#define MAX_LEDS    181
#define COLOR_ORDER NEO_GRB     // swap to NEO_RGB if colors look wrong

// ---- Wi-Fi access point settings ----
const char* AP_SSID = "ESP32-LEDs";
const char* AP_PASS = "simonkaplinsky";        // "" = open network. Or set 8+ chars for WPA2.

Adafruit_NeoPixel strip(MAX_LEDS, LED_PIN, COLOR_ORDER + NEO_KHZ800);
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// ---- strip state ----
int      activeLeds = MAX_LEDS;
uint32_t lastColor  = 0xFFFFFF;
bool     ledsOn     = false;

// ---- pattern engine state ----
enum Mode { M_STATIC, M_RAINBOW, M_PULSE, M_WIPE, M_CHASE };
Mode     mode = M_STATIC;
uint8_t  pr, pg, pb;         // pattern color
uint16_t pStepMs = 40;       // per-step / cycle timing (lower = faster)
unsigned long lastTick = 0;
int      pIndex = 0;         // generic step index
int      pDir = 1;           // pulse fade direction (internal)
int      travel = 1;         // travel direction: +1 = forward, -1 = reverse
uint16_t rainbowHue = 0;

// ---- script engine state ----
String   scriptSteps[16];
int      scriptCount = 0;
int      scriptPos = 0;
long     scriptLoops = 0;     // 0 = infinite, >0 = remaining, -1 = none
bool     scriptRunning = false;
unsigned long scriptWaitUntil = 0;

// ---------- helpers ----------
// When a web request is being handled, replies are also captured here
// and returned as the HTTP response body.
String* webReply = nullptr;

void reply(const String& s) {
  Serial.println(s);
  if (webReply) { *webReply += s; *webReply += '\n'; }
}

void showAll(uint32_t c) {
  for (int i = 0; i < activeLeds; i++) strip.setPixelColor(i, c);
  for (int i = activeLeds; i < MAX_LEDS; i++) strip.setPixelColor(i, 0);
  strip.show();
  if (c != 0) { lastColor = c; ledsOn = true; } else ledsOn = false;
}

uint32_t namedColor(const String& n) {
  if (n == "red")     return strip.Color(255, 0, 0);
  if (n == "green")   return strip.Color(0, 255, 0);
  if (n == "blue")    return strip.Color(0, 0, 255);
  if (n == "white")   return strip.Color(255, 255, 255);
  if (n == "yellow")  return strip.Color(255, 255, 0);
  if (n == "cyan")    return strip.Color(0, 255, 255);
  if (n == "magenta") return strip.Color(255, 0, 255);
  if (n == "orange")  return strip.Color(255, 100, 0);
  if (n == "purple")  return strip.Color(150, 0, 255);
  if (n == "off")     return 0;
  return 0xFFFFFFFF;
}

void startPattern(Mode m, int r, int g, int b, int ms) {
  mode = m; pr = r; pg = g; pb = b;
  if (ms > 0) pStepMs = ms;
  pDir = 1; lastTick = 0;
  // start at the correct end for wipe/chase based on travel direction
  pIndex = (travel >= 0) ? 0 : (activeLeds - 1);
  if (m == M_STATIC) showAll(strip.Color(r, g, b));
}

// ---------- pattern stepping (non-blocking) ----------
void runPatterns() {
  unsigned long now = millis();
  if (mode == M_STATIC) return;

  uint16_t interval = (mode == M_PULSE) ? max<uint16_t>(pStepMs / 64, 5) : pStepMs;
  if (now - lastTick < interval) return;
  lastTick = now;

  if (mode == M_RAINBOW) {
    for (int i = 0; i < activeLeds; i++) {
      uint16_t hue = rainbowHue + (i * 65536L / activeLeds);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
    }
    strip.show();
    rainbowHue += 256; ledsOn = true;

  } else if (mode == M_WIPE) {
    strip.setPixelColor(pIndex, strip.Color(pr, pg, pb));
    strip.show();
    ledsOn = true;
    pIndex += travel;
    if (pIndex >= activeLeds || pIndex < 0) { mode = M_STATIC; }  // one pass then hold

  } else if (mode == M_CHASE) {
    for (int i = 0; i < activeLeds; i++) strip.setPixelColor(i, 0);
    strip.setPixelColor(pIndex, strip.Color(pr, pg, pb));
    strip.show();
    ledsOn = true;
    pIndex += travel;
    if (pIndex >= activeLeds) pIndex = 0;
    if (pIndex < 0)           pIndex = activeLeds - 1;

  } else if (mode == M_PULSE) {
    // pIndex sweeps 0..255..0 as a brightness scaler
    float s = pIndex / 255.0;
    for (int i = 0; i < activeLeds; i++)
      strip.setPixelColor(i, strip.Color(pr * s, pg * s, pb * s));
    strip.show();
    ledsOn = true;
    pIndex += pDir * 8;
    if (pIndex >= 255) { pIndex = 255; pDir = -1; }
    if (pIndex <= 0)   { pIndex = 0;   pDir = 1;  }
  }
}

// ---------- script engine ----------
void stopScript() { scriptRunning = false; scriptCount = 0; scriptPos = 0; }

void handleCommand(String line);  // fwd decl

void startScript(String body) {
  stopScript();
  body.trim();
  scriptLoops = -1;
  // optional leading loopN
  if (body.startsWith("loop")) {
    int sp = body.indexOf(' ');
    String tok = (sp == -1) ? body : body.substring(0, sp);
    scriptLoops = tok.substring(4).toInt();   // loop0 -> 0 (infinite)
    body = (sp == -1) ? "" : body.substring(sp + 1);
  }
  // split on ';'
  scriptCount = 0;
  while (body.length() && scriptCount < 16) {
    int semi = body.indexOf(';');
    String step = (semi == -1) ? body : body.substring(0, semi);
    step.trim();
    if (step.length()) scriptSteps[scriptCount++] = step;
    if (semi == -1) break;
    body = body.substring(semi + 1);
  }
  scriptPos = 0; scriptRunning = scriptCount > 0; scriptWaitUntil = 0;
  reply(scriptRunning ? "OK: script started" : "ERR: empty script");
}

void runScript() {
  if (!scriptRunning) return;
  if (scriptWaitUntil && millis() < scriptWaitUntil) return;
  scriptWaitUntil = 0;

  // if a wipe pattern is still running, let it finish before next step
  if (mode == M_WIPE) return;

  if (scriptPos >= scriptCount) {
    if (scriptLoops == 0) { scriptPos = 0; }        // infinite
    else if (scriptLoops > 1) { scriptLoops--; scriptPos = 0; }
    else { stopScript(); reply("OK: script done"); return; }
  }

  String step = scriptSteps[scriptPos++];
  if (step.startsWith("wait")) {
    int ms = step.substring(4).toInt();
    scriptWaitUntil = millis() + (ms > 0 ? ms : 200);
  } else {
    handleCommand(step);   // reuse the normal command handler
  }
}

void printHelp() {
  reply(F("cmds: color R G B | led I R G B | fill NAME | bright N"));
  reply(F("      on | off | clear | toggle | count N | status | help"));
  reply(F("patterns: pulse|wipe|chase R G B [MS] | rainbow | stop"));
  reply(F("speed/dir: speed MS (5-2000, lower=faster) | dir fwd|rev|flip"));
  reply(F("script: script [loopN] step ; step ; ...  (step=wipe/pulse/"));
  reply(F("        chase/fill/color/bright/wait MS)"));
}

void printStatus() {
  String s = "brightness=" + String(strip.getBrightness());
  s += " active_leds=" + String(activeLeds);
  const char* mn[] = {"static","rainbow","pulse","wipe","chase"};
  s += " mode=" + String(mn[mode]);
  s += " speed=" + String(pStepMs) + "ms";
  s += " dir=" + String(travel > 0 ? "fwd" : "rev");
  s += " script=" + String(scriptRunning ? "on" : "off");
  s += " power=" + String(ledsOn ? "on" : "off");
  s += " clients=" + String(WiFi.softAPgetStationNum());
  reply(s);
}

// ---------- main command handler ----------
void handleCommand(String line) {
  line.trim();
  if (!line.length()) return;

  int sp = line.indexOf(' ');
  String cmd = (sp == -1) ? line : line.substring(0, sp);
  String rest = (sp == -1) ? "" : line.substring(sp + 1);
  cmd.toLowerCase();

  if (cmd == "help")            { printHelp(); }
  else if (cmd == "status")     { printStatus(); }
  else if (cmd == "off" || cmd == "clear") { mode = M_STATIC; showAll(0); reply("OK: off"); }
  else if (cmd == "on")         { mode = M_STATIC; showAll(lastColor); reply("OK: on"); }
  else if (cmd == "toggle")     { mode = M_STATIC; showAll(ledsOn ? 0 : lastColor); reply(ledsOn ? "OK: off" : "OK: on"); }
  else if (cmd == "stop")       { mode = M_STATIC; stopScript(); reply("OK: stopped"); }
  else if (cmd == "rainbow")    { startPattern(M_RAINBOW,0,0,0,0); reply("OK: rainbow"); }

  else if (cmd == "bright") {
    int n = constrain(rest.toInt(), 0, 255);
    strip.setBrightness(n); strip.show();
    reply("OK: brightness=" + String(n));
  }
  else if (cmd == "count") {
    activeLeds = constrain(rest.toInt(), 1, MAX_LEDS);
    reply("OK: active_leds=" + String(activeLeds));
  }
  else if (cmd == "fill") {
    rest.trim(); rest.toLowerCase();
    uint32_t c = namedColor(rest);
    if (c == 0xFFFFFFFF) { reply("ERR: unknown color name"); return; }
    mode = M_STATIC; showAll(c); reply("OK: fill " + rest);
  }
  else if (cmd == "color") {
    int r,g,b;
    if (sscanf(rest.c_str(), "%d %d %d", &r,&g,&b) == 3) {
      mode = M_STATIC;
      showAll(strip.Color(constrain(r,0,255),constrain(g,0,255),constrain(b,0,255)));
      reply("OK: color");
    } else reply("ERR: usage 'color R G B'");
  }
  else if (cmd == "led") {
    int i,r,g,b;
    if (sscanf(rest.c_str(), "%d %d %d %d", &i,&r,&g,&b) == 4 && i>=0 && i<MAX_LEDS) {
      mode = M_STATIC;
      strip.setPixelColor(i, strip.Color(constrain(r,0,255),constrain(g,0,255),constrain(b,0,255)));
      strip.show(); ledsOn = true; reply("OK: led " + String(i));
    } else reply("ERR: usage 'led I R G B'");
  }
  else if (cmd == "speed") {
    // lower ms = faster; accept 5..2000
    int ms = constrain(rest.toInt(), 5, 2000);
    pStepMs = ms;
    reply("OK: speed=" + String(ms) + "ms/step");
  }
  else if (cmd == "dir") {
    rest.trim(); rest.toLowerCase();
    if (rest == "fwd" || rest == "forward" || rest == "1")      travel = 1;
    else if (rest == "rev" || rest == "reverse" || rest == "-1") travel = -1;
    else if (rest == "flip" || rest == "toggle")                 travel = -travel;
    else { reply("ERR: usage 'dir fwd|rev|flip'"); return; }
    // re-seat index for the new direction if a travelling pattern is live
    if (mode == M_WIPE || mode == M_CHASE)
      pIndex = constrain(pIndex, 0, activeLeds - 1);
    reply(String("OK: dir=") + (travel > 0 ? "fwd" : "rev"));
  }
  else if (cmd == "pulse" || cmd == "wipe" || cmd == "chase") {
    int r,g,b,ms=0;
    int n = sscanf(rest.c_str(), "%d %d %d %d", &r,&g,&b,&ms);
    if (n >= 3) {
      Mode m = (cmd=="pulse")?M_PULSE:(cmd=="wipe")?M_WIPE:M_CHASE;
      startPattern(m, constrain(r,0,255),constrain(g,0,255),constrain(b,0,255), ms);
      reply("OK: " + cmd);
    } else reply("ERR: usage '" + cmd + " R G B [MS]'");
  }
  else if (cmd == "script") { startScript(rest); }
  else { reply("ERR: unknown '" + cmd + "' (type help)"); }
}

// ---------- the control webpage (served from flash) ----------
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>ESP32 LEDs</title>
<style>
  :root { --bg:#101318; --card:#1a1f27; --edge:#2a3140; --text:#e8ecf2; --dim:#8b95a5; --accent:#4c8dff; }
  * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
  body { margin:0; font-family:-apple-system,'Segoe UI',Roboto,sans-serif; background:var(--bg); color:var(--text); }
  .wrap { max-width:440px; margin:0 auto; padding:14px 14px 40px; }
  h1 { font-size:19px; margin:8px 2px 2px; }
  #stat { font-size:12px; color:var(--dim); margin:0 2px 12px; min-height:15px; word-break:break-all; }
  .card { background:var(--card); border:1px solid var(--edge); border-radius:14px; padding:14px; margin-bottom:12px; }
  .card h2 { font-size:12px; text-transform:uppercase; letter-spacing:.08em; color:var(--dim); margin:0 0 10px; font-weight:600; }
  .row { display:flex; gap:8px; flex-wrap:wrap; }
  button { flex:1; min-width:70px; padding:12px 8px; border-radius:10px; border:1px solid var(--edge);
           background:#232a35; color:var(--text); font-size:14px; font-weight:500; cursor:pointer; }
  button:active { background:var(--accent); border-color:var(--accent); }
  .swatches { display:grid; grid-template-columns:repeat(5,1fr); gap:8px; }
  .sw { height:42px; border-radius:10px; border:1px solid var(--edge); cursor:pointer; }
  .sliderrow { display:flex; align-items:center; gap:10px; margin-top:4px; }
  .sliderrow label { width:70px; font-size:13px; color:var(--dim); }
  .sliderrow output { width:38px; text-align:right; font-size:13px; }
  input[type=range] { flex:1; accent-color:var(--accent); }
  input[type=color] { width:100%; height:46px; border:1px solid var(--edge); border-radius:10px;
                      background:#232a35; padding:4px; margin-top:8px; }
  .cmdrow { display:flex; gap:8px; }
  .cmdrow input { flex:1; padding:11px 12px; border-radius:10px; border:1px solid var(--edge);
                  background:#232a35; color:var(--text); font-size:14px; }
  .cmdrow button { flex:0 0 64px; }
  #log { font-family:ui-monospace,Menlo,Consolas,monospace; font-size:12px; color:#9fd48f;
         background:#12161c; border:1px solid var(--edge); border-radius:10px; padding:10px;
         margin-top:10px; min-height:56px; max-height:150px; overflow-y:auto; white-space:pre-wrap; }
</style>
</head>
<body>
<div class="wrap">
  <h1>ESP32 LED Strip</h1>
  <div id="stat">connecting&hellip;</div>

  <div class="card">
    <h2>Power</h2>
    <div class="row">
      <button onclick="cmd('on')">On</button>
      <button onclick="cmd('off')">Off</button>
      <button onclick="cmd('toggle')">Toggle</button>
      <button onclick="cmd('stop')">Stop</button>
    </div>
  </div>

  <div class="card">
    <h2>Color</h2>
    <div class="swatches">
      <div class="sw" style="background:#f00"    onclick="cmd('fill red')"></div>
      <div class="sw" style="background:#0f0"    onclick="cmd('fill green')"></div>
      <div class="sw" style="background:#00f"    onclick="cmd('fill blue')"></div>
      <div class="sw" style="background:#ff0"    onclick="cmd('fill yellow')"></div>
      <div class="sw" style="background:#0ff"    onclick="cmd('fill cyan')"></div>
      <div class="sw" style="background:#f0f"    onclick="cmd('fill magenta')"></div>
      <div class="sw" style="background:#ff6400" onclick="cmd('fill orange')"></div>
      <div class="sw" style="background:#9600ff" onclick="cmd('fill purple')"></div>
      <div class="sw" style="background:#fff"    onclick="cmd('fill white')"></div>
      <div class="sw" style="background:#000"    onclick="cmd('fill off')"></div>
    </div>
    <input type="color" id="pick" value="#4c8dff">
  </div>

  <div class="card">
    <h2>Patterns</h2>
    <div class="row">
      <button onclick="pat('pulse')">Pulse</button>
      <button onclick="pat('wipe')">Wipe</button>
      <button onclick="pat('chase')">Chase</button>
      <button onclick="cmd('rainbow')">Rainbow</button>
    </div>
    <div class="row" style="margin-top:8px">
      <button onclick="cmd('dir fwd')">Dir &#8594;</button>
      <button onclick="cmd('dir rev')">Dir &#8592;</button>
      <button onclick="cmd('dir flip')">Flip</button>
    </div>
  </div>

  <div class="card">
    <h2>Levels</h2>
    <div class="sliderrow">
      <label>Brightness</label>
      <input type="range" id="bri" min="0" max="255" value="60">
      <output id="briv">60</output>
    </div>
    <div class="sliderrow">
      <label>Speed</label>
      <input type="range" id="spd" min="5" max="400" value="40">
      <output id="spdv">40</output>
    </div>
  </div>

  <div class="card">
    <h2>Command</h2>
    <div class="cmdrow">
      <input id="ci" placeholder="e.g. chase 255 0 0 30, or: help" autocomplete="off">
      <button onclick="sendBox()">Send</button>
    </div>
    <div id="log"></div>
  </div>
</div>

<script>
var log = document.getElementById('log');
var stat = document.getElementById('stat');

function addLog(s) {
  log.textContent += s;
  if (log.textContent.length > 4000) log.textContent = log.textContent.slice(-4000);
  log.scrollTop = log.scrollHeight;
}

function cmd(c, quiet) {
  return fetch('/cmd?c=' + encodeURIComponent(c))
    .then(function(r){ return r.text(); })
    .then(function(t){ if (!quiet) addLog('> ' + c + '\n' + t); return t; })
    .catch(function(){ if (!quiet) addLog('> ' + c + '\n(no response)\n'); });
}

function hexRgb(h) {
  return parseInt(h.substr(1,2),16) + ' ' + parseInt(h.substr(3,2),16) + ' ' + parseInt(h.substr(5,2),16);
}

function pat(name) { cmd(name + ' ' + hexRgb(document.getElementById('pick').value)); }

document.getElementById('pick').addEventListener('input', debounce(function(e){
  cmd('color ' + hexRgb(e.target.value), true);
}, 120));

function bindSlider(id, out, make) {
  var el = document.getElementById(id), ov = document.getElementById(out);
  el.addEventListener('input', function(){ ov.textContent = el.value; });
  el.addEventListener('input', debounce(function(){ cmd(make(el.value), true); }, 150));
}
bindSlider('bri', 'briv', function(v){ return 'bright ' + v; });
bindSlider('spd', 'spdv', function(v){ return 'speed ' + v; });

function debounce(fn, ms) {
  var t; return function(){ var a=arguments, s=this;
    clearTimeout(t); t=setTimeout(function(){ fn.apply(s,a); }, ms); };
}

var ci = document.getElementById('ci');
function sendBox(){ if (ci.value.trim()) { cmd(ci.value.trim()); ci.value=''; } }
ci.addEventListener('keydown', function(e){ if (e.key==='Enter') sendBox(); });

function poll() {
  cmd('status', true).then(function(t){
    if (t) stat.textContent = t.trim();
  });
}
poll();
setInterval(poll, 3000);
</script>
</body>
</html>
)HTML";

// ---------- web server & captive portal ----------
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCmd() {
  String out;
  webReply = &out;
  handleCommand(server.arg("c"));
  webReply = nullptr;
  server.send(200, "text/plain", out.length() ? out : "OK");
}

// Any unknown URL (including the OS "am I online?" probes like
// /generate_204 and /hotspot-detect.html) gets redirected to our page.
// That redirect is what makes phones pop up the captive-portal sheet.
void handleNotFound() {
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void setupWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if (strlen(AP_PASS) >= 8) WiFi.softAP(AP_SSID, AP_PASS);
  else                      WiFi.softAP(AP_SSID);           // open network

  dnsServer.start(DNS_PORT, "*", apIP);   // answer every DNS query with our IP

  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(60);
  strip.show();
  setupWifi();
  delay(200);
  reply(String("WS2812 Wi-Fi control ready. Join Wi-Fi '") + AP_SSID +
        "' then open http://" + apIP.toString() + "/  (or wait for the captive portal). Type help.");
}

void loop() {
  // 1) serial input (line-buffered)
  static String sbuf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (sbuf.length()) { handleCommand(sbuf); sbuf=""; } }
    else sbuf += c;
  }

  // 2) network
  dnsServer.processNextRequest();
  server.handleClient();

  // 3) run animations & scripts
  runPatterns();
  runScript();
}
