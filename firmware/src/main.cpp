#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "camera.h"
#include "ai_model.h"
#include "dashboard.h"
#include "link.h"

// ---- detection counters (attempts/confirmed arrive from the main
// controller via `state`; local counts are vision-side only) ----
static int count_cls[NUM_CLASSES] = {0};
static int count_attempts = 0;
static int count_confirmed = 0;

static bool camera_ok = false;
static bool ai_ok = false;

static target_dir_t last_dir = DIR_CENTER;

// ---- thermal guard (0 ok, 1 = AI throttled, 2 = AI paused; config.h) ----
static float cpu_temp_c = 0;
static int thermal_level = 0;

// ---- commands: forward to the main boat controller over UART ----
void cmd_start_auto(void)  { link_send_cmd("auto_on"); }
void cmd_stop_auto(void)   { link_send_cmd("auto_off"); }
void cmd_manual(int dir) {
    switch (dir) {
        case 1: link_send_cmd("fwd"); break;
        case 2: link_send_cmd("rev"); break;
        case 3: link_send_cmd("left"); break;
        case 4: link_send_cmd("right"); break;
        default: link_send_cmd("stopm"); break;
    }
}
void cmd_conveyor_test(void) { link_send_cmd("conveyor"); }
void cmd_reset_counters(void) {
    for (int i = 0; i < NUM_CLASSES; i++) count_cls[i] = 0;
    count_attempts = 0;
    count_confirmed = 0;
    link_send_cmd("reset");
}

// ---- camera MJPEG single frame ----
#include "esp_camera.h"
#include <img_converters.h>

// WiFiClient (arduino-esp32 2.0.x) does NOT override Print::availableForWrite()
// — the base returns 0 — so it must never be used as a TX-space guard: such a
// guard blocks its full budget and the frame body is NEVER written (root cause
// of "headers but no video on every client", found 2026-10-23). WiFiClient::
// write() is already bounded: select(1 s) x 10 retries + MSG_DONTWAIT, then it
// returns short and we abort the stream.

static bool jpeg_capture_and_send(WiFiClient& client) {
    camera_fb_t* fb = camera_jpeg_get();
    if (!fb || !fb->buf) {
        Serial.println("[stream] camera frame failed");
        camera_jpeg_return(fb);
        return false;
    }
    // fallback boards already produce JPEG: pass through after SOI check
    if (fb->format == PIXFORMAT_JPEG) {
        if (fb->len < 4 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
            Serial.printf("[stream] bad jpeg fb len=%u\n", (unsigned)fb->len);
            camera_jpeg_return(fb);
            return false;
        }
        Serial.printf("[stream] frame captured len=%u (passthrough)\n",
                      (unsigned)fb->len);
        char hdr[96];
        int hlen = snprintf(hdr, sizeof(hdr),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            (unsigned)fb->len);
        uint32_t tw = millis();
        size_t wn = client.print(hdr);
        size_t flen = fb->len;
        size_t wb = (wn == (size_t)hlen) ? client.write(fb->buf, flen) : 0;
        camera_jpeg_return(fb);
        bool ok = (wn == (size_t)hlen) && (wb == flen);
        if (!ok) {
            Serial.printf("[stream] send failed/timeout (w=%u+%u/%u+%u in %ums)\n",
                          (unsigned)wn, (unsigned)wb, (unsigned)hlen,
                          (unsigned)flen, (unsigned)(millis() - tw));
            return false;
        }
        Serial.println("[stream] frame sent");
        return true;
    }
    // this board's sensor only outputs RGB565: software-encode to JPEG
    uint32_t t0 = millis();
    uint8_t* jpg = nullptr;
    size_t jlen = 0;
    bool ok = frame2jpg(fb, 10, &jpg, &jlen);
    camera_jpeg_return(fb);
    if (!ok || !jpg || jlen < 4) {
        Serial.println("[stream] frame2jpg failed");
        free(jpg);
        return false;
    }
    // verify JPEG container markers before sending (req: valid SOI/EOI)
    if (jpg[0] != 0xFF || jpg[1] != 0xD8 ||
        jpg[jlen - 2] != 0xFF || jpg[jlen - 1] != 0xD9) {
        Serial.printf("[stream] bad jpeg len=%u (no SOI/EOI)\n", (unsigned)jlen);
        free(jpg);
        return false;
    }
    Serial.printf("[stream] frame captured len=%u enc=%ums\n",
                  (unsigned)jlen, (unsigned)(millis() - t0));
    char hdr[96];
    int hlen = snprintf(hdr, sizeof(hdr),
        "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
        (unsigned)jlen);
    uint32_t tw = millis();
    size_t wn = client.print(hdr);
    size_t wb = (wn == (size_t)hlen) ? client.write(jpg, jlen) : 0;
    ok = (wn == (size_t)hlen) && (wb == jlen);
    free(jpg);
    if (!ok) {
        Serial.printf("[stream] send failed/timeout (w=%u+%u/%u+%u in %ums)\n",
                      (unsigned)wn, (unsigned)wb, (unsigned)hlen,
                      (unsigned)jlen, (unsigned)(millis() - tw));
        return false;
    }
    Serial.println("[stream] frame sent");
    return true;
}

