/*
 * I2C bus scanner with a Wi-Fi web display - ESP32
 *
 *   I2C:  SDA = GPIO 21, SCL = GPIO 22
 *
 * The ESP32 creates its own Wi-Fi network and hosts the results page, so
 * no router and no USB serial connection are needed:
 *
 *   1) Join the Wi-Fi network "ESP32-Scan" from your phone/laptop
 *   2) The page pops up automatically (captive portal),
 *      or browse to  http://192.168.4.1
 *
 * Libraries: WiFi / WebServer / DNSServer are built into the ESP32 core.
 *            Nothing else needed - the bus is scanned with plain Wire.
 *
 * WHY THIS EXISTS
 *   A garbled USB serial link makes every result suspect. This moves the
 *   readout off the cable entirely, so what you see is what the chip
 *   found.
 *
 * WHAT IT SHOWS
 *   Every address that answers, a guess at what the device is, and - the
 *   useful part - a RELIABILITY figure: the percentage of scans since
 *   boot in which that device replied.
 *
 *     100%          solid wiring
 *     anything less something is intermittent - a loose jumper, a
 *                   marginal connection, or a failing device
 *
 *   A device that is properly connected answers every single time. That
 *   column is what separates a wiring fault from a dead part.
 */

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#define SDA_PIN   21
#define SCL_PIN   22
#define I2C_HZ    100000    // 100 kHz - the forgiving speed for scanning
#define SCAN_MS   1000      // rescan interval

// ---- Wi-Fi access point settings ----
const char* AP_SSID = "ESP32-Scan";
const char* AP_PASS = "";           // "" = open network, or 8+ chars for WPA2

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// ---- results ----
// One slot per possible 7-bit address. Cheap at 128 entries and it means
// no searching or allocation while scanning.
struct Slot {
  bool     everSeen = false;   // has it ever answered since boot
  bool     present  = false;   // did it answer on the most recent scan
  uint32_t seen     = 0;       // number of scans it answered
  uint32_t firstScan = 0;      // scan number it first appeared on
};
Slot slots[128];
uint32_t scanCount = 0;
unsigned long lastScan = 0;

// Common addresses, so the page names a device rather than just printing
// a number. Several parts share addresses, hence the "or".
const char* guessDevice(uint8_t addr) {
  switch (addr) {
    case 0x0D: return "HMC5883L magnetometer (alt)";
    case 0x18: return "LIS3DH accelerometer / MCP9808 temp";
    case 0x1E: return "HMC5883L / LSM303 magnetometer";
    case 0x20: return "PCF8574 or MCP23017 I/O expander";
    case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
               return "PCF8574 / MCP23017 expander (or LCD backpack)";
    case 0x29: return "TSL2561 light / VL53L0X distance";
    case 0x38: return "AHT10/AHT20 humidity or FT6206 touch";
    case 0x39: return "TSL2561 light sensor (alt)";
    case 0x3C: case 0x3D: return "SSD1306 / SH1106 OLED display";
    case 0x40: return "INA219 current / HTU21D / Si7021 humidity";
    case 0x44: case 0x45: return "SHT3x temperature+humidity";
    case 0x48: return "ADS1115 ADC / LM75 temperature";
    case 0x49: case 0x4A: case 0x4B: return "ADS1115 ADC (alt address)";
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56:
               return "AT24Cxx EEPROM";
    case 0x57: return "MAX30102/MAX30105 pulse sensor (or EEPROM)";
    case 0x5A: return "MLX90614 IR thermometer / CCS811 gas";
    case 0x68: return "MPU6050/MPU9250 IMU (or DS1307/DS3231 RTC)";
    case 0x69: return "MPU6050 IMU, AD0 pulled high";
    case 0x76: case 0x77: return "BMP280 / BME280 pressure+humidity";
    default:   return "unknown";
  }
}

// ---------- the scan ----------
void scanBus() {
  scanCount++;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    bool ack = (err == 0 || err == 4);   // 4 = present but complaining

    slots[addr].present = ack;
    if (ack) {
      if (!slots[addr].everSeen) {
        slots[addr].everSeen = true;
        slots[addr].firstScan = scanCount;
      }
      slots[addr].seen++;
    }
  }
}

// ---------- JSON for the page ----------
String buildJson() {
  String j = "{\"scans\":" + String(scanCount) + ",\"devices\":[";
  bool first = true;

  for (uint8_t addr = 1; addr < 127; addr++) {
    if (!slots[addr].everSeen) continue;

    // Reliability is measured from the scan it first appeared on, not
    // from boot - otherwise plugging something in halfway through makes
    // a perfectly good device look 50% flaky.
    uint32_t window = scanCount - slots[addr].firstScan + 1;
    int pct = window ? (int)((slots[addr].seen * 100) / window) : 0;

    char hex[8];
    snprintf(hex, sizeof(hex), "0x%02X", addr);

    if (!first) j += ',';
    first = false;
    j += "{\"hex\":\"" + String(hex) + "\",";
    j += "\"name\":\"" + String(guessDevice(addr)) + "\",";
    j += "\"present\":" + String(slots[addr].present ? "true" : "false") + ",";
    j += "\"seen\":" + String(slots[addr].seen) + ",";
    j += "\"pct\":" + String(pct) + "}";
  }
  j += "]}";
  return j;
}

