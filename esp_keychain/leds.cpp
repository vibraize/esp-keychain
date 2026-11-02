#include "leds.h"

#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "themes.h"

namespace leds {
namespace {

Adafruit_NeoPixel strip(LED_COUNT, PIN_LED_DATA, LED_PIXEL_TYPE);

const Theme& self = THEMES[SELF_THEME_INDEX];
const Theme& peer = THEMES[PEER_THEME_INDEX];

uint32_t bootMs = 0;
uint32_t nextFrameMs = 0;
bool dark = false;  // true once we've pushed an all-off frame in OUT_OF_RANGE

// Scale 0xRRGGBB by a perceptual level (0..1). Gamma goes on the level, then the
// hard cap, so LED_MAX_BRIGHTNESS stays the true ceiling.
uint32_t scaled(uint32_t rgb, float level) {
  level = constrain(level, 0.0f, 1.0f);
  float k = powf(level, 2.2f) * LED_MAX_BRIGHTNESS / 255.0f;
  uint8_t r = ((rgb >> 16) & 0xFF) * k;
  uint8_t g = ((rgb >> 8) & 0xFF) * k;
  uint8_t b = (rgb & 0xFF) * k;
  return strip.Color(r, g, b);
}

void fill(uint32_t c) {
  for (uint8_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, c);
}

float wave(uint32_t clk, uint32_t period) {
  return 0.5f - 0.5f * cosf((clk % period) * (TWO_PI / period));
}

void chase(uint32_t clk) {
  const float n = LED_COUNT;
  float head;
#if LED_CHASE_MODE == 0
  head = fmodf((float)clk / LED_CHASE_STEP_MS, n);
#else
  float span = (n > 1) ? 2.0f * (n - 1) : 1.0f;
  float t = fmodf((float)clk / LED_CHASE_STEP_MS, span);
  head = (t < n - 1) ? t : span - t;
#endif
  // Head in own primary, alternating laps in the peer's primary.
#if LED_CHASE_MODE == 0
  const uint32_t lapMs = LED_CHASE_STEP_MS * LED_COUNT;
#else
  const uint32_t lapMs = LED_CHASE_STEP_MS * (LED_COUNT > 1 ? 2 * (LED_COUNT - 1) : 1);
#endif
  uint32_t lap = clk / lapMs;
  uint32_t color = (lap % 2) ? peer.palette[0] : self.palette[0];

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    float d = fabsf(i - head);
#if LED_CHASE_MODE == 0
    d = fminf(d, n - d);  // circular distance
#endif
    float lvl = 1.0f - d / LED_CHASE_TAIL;
    uint8_t idx = LED_CHASE_REVERSE ? (LED_COUNT - 1 - i) : i;
    strip.setPixelColor(idx, lvl > 0 ? scaled(color, lvl) : scaled(self.palette[1], 0.3f));
  }
}

}  // namespace

void begin() {
  strip.begin();
  strip.clear();
  strip.show();
  bootMs = millis();
}

void update(uint32_t now, uint32_t clk, Zone zone) {
  if ((int32_t)(now - nextFrameMs) < 0) return;
  nextFrameMs = now + LED_FRAME_MS;

  if (now - bootMs < BOOT_SPLASH_MS) {
    // Boot: own color fades in, identifies which unit this is.
    fill(scaled(self.palette[0], (float)(now - bootMs) / BOOT_SPLASH_MS));
    strip.show();
    return;
  }

  if (zone == ZONE_OUT_OF_RANGE) {
    if (!dark) { strip.clear(); strip.show(); dark = true; }
    return;
  }
  dark = false;

  strip.clear();
  switch (zone) {
    case ZONE_FAR:
#if LED_FAR_ENABLE
      if ((clk / 600) % 2 == 0) strip.setPixelColor(LED_COUNT / 2, scaled(self.palette[0], LED_FAR_LEVEL));
#endif
      break;
    case ZONE_NEAR:
#if LED_NEAR_ENABLE
      fill(scaled(self.palette[1], LED_NEAR_LEVEL * (0.2f + 0.8f * wave(clk, 1200))));
#endif
      break;
    case ZONE_CLOSE:
      chase(clk);
      break;
    default:
      break;
  }
  strip.show();
}

}  // namespace leds
