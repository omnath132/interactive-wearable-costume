/*
 * WS2812 (NeoPixel) Wi-Fi control - 3 channels on GPIO 16, 17, 18, up to 181 LEDs each
 *
 * The ESP32 creates ITS OWN Wi-Fi network (no router needed) and hosts
 * the control webpage itself, with a captive portal:
 *
 *   1) Join the Wi-Fi network "ESP32-LEDs" from your phone/laptop
 *   2) The control page pops up automatically (captive portal),
 *      or browse to  http://192.168.4.1
 *
 * Target: ESP32-WROOM-32D / classic ESP32, Arduino-ESP32 3.x.
 * Libraries: Adafruit NeoPixel (pixel storage/color math only, no transmission).
 * Output: three persistent native ESP-IDF RMT transmitters.
 *            WiFi / WebServer / DNSServer are built into the ESP32 core.
 *
 * Serial Monitor @ 115200 still works with the same commands. Type help.
 *
 * Select a serial channel with: channel 1 / channel 2 / channel 3
 * The webpage has a separate control panel for each strip.
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
#include "IndependentRmtOutput.h"

constexpr int CHANNEL_COUNT = 3;
const uint8_t CHANNEL_PINS[CHANNEL_COUNT] = {16, 17, 18};
const char* FIRMWARE_VERSION = "3ch-rmt-2";
#define MAX_LEDS    181
#define COLOR_ORDER NEO_GRB     // swap to NEO_RGB if colors look wrong

// ---- Wi-Fi access point settings ----
const char* AP_SSID = "ESP32-LEDs";
const char* AP_PASS = "simonkaplinsky";        // "" = open network. Or set 8+ chars for WPA2.

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// ---------- helpers ----------
// When a web request is being handled, replies are also captured here
// and returned as the HTTP response body.
String* webReply = nullptr;

void reply(const String& s) {
  Serial.println(s);
  if (webReply) { *webReply += s; *webReply += '\n'; }
}

// Each channel owns its pixels, animation timing, and script.
class LedChannel {
public:
  Adafruit_NeoPixel strip;
  IndependentRmtOutput<MAX_LEDS> output;
  uint8_t number;
  uint8_t pin;

  LedChannel(uint8_t channelNumber, uint8_t dataPin)
    : strip(MAX_LEDS, -1, COLOR_ORDER + NEO_KHZ800),
      number(channelNumber), pin(dataPin) {}

  void showStrip() {
    output.markDirty();
  }

  void begin() {
    // NeoPixel is only an in-memory pixel buffer: never begin/show/setPin it.
    if (!output.begin(pin)) {
      reply("ERR: RMT init GPIO " + String(pin) + ": " + esp_err_to_name(output.error()));
    }
    strip.setBrightness(60);
    strip.clear();
    showStrip();
  }

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

void showAll(uint32_t c) {
  for (int i = 0; i < activeLeds; i++) strip.setPixelColor(i, c);
  for (int i = activeLeds; i < MAX_LEDS; i++) strip.setPixelColor(i, 0);
  showStrip();
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
    showStrip();
    rainbowHue += 256; ledsOn = true;

  } else if (mode == M_WIPE) {
    strip.setPixelColor(pIndex, strip.Color(pr, pg, pb));
    showStrip();
    ledsOn = true;
    pIndex += travel;
    if (pIndex >= activeLeds || pIndex < 0) { mode = M_STATIC; }  // one pass then hold

  } else if (mode == M_CHASE) {
    for (int i = 0; i < activeLeds; i++) strip.setPixelColor(i, 0);
    strip.setPixelColor(pIndex, strip.Color(pr, pg, pb));
    showStrip();
    ledsOn = true;
    pIndex += travel;
    if (pIndex >= activeLeds) pIndex = 0;
    if (pIndex < 0)           pIndex = activeLeds - 1;

  } else if (mode == M_PULSE) {
    // pIndex sweeps 0..255..0 as a brightness scaler
    float s = pIndex / 255.0;
    for (int i = 0; i < activeLeds; i++)
      strip.setPixelColor(i, strip.Color(pr * s, pg * s, pb * s));
    showStrip();
    ledsOn = true;
    pIndex += pDir * 8;
    if (pIndex >= 255) { pIndex = 255; pDir = -1; }
    if (pIndex <= 0)   { pIndex = 0;   pDir = 1;  }
  }
}

// ---------- script engine ----------
void stopScript() { scriptRunning = false; scriptCount = 0; scriptPos = 0; }


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
  reply(F("Serial selection: channel 1|2|3 (GPIO 16|17|18)"));
  reply(F("cmds: color R G B | led I R G B | fill NAME | bright N"));
  reply(F("      on | off | clear | toggle | count N | status | help"));
  reply(F("patterns: pulse|wipe|chase R G B [MS] | rainbow | stop"));
  reply(F("speed/dir: speed MS (5-2000, lower=faster) | dir fwd|rev|flip"));
  reply(F("script: script [loopN] step ; step ; ...  (step=wipe/pulse/"));
  reply(F("        chase/fill/color/bright/wait MS)"));
}

void printStatus() {
  String s = "channel=" + String(number) + " pin=" + String(pin);
  s += " fw=" + String(FIRMWARE_VERSION);
  s += " driver=dedicated-rmt tx=" + String(output.ready() ? "ready" : "error");
  s += " tx_error=" + String(esp_err_to_name(output.error()));
  s += " frames=" + String(output.frames());
  s += " brightness=" + String(strip.getBrightness());
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

// External commands take over this strip; script steps use handleCommand directly.
void handleManualCommand(String line) {
  line.trim();
  String cmd = line.substring(0, line.indexOf(' ') < 0 ? line.length() : line.indexOf(' '));
  cmd.toLowerCase();
  if (cmd == "on" || cmd == "off" || cmd == "clear" || cmd == "toggle" ||
      cmd == "fill" || cmd == "color" || cmd == "led" || cmd == "pulse" ||
      cmd == "wipe" || cmd == "chase" || cmd == "rainbow" || cmd == "stop") {
    stopScript();
  }
  handleCommand(line);
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
  else if (cmd == "toggle")     { mode = M_STATIC; showAll(ledsOn ? 0 : lastColor); reply(ledsOn ? "OK: on" : "OK: off"); }
  else if (cmd == "stop")       { mode = M_STATIC; stopScript(); reply("OK: stopped"); }
  else if (cmd == "rainbow")    { startPattern(M_RAINBOW,0,0,0,0); reply("OK: rainbow"); }

  else if (cmd == "bright") {
    int n = constrain(rest.toInt(), 0, 255);
    strip.setBrightness(n); showStrip();
    reply("OK: brightness=" + String(n));
  }
  else if (cmd == "count") {
    activeLeds = constrain(rest.toInt(), 1, MAX_LEDS);
    pIndex = constrain(pIndex, 0, activeLeds - 1);
    for (int i = activeLeds; i < MAX_LEDS; i++) strip.setPixelColor(i, 0);
    showStrip();
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
      showStrip(); ledsOn = true; reply("OK: led " + String(i));
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

};

LedChannel channels[CHANNEL_COUNT] = {
  {1, CHANNEL_PINS[0]},
  {2, CHANNEL_PINS[1]},
  {3, CHANNEL_PINS[2]}
};
int serialChannel = 0;

void handleSerialCommand(String line) {
  line.trim();
  if (line.startsWith("channel ")) {
    String value = line.substring(8);
    value.trim();
    if (value != "1" && value != "2" && value != "3") {
      reply("ERR: usage 'channel 1|2|3'");
      return;
    }
    serialChannel = value.toInt() - 1;
    channels[serialChannel].printStatus();
    return;
  }
  channels[serialChannel].handleManualCommand(line);
}

// ---------- the control webpage (served from flash) ----------
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 — Independent LED Strips</title>
<style>
* { box-sizing:border-box; }
body { margin:0; padding:18px; background:#101318; color:#e8ecf2; font-family:system-ui,sans-serif; }
main { max-width:1200px; margin:auto; }
h1 { font-size:24px; } h2 { margin:0 0 6px; font-size:20px; }
p, .status { color:#adb7c7; }
#strips { display:grid; grid-template-columns:repeat(auto-fit,minmax(290px,1fr)); gap:16px; }
.strip { padding:18px; border:1px solid #354155; border-radius:14px; background:#1a1f27; min-width:0; }
.status { font-size:12px; min-height:64px; overflow-wrap:anywhere; }
.row { display:flex; flex-wrap:wrap; gap:7px; margin:10px 0; }
button, input { font:inherit; border:1px solid #45536b; border-radius:8px; background:#232a35; color:inherit; padding:9px; min-width:0; }
button { cursor:pointer; flex:1; } button:active { background:#315a98; }
label { display:block; margin-top:15px; font-size:14px; }
input[type=range] { width:100%; padding:0; accent-color:#4c8dff; }
input[type=color] { width:100%; height:42px; margin-top:6px; }
input[type=number] { width:85px; }
.command { width:100%; }
pre { white-space:pre-wrap; overflow-wrap:anywhere; max-height:150px; overflow:auto; font-size:12px; color:#a9dc98; }
</style>
</head>
<body><main>
<h1>Independent LED Strips</h1>
<p>Each panel controls only its labeled GPIO pin. Patterns on other strips keep running.</p>
<div id="strips"></div>
<template id="strip-template">
  <section class="strip">
    <h2></h2><div class="status" aria-live="polite">Connecting…</div>
    <div class="row">
      <button data-command="on">On</button><button data-command="off">Off</button>
      <button data-command="toggle">Toggle</button><button data-command="stop">Stop</button>
    </div>
    <label>Color<input class="color" type="color" value="#4c8dff"></label>
    <div class="row">
      <button data-command="fill red">Red</button><button data-command="fill green">Green</button>
      <button data-command="fill blue">Blue</button><button data-command="fill white">White</button>
    </div>
    <div class="row">
      <button data-pattern="pulse">Pulse</button><button data-pattern="wipe">Wipe</button>
      <button data-pattern="chase">Chase</button><button data-command="rainbow">Rainbow</button>
    </div>
    <div class="row">
      <button data-command="dir fwd">Forward</button><button data-command="dir rev">Reverse</button>
    </div>
    <label>Brightness <output class="brightness-value">60</output>
      <input class="brightness" type="range" min="0" max="255" value="60">
    </label>
    <label>Speed (ms) <output class="speed-value">40</output>
      <input class="speed" type="range" min="5" max="2000" value="40">
    </label>
    <label>LED count <input class="count" type="number" min="1" max="181" value="181"></label>
    <label>Command or script<input class="command" placeholder="e.g. chase 255 0 0 30" autocomplete="off"></label>
    <div class="row"><button class="send">Send</button></div>
    <pre class="log" aria-live="polite"></pre>
  </section>
</template>
<script>
function rgb(hex) {
  return [1,3,5].map(function(i){ return parseInt(hex.slice(i,i+2),16); }).join(' ');
}

function mountStrip(channel, pin) {
  var panel = document.getElementById('strip-template').content.firstElementChild.cloneNode(true);
  document.getElementById('strips').appendChild(panel);
  panel.querySelector('h2').textContent = 'Strip ' + channel + ' · GPIO ' + pin;
  var status = panel.querySelector('.status'), log = panel.querySelector('.log');
  // Each panel captures an immutable channel and has its own ordered request queue.
  var queue = Promise.resolve();
  function send(command, quiet) {
    queue = queue.then(function(){
      return fetch('/cmd?channel=' + channel + '&c=' + encodeURIComponent(command), {cache:'no-store'})
        .then(function(response){
          return response.text().then(function(text){
            if (!response.ok) throw new Error(text);
            if (!quiet) {
              log.textContent = (log.textContent + '> ' + command + '\n' + text).slice(-2000);
              log.scrollTop = log.scrollHeight;
            }
            if (command === 'status') status.textContent = text.trim();
            return text;
          });
        });
    }).catch(function(error){ status.textContent = 'Request failed: ' + error.message; });
    return queue;
  }
  panel.querySelectorAll('[data-command]').forEach(function(button){
    button.addEventListener('click', function(){ send(button.dataset.command); });
  });
  panel.querySelectorAll('[data-pattern]').forEach(function(button){
    button.addEventListener('click', function(){ send(button.dataset.pattern + ' ' + rgb(panel.querySelector('.color').value)); });
  });
  panel.querySelector('.color').addEventListener('change', function(event){ send('color ' + rgb(event.target.value)); });
  [['brightness','bright'],['speed','speed']].forEach(function(pair){
    var input = panel.querySelector('.' + pair[0]), output = panel.querySelector('.' + pair[0] + '-value');
    input.addEventListener('input', function(){ output.textContent = input.value; });
    input.addEventListener('change', function(){ send(pair[1] + ' ' + input.value); });
  });
  panel.querySelector('.count').addEventListener('change', function(event){
    if (event.target.checkValidity()) send('count ' + event.target.value);
  });
  var input = panel.querySelector('.command');
  function sendBox() { if (input.value.trim()) { send(input.value.trim()); input.value = ''; } }
  panel.querySelector('.send').addEventListener('click', sendBox);
  input.addEventListener('keydown', function(event){ if (event.key === 'Enter') sendBox(); });
  // Initialize controls once; polling must not overwrite a user's pending input.
  send('status', true).then(function(text){
    if (!text) return;
    [['brightness','brightness'],['speed','speed'],['count','active_leds']].forEach(function(pair){
      var match = text.match(new RegExp(pair[1] + '=(\\d+)'));
      if (match) {
        var control = panel.querySelector('.' + pair[0]);
        if (document.activeElement !== control) {
          control.value = match[1];
          var output = panel.querySelector('.' + pair[0] + '-value');
          if (output) output.textContent = match[1];
        }
      }
    });
  });
  // Schedule after completion so a disconnected board cannot accumulate polls.
  function poll() { send('status', true).then(function(){ setTimeout(poll, 3000); }); }
  setTimeout(poll, 3000);
}
[16,17,18].forEach(function(pin, index){ mountStrip(index + 1, pin); });
</script>
</main></body></html>
)HTML";

// ---------- web server & captive portal ----------
void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCmd() {
  String value = server.arg("channel");
  if (value != "1" && value != "2" && value != "3") {
    server.send(400, "text/plain", "ERR: channel must be 1, 2, or 3");
    return;
  }
  String out;
  webReply = &out;
  reply("Channel " + value + " / GPIO " + String(CHANNEL_PINS[value.toInt() - 1]));
  channels[value.toInt() - 1].handleManualCommand(server.arg("c"));
  webReply = nullptr;
  server.sendHeader("Cache-Control", "no-store");
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
  for (int i = 0; i < CHANNEL_COUNT; i++) channels[i].begin();
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
    if (c == '\n' || c == '\r') { if (sbuf.length()) { handleSerialCommand(sbuf); sbuf=""; } }
    else sbuf += c;
  }

  // 2) network
  dnsServer.processNextRequest();
  server.handleClient();

  // 3) run animations & scripts
  for (int i = 0; i < CHANNEL_COUNT; i++) {
    channels[i].runPatterns();
    channels[i].runScript();
  }
  // Start each ready transmitter without waiting on another strip. An active
  // transmitter keeps its own snapshot until the hardware reports completion.
  for (int i = 0; i < CHANNEL_COUNT; i++)
    channels[i].output.service(channels[i].strip.getPixels());
}
