// config.h - every tunable lives here. The only line that must differ between
// the two keychains is DEVICE_ID.
#pragma once

#include <Arduino.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
#define DEVICE_ID 0            // 0 or 1. Flash one unit with each.
#define PEER_ID (1 - DEVICE_ID)

// Theme indices into THEMES[] (themes.h). Default: theme follows device ID.
#define SELF_THEME_INDEX DEVICE_ID
#define PEER_THEME_INDEX PEER_ID

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------
#define DEBUG_SERIAL 1         // 1 = print raw/smoothed RSSI + zone for Serial Plotter
#define DEBUG_PRINT_MS 100
#define DEBUG_I2C_SCAN 1       // 1 = list I2C devices at boot (confirms OLED address)

#define SIMULATE_RSSI 0        // 1 = radio off, RSSI comes from Serial input (see README)

// ---------------------------------------------------------------------------
// Pins (avoid strapping pins GPIO2, GPIO8, GPIO9)
// ---------------------------------------------------------------------------
#define PIN_SDA 6
#define PIN_SCL 7
#define PIN_LED_DATA 4

// ---------------------------------------------------------------------------
// OLED (SSD1306 128x32)
// ---------------------------------------------------------------------------
#define OLED_I2C_ADDR 0x3C     // 7-bit address. Unverified: check the boot I2C scan.
#define OLED_I2C_HZ 400000
#define OLED_FLIP 0            // 1 = rotate 180 deg if the module is mounted upside down
#define OLED_CONTRAST_MAX 255
#define OLED_CONTRAST_MIN 20   // low end of the NEAR pulse and the idle dim level
#define OLED_IDLE_BLANK_MS 3000 // OUT_OF_RANGE: dim idle sprite this long, then panel off. 0 = blank immediately
#define ANIM_FRAME_MS 50       // ~20 fps. Each full frame is ~512 B over I2C.
#define BOOT_SPLASH_MS 1500

// ---------------------------------------------------------------------------
// LEDs (3x PL9823-F5 through-hole addressable, chained DIN->DOUT from PIN_LED_DATA)
// ---------------------------------------------------------------------------
#define LED_COUNT 3
// Color order + speed for Adafruit_NeoPixel. PL9823 is usually RGB; SMD
// WS2812B is GRB. If the boot fade shows the wrong color (e.g. green instead
// of pink on theme 0), try NEO_GRB.
#define LED_PIXEL_TYPE (NEO_RGB + NEO_KHZ800)
#define LED_MAX_BRIGHTNESS 40  // hard cap, 0-255. Big impact on battery life.
#define LED_FRAME_MS 20
#define LED_CHASE_MODE 1       // 0 = wrap (loop/triangle layout), 1 = ping-pong (row layout)
#define LED_CHASE_REVERSE 0
#define LED_CHASE_STEP_MS 140  // time for the head to move one LED
#define LED_CHASE_TAIL 1.3f    // tail length in LEDs
#define LED_FAR_ENABLE 1       // faint single-LED blink in FAR
#define LED_NEAR_ENABLE 1      // soft breathing in NEAR
#define LED_FAR_LEVEL 0.25f    // fraction of LED_MAX_BRIGHTNESS
#define LED_NEAR_LEVEL 0.5f

// ---------------------------------------------------------------------------
// Radio / ESP-NOW
// ---------------------------------------------------------------------------
#define ESPNOW_CHANNEL 1       // both units must match (1-13)
#define PACKET_MAGIC 0xA7      // change to isolate from other builds of this firmware
#define WIFI_TX_POWER_FIXED 1  // 1 = force TX power below for consistent RSSI
#define WIFI_TX_POWER WIFI_POWER_8_5dBm
#define CPU_FREQ_MHZ 160       // 80 saves some power (WiFi needs >= 80). 160 is the core default.

#define BEACON_SLOW_MS 500     // no peer heard
#define BEACON_FAST_MS 150     // peer heard
#define BEACON_JITTER_MS 30    // random extra delay so both units don't stay in lockstep
#define PEER_TIMEOUT_MS 2000   // no packet for this long -> OUT_OF_RANGE

// ---------------------------------------------------------------------------
// RSSI filtering: median of last N, then EMA. N=1 disables median, alpha=1 disables EMA.
// ---------------------------------------------------------------------------
#define RSSI_MEDIAN_N 5        // 1..15
#define RSSI_EMA_ALPHA 0.35f   // 0..1, higher = faster/noisier

// ---------------------------------------------------------------------------
// Zones (dBm on the smoothed RSSI). Enter a zone at its threshold, leave it
// only after dropping RSSI_HYST_DB below.
// ---------------------------------------------------------------------------
#define RSSI_FAR_DBM -85
#define RSSI_NEAR_DBM -70
#define RSSI_CLOSE_DBM -55
#define RSSI_HYST_DB 5
#define ZONE_MIN_DWELL_MS 300  // min time in a zone before changing (timeout bypasses this)

// ---------------------------------------------------------------------------
// Animation sync. Device 0 is the clock leader; device 1 follows its clock in
// SYNC_MIN_ZONE and above so LED chase and sprite frames line up.
// ---------------------------------------------------------------------------
#define SYNC_ENABLE 1
#define SYNC_MIN_ZONE ZONE_CLOSE

// ---------------------------------------------------------------------------
#if DEVICE_ID != 0 && DEVICE_ID != 1
#error "DEVICE_ID must be 0 or 1"
#endif
#if RSSI_MEDIAN_N < 1 || RSSI_MEDIAN_N > 15
#error "RSSI_MEDIAN_N must be 1..15"
#endif
#if !(RSSI_FAR_DBM < RSSI_NEAR_DBM && RSSI_NEAR_DBM < RSSI_CLOSE_DBM)
#error "Zone thresholds must satisfy FAR < NEAR < CLOSE"
#endif
