// display_anim.h - OLED animation state machine keyed to the proximity zone.
#pragma once

#include <Arduino.h>

#include "proximity.h"

namespace anim {

void begin();
// now = local millis(), clk = animation clock (synced to peer in CLOSE)
void update(uint32_t now, uint32_t clk, Zone zone);

}  // namespace anim
