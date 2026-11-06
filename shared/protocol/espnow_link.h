#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

/* ========================================================================
 * Shared ESP-NOW transport helpers for the river cleaning boat.
 *
 * Replaces the wired UART2 link between the ESP32-CAM (AI vision) and the
 * standard ESP32 (main controller) with a wireless ESP-NOW link:
 *
 *   - Connectionless, no pairing, no key exchange, no per-board MAC
 *     configuration. Broadcast addressing (FF:FF:FF:FF:FF:FF): any board
 *     hearing the frame can process it; the framed protocol provides
 *     validation.
 *   - Bootstrap channel ESPNOW_CHANNEL (1) before any AP association.
 *     Both boards also run Wi-Fi infrastructure (main: router STA +
 *     dashboard SoftAP; CAM: joins the dashboard SoftAP for video) and
 *     the ESP32 has one radio, so once associated ESP-NOW must operate
 *     on the *actual* Wi-Fi channel. ESP-IDF keeps an internal "home
 *     channel" from esp_now_init() time that does not follow later STA
 *     channel moves (IDFGH-17728): sends then fail with "Peer channel is
 *     not equal to the home channel" and receives are filtered out.
 *     espnow_link_resync() detects the drift and rebuilds ESP-NOW on the
 *     live channel. Call it from the SAME translation unit that called
 *     espnow_link_begin() (the module state is file-static): the main
 *     controller from SerialRx::update(), the CAM from SerialTx sends.
 *   - Modem sleep disabled for lowest latency and no idle dropouts.
 *   - Payload up to 250 bytes; protocol frames are <= 71 bytes, so each
 *     frame travels as exactly one ESP-NOW datagram.
 *
 * Typical latency: well under 1 ms on the same hull.
 * ======================================================================== */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_CHANNEL        1
#define ESPNOW_MAX_PACKET     250u

/* Exported by libnet80211 but not declared in this IDF's esp_wifi.h.
 * Sets the Wi-Fi/ESP-NOW "home channel" (the ESP-NOW RX-filter channel)
 * without moving the radio when already on that channel. Signature and
 * linkage match the internal definition. See IDFGH-17728. */
extern "C" esp_err_t esp_wifi_set_home_channel(uint8_t primary,
                                               wifi_second_chan_t second);

static const uint8_t ESPNOW_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Channel the ESP-NOW module was last built on (file-static: only the
 * translation unit that owns begin() may call resync()). */
static uint8_t s_home_ch = 0;
static bool    s_live     = false;

/* Bring up the radio + ESP-NOW and register the broadcast peer.
 *
 * ap_ssid == nullptr: plain station mode (ESP32-CAM).
 * ap_ssid != nullptr: bring up a WPA2 SoftAP on ESPNOW_CHANNEL for the
 * dashboard WebSocket gateway (main controller) — until the STA joins
 * the router, when the AP (and ESP-NOW, via resync) follow the router's
 * channel. Returns true when the link is ready to send/receive. */
static inline bool espnow_link_begin(const char *ap_ssid = nullptr,
                                     const char *ap_pass = nullptr) {
    if (!WiFi.mode(ap_ssid ? WIFI_AP_STA : WIFI_STA)) return false;
    WiFi.setSleep(false);                     /* no modem sleep */
    if (ap_ssid) {
        /* Channel fixed to ESPNOW_CHANNEL; max 4 stations (laptop only). */
        bool ok = (ap_pass && strlen(ap_pass) >= 8)
            ? WiFi.softAP(ap_ssid, ap_pass, ESPNOW_CHANNEL, 0, 4)
            : WiFi.softAP(ap_ssid, nullptr, ESPNOW_CHANNEL, 0, 4);
        if (!ok) return false;
    }
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) return false;

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, ESPNOW_BROADCAST_MAC, 6);
    peer.channel  = ESPNOW_CHANNEL;
    peer.encrypt  = false;                    /* local link, no keys */
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    uint8_t ch = 0;
    wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&ch, &sec) != ESP_OK || ch == 0)
        ch = ESPNOW_CHANNEL;
    s_home_ch = ch;
    s_live = true;
    return true;
}

/* Re-synchronize ESP-NOW with the live Wi-Fi channel. Call from loop().
 *
 * Returns true when ESP-NOW was re-created — the caller must re-register
 * its esp_now callbacks (esp_now_deinit clears them). Returns false when
 * nothing changed, when the link is down, or when the rebuild is being
 * throttled (the STA can hop channels several times while associating). */
static inline bool espnow_link_resync(void) {
    if (!s_live) return false;
    uint8_t ch = 0;
    wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&ch, &sec) != ESP_OK || ch == 0 ||
        ch == s_home_ch) {
        return false;
    }
    static uint32_t s_last_ms = 0;             /* limit re-init churn */
    uint32_t now = millis();
    if (now - s_last_ms < 1000u) return false;
    s_last_ms = now;

    /* IDF IDFGH-17728: home_channel does not follow STA channel moves.
     * Force it onto the live channel, then rebuild ESP-NOW there. */
    esp_wifi_set_home_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_now_deinit();
    if (esp_now_init() != ESP_OK) return false;   /* retried next second */
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, ESPNOW_BROADCAST_MAC, 6);
    peer.channel  = ch;
    peer.encrypt  = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    Serial.printf("[ESPNOW] channel %u -> %u, link rebuilt\n",
                  s_home_ch, ch);
    s_home_ch = ch;
    return true;
}

/* Send one datagram to the broadcast peer. */
static inline bool espnow_link_send(const uint8_t *data, size_t len) {
    if (!data || len == 0 || len > ESPNOW_MAX_PACKET) return false;
    return esp_now_send(ESPNOW_BROADCAST_MAC, data, len) == ESP_OK;
}

/* Human-readable own MAC (diagnostics / future unicast use). */
static inline void espnow_link_mac_str(char *buf, size_t buflen) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(buf, buflen, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#endif /* ESPNOW_LINK_H */
