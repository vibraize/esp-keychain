#include "display_anim.h"

#include <U8g2lib.h>
#include <Wire.h>

#include "config.h"
#include "themes.h"

static_assert(SELF_THEME_INDEX < THEME_COUNT && PEER_THEME_INDEX < THEME_COUNT,
              "theme index out of range, check config.h / themes.h");

namespace anim {
namespace {

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(OLED_FLIP ? U8G2_R2 : U8G2_R0,
                                            U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

const Theme& self = THEMES[SELF_THEME_INDEX];
const Theme& peer = THEMES[PEER_THEME_INDEX];

enum State : uint8_t { ST_BOOT, ST_IDLE, ST_BLANK, ST_FAR, ST_NEAR, ST_CLOSE };
State state = ST_BOOT;
uint32_t stateSinceMs = 0;
uint32_t nextFrameMs = 0;
uint8_t contrast = 0;
bool panelOn = true;

void setContrast(uint8_t c) {
  if (c == contrast) return;
  contrast = c;
  u8g2.setContrast(c);
}

void setPanel(bool on) {
  if (on == panelOn) return;
  panelOn = on;
  u8g2.setPowerSave(on ? 0 : 1);
}

// 0..1 triangle-ish smooth wave, period in ms
float wave(uint32_t clk, uint32_t period) {
  return 0.5f - 0.5f * cosf((clk % period) * (TWO_PI / period));
}

void drawSprite(const Theme& t, int x, int y, uint32_t clk) {
  uint8_t f = (clk / t.frameMs) % t.frameCount;
  u8g2.drawXBMP(x, y, t.w, t.h, t.frames[f]);
}

// Signal bars in the top-right corner, n = 0..3
void drawBars(uint8_t n) {
  for (uint8_t i = 0; i < 3; i++) {
    int h = 3 + i * 3;
    int x = 116 + i * 4;
    if (i < n) u8g2.drawBox(x, 10 - h, 3, h);
    else u8g2.drawFrame(x, 10 - h, 3, h);
  }
}

void drawBoot() {
  u8g2.setFont(u8g2_font_6x10_tf);
  drawSprite(self, 4, (32 - self.h) / 2, millis());
  u8g2.drawStr(36, 14, self.name);
  char buf[12];
  snprintf(buf, sizeof(buf), "ID %d", DEVICE_ID);
  u8g2.drawStr(36, 27, buf);
}

void drawIdle(uint32_t clk) {
  drawSprite(self, (128 - self.w) / 2, (32 - self.h) / 2, clk);
}

void drawFar(uint32_t clk) {
  if ((clk / 600) % 2 == 0) drawSprite(self, (128 - self.w) / 2, (32 - self.h) / 2, clk);
  drawBars(1);
}

void drawNear(uint32_t clk) {
  float p = wave(clk, 1200);
  int cx = 64, cy = 16;
  drawSprite(self, cx - self.w / 2, cy - self.h / 2, clk);
  // Expanding rings so the pulse reads even if contrast changes are subtle.
  int r = self.w / 2 + 2 + (int)(p * 14);
  u8g2.drawCircle(cx, cy, r);
  if (r > self.w / 2 + 8) u8g2.drawCircle(cx, cy, r - 6);
  drawBars(2);
  setContrast(OLED_CONTRAST_MIN + (uint8_t)(p * (OLED_CONTRAST_MAX - OLED_CONTRAST_MIN)));
}

void drawClose(uint32_t clk) {
  // Every 6 s, show the theme message for 1.5 s.
  if ((clk % 6000) > 4500) {
    u8g2.setFont(u8g2_font_6x10_tf);
    int w = u8g2.getStrWidth(self.closeMessage);
    u8g2.drawStr((128 - w) / 2, 20, self.closeMessage);
    return;
  }

  // Own sprite on the left, peer's on the right, swaying toward each other.
  float s = wave(clk, 1600);
  int bobA = (int)(wave(clk, 500) * (32 - self.h));
  int bobB = (int)(wave(clk + 250, 500) * (32 - peer.h));
  int ax = 8 + (int)(s * 14);
  int bx = 120 - peer.w - (int)(s * 14);
  drawSprite(self, ax, bobA, clk);
  drawSprite(peer, bx, bobB, clk);

  // Rising sparkles between them. Deterministic from clk so synced units match.
  for (uint8_t i = 0; i < 6; i++) {
    int x = 48 + (i * 37) % 32;
    int y = 31 - (int)((clk / 25 + i * 11) % 34);
    x += (int)(2.0f * sinf((clk + i * 300) * 0.006f));
    if ((i + clk / 200) % 3 == 0) {
      u8g2.drawPixel(x, y);
      u8g2.drawPixel(x - 1, y);
      u8g2.drawPixel(x + 1, y);
      u8g2.drawPixel(x, y - 1);
      u8g2.drawPixel(x, y + 1);
    } else {
      u8g2.drawPixel(x, y);
    }
  }
}

State stateFor(Zone z) {
  switch (z) {
    case ZONE_FAR: return ST_FAR;
    case ZONE_NEAR: return ST_NEAR;
    case ZONE_CLOSE: return ST_CLOSE;
    default: return ST_IDLE;
  }
}

void enter(State s, uint32_t now) {
  state = s;
  stateSinceMs = now;
  nextFrameMs = now;  // redraw immediately on change
  switch (s) {
    case ST_BLANK: setPanel(false); break;
    case ST_IDLE: setPanel(true); setContrast(OLED_CONTRAST_MIN); break;
    case ST_FAR: setPanel(true); setContrast(OLED_CONTRAST_MAX / 2); break;
    case ST_NEAR: setPanel(true); break;  // contrast animated per frame
    default: setPanel(true); setContrast(OLED_CONTRAST_MAX); break;
  }
}

}  // namespace

void begin() {
#if DEBUG_I2C_SCAN
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.print("# I2C devices:");
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " none (check SDA/SCL wiring)");
#endif
  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);  // U8g2 takes the 8-bit form
  u8g2.setBusClock(OLED_I2C_HZ);
  u8g2.begin();
  contrast = 0;
  setContrast(OLED_CONTRAST_MAX);
  enter(ST_BOOT, millis());
}

void update(uint32_t now, uint32_t clk, Zone zone) {
  // State transitions
  if (state == ST_BOOT) {
    if (now - stateSinceMs >= BOOT_SPLASH_MS) enter(stateFor(zone), now);
  } else {
    State want = stateFor(zone);
    bool idleFamily = (state == ST_IDLE || state == ST_BLANK);
    if (want == ST_IDLE) {
      if (!idleFamily) enter(ST_IDLE, now);
      else if (state == ST_IDLE && now - stateSinceMs >= OLED_IDLE_BLANK_MS) enter(ST_BLANK, now);
    } else if (want != state) {
      enter(want, now);
    }
  }

  if (state == ST_BLANK) return;  // panel off, no I2C traffic
  if ((int32_t)(now - nextFrameMs) < 0) return;
  nextFrameMs = now + ANIM_FRAME_MS;

  u8g2.clearBuffer();
  switch (state) {
    case ST_BOOT: drawBoot(); break;
    case ST_IDLE: drawIdle(clk); break;
    case ST_FAR: drawFar(clk); break;
    case ST_NEAR: drawNear(clk); break;
    case ST_CLOSE: drawClose(clk); break;
    default: break;
  }
  u8g2.sendBuffer();
}

}  // namespace anim
