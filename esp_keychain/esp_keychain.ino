// esp_keychain - paired ESP32-C3 proximity keychains.
// Set DEVICE_ID in config.h before flashing each unit.

#include "config.h"
#include "display_anim.h"
#include "leds.h"
#include "proximity.h"

#if DEBUG_SERIAL
// Map a zone onto the dBm axis so it plots as a step line next to the RSSI traces.
static int zoneToDbm(Zone z) {
  switch (z) {
    case ZONE_FAR: return RSSI_FAR_DBM;
    case ZONE_NEAR: return RSSI_NEAR_DBM;
    case ZONE_CLOSE: return RSSI_CLOSE_DBM;
    default: return -100;
  }
}

static void debugPrint(uint32_t now) {
  static uint32_t nextMs = 0;
  if ((int32_t)(now - nextMs) < 0) return;
  nextMs = now + DEBUG_PRINT_MS;
  // "label:value" pairs, Arduino IDE 2 Serial Plotter format.
  Serial.printf("raw:%d,smooth:%.1f,zone:%d,far:%d,near:%d,close:%d\n",
                prox::lastRawRssi(), prox::smoothedRssi(), zoneToDbm(prox::zone()),
                RSSI_FAR_DBM, RSSI_NEAR_DBM, RSSI_CLOSE_DBM);
}
#endif

void setup() {
  Serial.begin(115200);
  // No wait-for-Serial: on battery there is no host and the units must start anyway.
  leds::begin();
  anim::begin();
  prox::begin();
  Serial.printf("# esp_keychain DEVICE_ID=%d channel=%d\n", DEVICE_ID, ESPNOW_CHANNEL);
}

void loop() {
  uint32_t now = millis();
  prox::update(now);
  Zone z = prox::zone();
  uint32_t clk = prox::animClock(now);
  anim::update(now, clk, z);
  leds::update(now, clk, z);
#if DEBUG_SERIAL
  debugPrint(now);
#endif
}
