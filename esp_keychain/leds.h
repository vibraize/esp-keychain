// leds.h - WS2812B effects keyed to the proximity zone.
#pragma once

#include <Arduino.h>

#include "proximity.h"

namespace leds {

void begin();
void update(uint32_t now, uint32_t clk, Zone zone);

}  // namespace leds