bool stream_one(WiFiClient& c) { return jpeg_capture_and_send(c); }

// ---- region classification ----
static uint8_t* region_buf = nullptr;

static bool ai_with_regions(ai_result_t* out, target_dir_t* dir) {
    if (!ai_infer_full(out)) return false;
    if (out->best_conf < DETECT_CONF_THRESHOLD) return false;

    struct { float x; target_dir_t d; } regions[3] = {
        {0.0f, DIR_LEFT}, {0.33f, DIR_CENTER}, {0.67f, DIR_RIGHT}
    };
    float best = out->best_conf;
    target_dir_t best_dir = DIR_CENTER;
    if (!region_buf) {
        region_buf = (uint8_t*)ps_malloc(INFERENCE_SIZE * INFERENCE_SIZE * 3);
        if (!region_buf) return true;
    }
    for (int i = 0; i < 3; i++) {
        ai_result_t r;
        if (!camera_crop_resize(regions[i].x, 0, 0.33f, 1.0f,
                                region_buf, INFERENCE_SIZE)) continue;
        if (!ai_infer(region_buf, INFERENCE_SIZE, &r)) continue;
        if (r.best_class == out->best_class && r.best_conf > best) {
            best = r.best_conf;
            best_dir = regions[i].d;
        }
    }
    out->best_conf = best;
    *dir = best_dir;
    last_dir = best_dir;
    return true;
}

// ---- runtime fault forwarding (rate-limited) ----
static void vision_fault(const char* msg) {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 10000) return;
    last = now;
    Serial.printf("[fault] %s\n", msg);
    link_send_fault(msg);
}

