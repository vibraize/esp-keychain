#include "proximity.h"

#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"

namespace prox {
namespace {

struct __attribute__((packed)) Beacon {
  uint8_t magic;
  uint8_t deviceId;
  uint32_t seq;
  uint32_t animClock;  // sender's animation clock, used for sync
};

const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Samples are produced in the WiFi task (or SIMULATE path) and consumed in loop().
struct Sample {
  int8_t rssi;
  uint32_t rxMs;
  uint32_t peerClock;
  bool hasClock;
};
constexpr uint8_t QUEUE_LEN = 8;
Sample queue[QUEUE_LEN];
volatile uint8_t qHead = 0, qTail = 0;
portMUX_TYPE qMux = portMUX_INITIALIZER_UNLOCKED;

void pushSample(const Sample& s) {
  portENTER_CRITICAL(&qMux);
  uint8_t next = (qHead + 1) % QUEUE_LEN;
  if (next != qTail) {  // drop when full; loop() drains far faster than packets arrive
    queue[qHead] = s;
    qHead = next;
  }
  portEXIT_CRITICAL(&qMux);
}

bool popSample(Sample& s) {
  bool ok = false;
  portENTER_CRITICAL(&qMux);
  if (qTail != qHead) {
    s = queue[qTail];
    qTail = (qTail + 1) % QUEUE_LEN;
    ok = true;
  }
  portEXIT_CRITICAL(&qMux);
  return ok;
}

// --- state ---
Zone curZone = ZONE_OUT_OF_RANGE;
uint32_t zoneSinceMs = 0;
uint32_t lastRxMs = 0;
bool havePeer = false;
int rawRssi = -100;
float ema = -100.0f;
bool emaInit = false;
int8_t medBuf[RSSI_MEDIAN_N];
uint8_t medCount = 0, medIdx = 0;

uint32_t seq = 0;
uint32_t nextBeaconMs = 0;

bool clockSynced = false;
int32_t clockOffset = 0;  // leader clock - local millis()

void resetFilter() {
  medCount = medIdx = 0;
  emaInit = false;
  ema = -100.0f;
}

float filterSample(int8_t x) {
  medBuf[medIdx] = x;
  medIdx = (medIdx + 1) % RSSI_MEDIAN_N;
  if (medCount < RSSI_MEDIAN_N) medCount++;

  int8_t sorted[RSSI_MEDIAN_N];
  memcpy(sorted, medBuf, medCount);
  for (uint8_t i = 1; i < medCount; i++) {
    int8_t v = sorted[i];
    int8_t j = i - 1;
    while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
    sorted[j + 1] = v;
  }
  float med = sorted[medCount / 2];

  if (!emaInit) { ema = med; emaInit = true; }
  else ema += RSSI_EMA_ALPHA * (med - ema);
  return ema;
}

// Entry threshold for each zone, indexed by Zone.
const int ENTER_DBM[4] = {-127, RSSI_FAR_DBM, RSSI_NEAR_DBM, RSSI_CLOSE_DBM};

Zone zoneFor(Zone cur, float rssi) {
  int z = cur;
  while (z < ZONE_CLOSE && rssi >= ENTER_DBM[z + 1]) z++;
  while (z > ZONE_OUT_OF_RANGE && rssi < ENTER_DBM[z] - RSSI_HYST_DB) z--;
  return (Zone)z;
}

void setZone(Zone z, uint32_t now) {
  if (z == curZone) return;
  curZone = z;
  zoneSinceMs = now;
}

#if !SIMULATE_RSSI
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != (int)sizeof(Beacon)) return;
  Beacon b;
  memcpy(&b, data, sizeof(b));
  if (b.magic != PACKET_MAGIC) return;
  if (b.deviceId == DEVICE_ID) return;
  if (b.deviceId != PEER_ID) return;

  Sample s;
  s.rssi = info->rx_ctrl->rssi;
  s.rxMs = millis();
  s.peerClock = b.animClock;
  s.hasClock = true;
  pushSample(s);
}

void sendBeacon(uint32_t now) {
  Beacon b;
  b.magic = PACKET_MAGIC;
  b.deviceId = DEVICE_ID;
  b.seq = seq++;
  b.animClock = animClock(now);
  esp_now_send(BROADCAST, (const uint8_t*)&b, sizeof(b));
}