// ---------- the page ----------
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>I2C Scanner</title>
<style>
  :root { --bg:#101318; --card:#1a1f27; --edge:#2a3140; --text:#e8ecf2;
          --dim:#8b95a5; --ok:#4ade80; --warn:#fbbf24; --bad:#f87171; }
  * { box-sizing:border-box; }
  body { margin:0; font-family:-apple-system,'Segoe UI',Roboto,sans-serif;
         background:var(--bg); color:var(--text); }
  .wrap { max-width:560px; margin:0 auto; padding:16px 14px 40px; }
  h1 { font-size:20px; margin:8px 2px 4px; }
  #meta { font-size:12px; color:var(--dim); margin:0 2px 16px; }
  .dev { background:var(--card); border:1px solid var(--edge); border-radius:12px;
         padding:12px 14px; margin-bottom:10px; }
  .top { display:flex; align-items:baseline; gap:10px; }
  .addr { font-family:ui-monospace,Menlo,Consolas,monospace; font-size:18px;
          font-weight:600; }
  .dot { width:9px; height:9px; border-radius:50%; flex:0 0 9px; }
  .name { font-size:13px; color:var(--dim); margin-top:5px; }
  .rel { font-size:12px; margin-top:8px; display:flex; align-items:center; gap:8px; }
  .bar { flex:1; height:5px; background:#2a3140; border-radius:3px; overflow:hidden; }
  .fill { height:100%; border-radius:3px; }
  .pct { width:38px; text-align:right; font-variant-numeric:tabular-nums; }
  .empty { color:var(--dim); font-size:14px; text-align:center; padding:34px 10px;
           background:var(--card); border:1px solid var(--edge); border-radius:12px;
           line-height:1.6; }
  .note { font-size:12px; color:var(--dim); margin-top:16px; line-height:1.6; }
</style>
</head>
<body>
<div class="wrap">
  <h1>I2C Scanner</h1>
  <div id="meta">connecting&hellip;</div>
  <div id="list"></div>
  <div class="note">
    Reliability is the share of scans in which a device replied, counted
    from when it first appeared. A solidly wired device sits at 100%.
    Anything less means an intermittent connection.
  </div>
</div>

<script>
function colour(pct, present) {
  if (!present) return 'var(--bad)';
  if (pct >= 99) return 'var(--ok)';
  if (pct >= 90) return 'var(--warn)';
  return 'var(--bad)';
}

function render(d) {
  document.getElementById('meta').textContent =
    d.scans + ' scans - ' + d.devices.length + ' device(s) seen';

  var list = document.getElementById('list');
  if (!d.devices.length) {
    list.innerHTML = '<div class="empty">Nothing on the bus.<br>' +
      'Check power, check SDA/SCL are not swapped,<br>' +
      'and make sure ground is shared.</div>';
    return;
  }

  var html = '';
  for (var i = 0; i < d.devices.length; i++) {
    var v = d.devices[i], c = colour(v.pct, v.present);
    html += '<div class="dev">' +
      '<div class="top"><span class="dot" style="background:' + c + '"></span>' +
      '<span class="addr">' + v.hex + '</span></div>' +
      '<div class="name">' + v.name + '</div>' +
      '<div class="rel"><div class="bar"><div class="fill" style="width:' +
      v.pct + '%;background:' + c + '"></div></div>' +
      '<span class="pct">' + v.pct + '%</span></div>' +
      '</div>';
  }
  list.innerHTML = html;
}

function poll() {
  fetch('/api/scan')
    .then(function(r){ return r.json(); })
    .then(render)
    .catch(function(){
      document.getElementById('meta').textContent = 'lost connection to the ESP32';
    });
}
poll();
setInterval(poll, 1000);
</script>
</body>
</html>
)HTML";

// ---------- web server & captive portal ----------
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }
void handleApi()  { server.send(200, "application/json", buildJson()); }

// Any unknown URL - including the OS "am I online?" probes - is
// redirected to our page. That redirect is what makes phones pop up the
// captive-portal sheet.
void handleNotFound() {
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void setupWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if (strlen(AP_PASS) >= 8) WiFi.softAP(AP_SSID, AP_PASS);
  else                      WiFi.softAP(AP_SSID);        // open network

  dnsServer.start(DNS_PORT, "*", apIP);   // every DNS query -> our IP

  server.on("/", handleRoot);
  server.on("/api/scan", handleApi);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  Serial.begin(9600);          // slow, because the USB link is unreliable
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  setupWifi();

  Serial.printf("\n=== I2C scanner ===\nJoin Wi-Fi '%s' then open http://%s/\n",
                AP_SSID, apIP.toString().c_str());
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (millis() - lastScan >= SCAN_MS) {
    lastScan = millis();
    scanBus();
  }
}
