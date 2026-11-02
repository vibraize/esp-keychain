// proximity.h - ESP-NOW beaconing, RSSI smoothing, zone state, animation clock sync.
#pragma once

#include <Arduino.h>

enum Zone : uint8_t {
  ZONE_OUT_OF_RANGE = 0,
  ZONE_FAR = 1,
  ZONE_NEAR = 2,
  ZONE_CLOSE = 3,
};

namespace prox {

void begin();
void update(uint32_t now);  // call every loop: drains received samples, beacons, updates zone

Zone zone();
bool peerPresent();         // a valid packet arrived within PEER_TIMEOUT_MS
int lastRawRssi();          // most recent raw sample, -100 if none
float smoothedRssi();       // filtered value, -100 if no peer

// Animation clock: millis() locally, or the leader's clock when synced.
uint32_t animClock(uint32_t now);

const char* zoneName(Zone z);

}  // namespace prox