// ---- timing ----
static uint32_t t_ai = 0, t_dash = 0, t_therm = 0;
static int infer_fail_streak = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    // AI-Thinker flash LED (GPIO4) off: constant light = extra heat + current
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    Serial.printf("\n[boot] reset_reason=%d free_heap=%u psram=%d spiram_free=%u role=CAM_AI\n",
                  (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap(),
                  (int)psramFound(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    uint32_t t0 = millis();

    link_init();
    Serial.printf("[init] link   %ums\n", (unsigned)(millis() - t0));

    // AI arena FIRST: needs one large contiguous DRAM block; camera init
    // fragments DRAM (DMA descriptors) and pushes malloc to PSRAM (slow).
    ai_ok = ai_init();

    camera_ok = camera_init();
    Serial.printf("[cam] init %s (%ums)\n", camera_ok ? "OK" : "FAILED",
                  (unsigned)(millis() - t0));

    ai_ok = ai_ok && camera_ok;
    Serial.printf("[ai] ready=%d (%ums) heap=%u\n", (int)ai_ok,
                  (unsigned)(millis() - t0), (unsigned)ESP.getFreeHeap());

    dashboard_begin();
    Serial.printf("[dash] up (%ums) heap=%u\n", (unsigned)(millis() - t0),
                  (unsigned)ESP.getFreeHeap());

    if (!camera_ok) {
        Serial.println("[selftest] camera FAILED -> FAULT_STOP");
        link_send_fault("camera_init_failed");
    } else if (!ai_ok) {
        Serial.println("[selftest] camera OK but AI FAILED -> FAULT_STOP");
        link_send_fault("ai_init_failed");
    } else {
        Serial.println("[selftest] camera+ai ready; vision-only, boat via link");
    }
}

void loop() {
    uint32_t now = millis();
    dashboard_loop();
    link_loop(now);

    static uint32_t t_hb = 0;
    if (now - t_hb >= 1000) {
        t_hb = now;
        Serial.printf("[hb] up=%us state=%s ai=%d cam=%d auto=%d heap=%u cls=%d/%d/%d/%d att=%d wifi=%d t=%.0fC th=%d\n",
            (unsigned)(now/1000), link_boat_state(), (int)ai_ok,
            (int)camera_ok, (int)link_main_autoflag(),
            (unsigned)ESP.getFreeHeap(),
            count_cls[0], count_cls[1], count_cls[2], count_cls[3],
            count_attempts, (int)WiFi.softAPgetStationNum(),
            cpu_temp_c, thermal_level);
    }

    // ---- thermal guard: internal sensor once per second, hysteresis ----
    if (now - t_therm >= 1000) {
        t_therm = now;
        cpu_temp_c = temperatureRead();
        int prev = thermal_level;
        if (cpu_temp_c >= THERMAL_LIMIT_C) thermal_level = 2;
        else if (cpu_temp_c >= THERMAL_WARN_C) thermal_level = thermal_level > 1 ? 2 : 1;
        else if (cpu_temp_c < THERMAL_RESUME_C) thermal_level = 0;
        if (thermal_level != prev) {
            Serial.printf("[therm] %.0fC level %d -> %d (%s)\n", cpu_temp_c,
                          prev, thermal_level,
                          thermal_level == 0 ? "normal"
                          : thermal_level == 1 ? "AI throttled to 2s"
                                               : "AI PAUSED until cools");
        }
    }
    link_set_status(camera_ok, ai_ok, cpu_temp_c, thermal_level);

    // ---- AI every AI_PERIOD_MS (skip when streaming / too hot) ----
    ai_result_t res = {};
    target_dir_t dir = DIR_CENTER;
    bool have_ai = false;
    if (camera_ok && ai_ok && thermal_level < 2 && !camera_streaming() &&
        now - t_ai >= AI_PERIOD_MS &&
        (thermal_level == 0 || now - t_ai >= THERMAL_MIN_INTERVAL_MS)) {
        t_ai = now;
        uint32_t t0 = millis();
        have_ai = ai_with_regions(&res, &dir);
        if (!have_ai || !res.ok) {
            if (++infer_fail_streak >= 5) {
                infer_fail_streak = 5;
                vision_fault("ai_infer_failed");
            }
        } else {
            infer_fail_streak = 0;
        }
        Serial.printf("[ai] cycle %ums conf=%.2f cls=%d\n",
                      (unsigned)(millis()-t0), res.best_conf, res.best_class);
        if (have_ai && res.ok) {
            bool detected = res.best_conf >= DETECT_CONF_THRESHOLD;
            if (detected) count_cls[res.best_class]++;
            link_send_det(res.best_class, res.best_conf, (int)dir, detected);
        }
    }

    // ---- dashboard snapshot ----
    if (now - t_dash >= DASHBOARD_JSON_PERIOD_MS) {
        t_dash = now;
        rb_state_t st = {};
        bool have_state = link_get_state(&st);
        bool up = link_up(now);
        const char* rg = "-";
        if (have_ai && res.best_conf >= DETECT_CONF_THRESHOLD) {
            rg = (dir == DIR_LEFT) ? "left" : (dir == DIR_RIGHT) ? "right" : "center";
        }
        dash_snapshot_t s = {};
        s.state = up ? link_boat_state() : "NO_LINK";
        s.ai_class = (have_ai && res.best_conf >= DETECT_CONF_THRESHOLD) ? res.best_class : -1;
        s.ai_conf = have_ai ? res.best_conf : 0;
        s.region = rg;
        s.count_plastic_bottle = count_cls[0];
        s.count_plastic_wrapper = count_cls[1];
        s.count_organic = count_cls[2];
        s.count_other = count_cls[3] + count_cls[4];
        s.collection_attempts = have_state ? st.att : count_attempts;
        s.collection_confirmed = have_state ? st.cfm : count_confirmed;
        s.basket_state = have_state ? st.basket : "NO_LINK";
        s.basket_fill_pct = have_state ? st.fill : 0;
        s.obstacle_left_cm = have_state ? st.obs_l : -1;
        s.obstacle_right_cm = have_state ? st.obs_r : -1;
        s.lat = have_state ? st.lat : 0;
        s.lon = have_state ? st.lon : 0;
        s.gps_fix = have_state ? (st.fix != 0) : false;
        s.gps_sats = have_state ? st.sats : 0;
        switch (have_state ? st.conv : -1) {
            case 0: s.conveyor = "OFF"; break;
            case 1: s.conveyor = "RUN"; break;
            case 2: s.conveyor = "TIMEOUT"; break;
            case 3: s.conveyor = "FAULT"; break;
            default: s.conveyor = "NO_LINK"; break;
        }
        const char* f = have_state ? st.fault : "";
        if ((!f || !f[0]) && (!camera_ok || !ai_ok)) f = "FAULT";
        if ((!f || !f[0]) && !up) f = "LINK_LOST";
        s.fault = (f && f[0]) ? f : "";
        s.wifi_on = true;
        s.battery_v = -1;
        s.cpu_temp = cpu_temp_c;
        s.thermal = thermal_level;
        dashboard_update(&s);
    }

    delay(CONTROL_LOOP_MS / 2);
}