void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);  // modem sleep would drop ESP-NOW packets
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
#if WIFI_TX_POWER_FIXED
  WiFi.setTxPower(WIFI_TX_POWER);
#endif

  if (esp_now_init() != ESP_OK) {
    Serial.println("# esp_now_init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("# esp_now_add_peer failed");
}
#endif  // !SIMULATE_RSSI

#if SIMULATE_RSSI
// Type a dBm value (e.g. -60) + Enter to feed fake packets; "x" stops them.
char lineBuf[16];
uint8_t lineLen = 0;
bool simActive = false;
int simRssi = -100;
uint32_t nextSimMs = 0;

void simPollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen == 0) continue;
      lineBuf[lineLen] = 0;
      lineLen = 0;
      if (lineBuf[0] == 'x' || lineBuf[0] == 'X') {
        simActive = false;
        Serial.println("# sim: packets stopped");
      } else {
        int v = atoi(lineBuf);
        if (v > 0) v = -v;  // accept "60" as -60
        simRssi = constrain(v, -100, -20);
        simActive = true;
        Serial.printf("# sim: rssi=%d\n", simRssi);
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

void simUpdate(uint32_t now) {
  simPollSerial();
  if (simActive && (int32_t)(now - nextSimMs) >= 0) {
    nextSimMs = now + BEACON_FAST_MS;
    Sample s;
    s.rssi = simRssi + (int)random(-3, 4);  // a little noise to exercise the filter
    s.rxMs = now;
    s.peerClock = 0;
    s.hasClock = false;
    pushSample(s);
  }
}
#endif  // SIMULATE_RSSI

}  // namespace

void begin() {
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
#if SIMULATE_RSSI
  Serial.println("# SIMULATE_RSSI: type a dBm value (e.g. -60) and Enter, 'x' to stop");
#else
  radioBegin();
#endif
  randomSeed(esp_random());
}

void update(uint32_t now) {
#if SIMULATE_RSSI
  simUpdate(now);
#endif

  Sample s;
  while (popSample(s)) {
    lastRxMs = s.rxMs;
    havePeer = true;
    rawRssi = s.rssi;
    filterSample(s.rssi);

#if SYNC_ENABLE
    if (DEVICE_ID != 0 && s.hasClock) {
      int32_t off = (int32_t)(s.peerClock - s.rxMs);
      // Snap on first lock or big jumps, otherwise ease out radio latency jitter.
      if (!clockSynced || abs(off - clockOffset) > 100) clockOffset = off;
      else clockOffset += (off - clockOffset) / 4;
      clockSynced = true;
    }
#endif
  }

  if (havePeer && (now - lastRxMs) > PEER_TIMEOUT_MS) {
    havePeer = false;
    clockSynced = false;
    rawRssi = -100;
    resetFilter();
    setZone(ZONE_OUT_OF_RANGE, now);  // timeout ignores dwell time
  }

  if (havePeer && (now - zoneSinceMs) >= ZONE_MIN_DWELL_MS) {
    setZone(zoneFor(curZone, ema), now);
  }

#if !SIMULATE_RSSI
  if ((int32_t)(now - nextBeaconMs) >= 0) {
    sendBeacon(now);
    uint32_t interval = havePeer ? BEACON_FAST_MS : BEACON_SLOW_MS;
    nextBeaconMs = now + interval + random(0, BEACON_JITTER_MS + 1);
  }
#endif
}

Zone zone() { return curZone; }
bool peerPresent() { return havePeer; }
int lastRawRssi() { return rawRssi; }
float smoothedRssi() { return havePeer ? ema : -100.0f; }

uint32_t animClock(uint32_t now) {
#if SYNC_ENABLE
  if (clockSynced && curZone >= SYNC_MIN_ZONE) return now + clockOffset;
#endif
  return now;
}

const char* zoneName(Zone z) {
  switch (z) {
    case ZONE_FAR: return "FAR";
    case ZONE_NEAR: return "NEAR";
    case ZONE_CLOSE: return "CLOSE";
    default: return "OUT";
  }
}

}  // namespace prox
