#include "dashboard.h"
#include "config.h"
#include "camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(DASHBOARD_PORT);
static dash_snapshot_t snap;
static volatile bool dirty = false;

// control callbacks provided by main.cpp
extern void cmd_start_auto(void);
extern void cmd_stop_auto(void);
extern void cmd_manual(int dir);   // 0=stop 1=fwd 2=rev 3=left 4=right
extern void cmd_conveyor_test(void);
extern void cmd_reset_counters(void);

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>River Bot</title>
<style>
body{font-family:system-ui,sans-serif;background:#0e1726;color:#dfe9f5;margin:0;padding:12px}
h1{font-size:18px;margin:0 0 10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.card{background:#18263b;border-radius:8px;padding:10px}
.card h2{font-size:12px;color:#7fa6d9;margin:0 0 6px;text-transform:uppercase}
.big{font-size:22px;font-weight:700}
.row{display:flex;gap:6px;flex-wrap:wrap;margin-top:10px}
button{background:#2b6cb0;color:#fff;border:0;border-radius:6px;padding:10px 14px;font-size:14px}
button:active{opacity:.7}
button.stop{background:#c53030}
button.warn{background:#b7791f}
#vid{width:100%%;border-radius:8px;background:#000}
.ok{color:#48bb78}.bad{color:#f56565}.wrn{color:#ecc94b}
</style></head><body>
<h1>River Cleaning Boat</h1>
<img id="vid" src="/stream">
<div class="grid">
<div class="card"><h2>State</h2><div class="big" id="st">-</div><div id="ai">-</div></div>
<div class="card"><h2>Region</h2><div class="big" id="rg">-</div></div>
<div class="card"><h2>Basket</h2><div class="big" id="bk">-</div><div id="bp">0%%</div></div>
<div class="card"><h2>Obstacles L/R</h2><div class="big" id="ob">- / -</div></div>
<div class="card"><h2>GPS</h2><div id="gps">-</div></div>
<div class="card"><h2>Counts</h2><div id="ct">-</div></div>
<div class="card"><h2>Attempts</h2><div class="big" id="at">0</div></div>
<div class="card"><h2>Conveyor</h2><div id="cv">-</div></div>
<div class="card"><h2>Thermal</h2><div class="big" id="th">-</div><div id="thn">-</div></div>
</div>
<div class="row">
<button onclick="x('/cmd/start')">Start Auto</button>
<button class="stop" onclick="x('/cmd/stop')">STOP</button>
<button onclick="x('/cmd/rev')">Manual Rev</button>
<button onclick="x('/cmd/left')">Left</button>
<button onclick="x('/cmd/right')">Right</button>
<button class="warn" onclick="x('/cmd/conveyor')">Conveyor Test</button>
<button onclick="if(confirm('Reset counts?'))x('/cmd/reset')">Reset Counts</button>
</div>
<script>
function x(u){fetch(u)}
async function tick(){
 try{
  const r=await fetch('/status.json'); const j=await r.json();
  s.textContent=j.state;
  ai.textContent=j.ai_class>=0?(CLASSN[j.ai_class]+' '+Math.round(j.ai_conf*100)+'%%'):'none';
  rg.textContent=j.region;
  bk.textContent=j.basket_state; bp.textContent=Math.round(j.basket_fill)+'%%';
  ob.textContent=j.obs_l+' / '+j.obs_r;
  gps.textContent=j.gps_fix?(j.lat.toFixed(6)+', '+j.lon.toFixed(6)+' ('+j.sats+' sats)'):'no fix';
  ct.innerHTML='bottle '+j.c_bottle+' · wrap '+j.c_wrap+'<br>organic '+j.c_org+' · other '+j.c_other;
  at.textContent=j.attempts;
  cv.textContent=j.conveyor;
  th.textContent=Math.round(j.cpu_temp)+'°C';
  thn.textContent=j.thermal==0?'normal':(j.thermal==1?'THROTTLED (AI 2s)':'AI PAUSED (hot)');
  thn.className=j.thermal==0?'ok':(j.thermal==1?'wrn':'bad');
 }catch(e){}
}
const CLASSN=JSON.parse('%CLASSN%');
setInterval(tick,500);tick();
</script></body></html>)HTML";

static void handle_index() {
    String html(INDEX_HTML);
    // build class name json array
    String arr = "[";
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (i) arr += ",";
        arr += "\"" + String(CLASS_NAMES[i]) + "\"";
    }
    arr += "]";
    html.replace("%CLASSN%", arr);
    server.send(200, "text/html", html);
}

static void handle_status() {
    JsonDocument d;
    d["state"] = snap.state ? snap.state : "?";
    d["ai_class"] = snap.ai_class;
    d["ai_conf"] = snap.ai_conf;
    d["region"] = snap.region ? snap.region : "-";
    d["c_bottle"] = snap.count_plastic_bottle;
    d["c_wrap"] = snap.count_plastic_wrapper;
    d["c_org"] = snap.count_organic;
    d["c_other"] = snap.count_other;
    d["attempts"] = snap.collection_attempts;
    d["confirmed"] = snap.collection_confirmed;
    d["basket_state"] = snap.basket_state ? snap.basket_state : "?";
    d["basket_fill"] = snap.basket_fill_pct;
    d["obs_l"] = snap.obstacle_left_cm;
    d["obs_r"] = snap.obstacle_right_cm;
    d["lat"] = snap.lat;
    d["lon"] = snap.lon;
    d["gps_fix"] = snap.gps_fix;
    d["sats"] = snap.gps_sats;
    d["conveyor"] = snap.conveyor ? snap.conveyor : "?";
    d["fault"] = snap.fault ? snap.fault : "";
    d["battery"] = snap.battery_v;
    d["wifi_on"] = snap.wifi_on;
    d["cpu_temp"] = snap.cpu_temp;
    d["thermal"] = snap.thermal;
    String out;
    serializeJson(d, out);
    server.send(200, "application/json", out);
}

static void handle_stream(void (*fn)()) { fn(); }

// ---- MJPEG on port 81, own FreeRTOS task ----
static WiFiServer stream_srv(81);
extern bool stream_one(WiFiClient & c);   // defined in main.cpp

static void stream_task(void*) {
    stream_srv.begin();
    for (;;) {
        WiFiClient c = stream_srv.available();
        if (!c || !c.connected()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        // drain the HTTP request line + headers (wait up to 800ms for GET)
        uint32_t t0 = millis();
        bool got = false;
        while (millis() - t0 < 800) {
            if (c.available()) {
                got = true;
                if (c.readStringUntil('\n') == "\r") break;
            } else if (got) {
                break;
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        camera_stream_begin();
        Serial.println("[stream] client connected");
        c.setNoDelay(true);
        uint32_t t_client = millis();
        static const char head[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-store\r\n\r\n";
        c.print(head);
        while (c.connected() && millis() - t_client < 600000) {
            if (!stream_one(c)) break;
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        c.stop();
        camera_stream_end();
    }
}

void dashboard_begin(void) {
    WiFi.mode(WIFI_AP);
    // modem sleep must stay OFF in softAP mode: with sleep on, the radio
    // misses client DHCP/internet-check packets and stations flap
    // (wifi=1/0/1 in heartbeats, "connecting timeout" on phones)
    WiFi.setSleep(false);
    // channel 6: least-congested non-overlapping channel in this environment
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, 6);
    Serial.printf("[wifi] AP '%s' ip=%s\n", WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());

    xTaskCreatePinnedToCore(stream_task, "mjpeg", 6144, nullptr, 1, nullptr, 1);

    server.on("/", HTTP_GET, handle_index);
    server.on("/status.json", HTTP_GET, handle_status);
    server.on("/cmd/start", HTTP_GET, []() { cmd_start_auto(); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/stop", HTTP_GET, []() { cmd_stop_auto(); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/fwd", HTTP_GET, []() { cmd_manual(1); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/rev", HTTP_GET, []() { cmd_manual(2); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/left", HTTP_GET, []() { cmd_manual(3); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/right", HTTP_GET, []() { cmd_manual(4); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/stopm", HTTP_GET, []() { cmd_manual(0); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/conveyor", HTTP_GET, []() { cmd_conveyor_test(); server.send(200, "text/plain", "ok"); });
    server.on("/cmd/reset", HTTP_GET, []() { cmd_reset_counters(); server.send(200, "text/plain", "ok"); });
    server.on("/stream", HTTP_GET, []() {
        // MJPEG lives on its own port/task so /status.json keeps working
        // while a browser holds the stream open (single-threaded WebServer).
        server.sendHeader("Location",
                          "http://" + WiFi.softAPIP().toString() + ":81/");
        server.send(302, "text/plain", "redirect");
    });
    server.begin();
}

void dashboard_update(const dash_snapshot_t* s) {
    snap = *s;
    dirty = true;
}

void dashboard_loop(void) {
    server.handleClient();
}
