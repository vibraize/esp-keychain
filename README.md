# esp-keychain

Two paired ESP32-C3 keychains that sense each other over ESP-NOW and react with an OLED animation and 3 RGB LEDs. Both run the same firmware; only `DEVICE_ID` in `config.h` differs.

> Status: firmware drafted, **not yet tested on hardware**. See [Untested](#untested--known-unknowns).

## Repo layout

```
esp_keychain/            Arduino sketch folder (name matches the .ino)
  esp_keychain.ino       setup/loop, debug output
  config.h               DEVICE_ID, pins, thresholds, timings, modes
  proximity.h/.cpp       ESP-NOW beacons, RSSI filter, zones, clock sync, RSSI simulator
  display_anim.h/.cpp    OLED state machine and drawing
  leds.h/.cpp            addressable LED effects (PL9823)
  themes.h               per-device sprites, palettes, text
tools/png2xbm.py         convert PNG frames to sprite arrays for themes.h
```

## Hardware

| Part | Notes |
|---|---|
| Seeed Studio XIAO ESP32C3 | Onboard LiPo charger (BAT+/BAT- pads on the underside), USB-C for charging and programming, U.FL external antenna |
| 0.91" 128x32 OLED, SSD1306, I2C | Address assumed 0x3C, confirmed by the boot I2C scan |
| 3x PL9823-F5 5 mm through-hole addressable RGB LEDs (diffused) | Chained DIN -> DOUT, 100 nF cap at each |
| 1S 3.7 V LiPo + 3 mm slide switch | See [Battery sizing](#battery-sizing) |
| Antenna | The one included with the XIAO. **Must be connected**, see notes |

### Wiring

| From | To | Notes |
|---|---|---|
| LiPo + | Slide switch | |
| Slide switch | **Switched rail** -> XIAO `BAT+` pad | Raw LiPo voltage, ~3.0 to 4.2 V |
| LiPo - | XIAO `BAT-` pad | Common ground for everything |
| OLED `VCC` | XIAO `3V3` | Most SSD1306 modules accept 3.3 V; check yours |
| OLED `GND` | XIAO `GND` | |
| OLED `SDA` | `D4` (GPIO6) | |
| OLED `SCL` | `D5` (GPIO7) | |
| PL9823 #1 `VDD` | Switched rail | Same node as `BAT+` |
| PL9823 #1 `GND` | `GND` | |
| PL9823 #1 `DIN` | `D2` (GPIO4) | A 220 to 470 Ω series resistor is common practice |
| PL9823 #1 `DOUT` -> #2 `DIN`, #2 `DOUT` -> #3 `DIN` | | VDD/GND of all three on the switched rail / GND |
| 100 nF ceramic cap | Across `VDD`-`GND` at each PL9823 | Keep it right at the legs |

Notes:
- Running the LEDs from the battery rail keeps 3.3 V data inside the usual WS281x-family logic-high spec of 0.7 x VDD (at most ~2.9 V at 4.2 V). Confirm against the PL9823 datasheet.
- The XIAO's `BAT+`/`BAT-` pads are on the underside. Tin them first, then solder thin wires on quickly so you don't overheat the board.
- **Charging goes through the switch.** With the switch off the battery is disconnected, so it won't charge from USB. Switch on to charge; the unit runs while charging.
- **Antenna:** the XIAO ESP32C3 uses an external antenna on a U.FL connector. Without it, range and RSSI will be very poor. Where the antenna sits in the case changes RSSI, so tune thresholds with the antenna in its final position.
- The LEDs run from the switched battery rail and not from the XIAO `5V` pin. I'm not sure whether that pin is powered when running on battery alone, so I didn't rely on it.
- **Check the charge current** on the Seeed wiki against your cell. A common rule is to charge at no more than 1C (e.g. ≤ 250 mA for a 250 mAh cell). A cell much smaller than the charge current is a bad idea.
- Strapping pins on the XIAO ESP32C3 are `D0` (GPIO2), `D8` (GPIO8) and `D9` (GPIO9). Keep extra wiring off them.
- PL9823 leg order: check the datasheet or seller drawing before soldering. Don't go by leg length alone.
- A ~100 µF cap across the LED supply near LED #1 is common addressable-LED advice, on top of the 100 nF per LED.

### Other through-hole addressable LEDs

The firmware defaults to the PL9823-F5. **WS2812D-F5** and **APA106-F5** (or the 8 mm F8 versions) should also work:

- Each has 4 legs: DIN, VDD, GND, DOUT. The leg order differs between parts and sellers, so check the datasheet for the exact part you buy.
- Unlike pixel boards, these have no built-in capacitor. Put a 100 nF ceramic cap from VDD to GND right at each LED.
- Color order varies. `LED_PIXEL_TYPE` defaults to `(NEO_RGB + NEO_KHZ800)`; if the boot color doesn't match the theme's primary color, try `NEO_GRB`. Flat SMD WS2812B pixels use `NEO_GRB`.
- Check the part's minimum VDD in its datasheet. A LiPo sags to ~3.3 V near empty, so colors may shift or blue may drop out before the board browns out.
- Diffused lenses look softer and more even than clear ones.

## Toolchain

- Arduino IDE 2.x
- Board package: **esp32 by Espressif Systems**, version **3.0.0 or newer** (the code uses the core 3.x `esp_now_recv_info_t` receive callback)
- Board: **XIAO_ESP32C3**
- Tools menu: make sure **USB CDC On Boot** is **Enabled** if the option is shown. Leave other options at defaults.

### Libraries (Library Manager)

| Library Manager name | Author | Minimum version | Why |
|---|---|---|---|
| U8g2 | oliver | 2.34.x | SSD1306 128x32 driver, full-buffer mode |
| Adafruit NeoPixel | Adafruit | 1.12.0 | PL9823 / WS281x-family LEDs. 1.12.0 is the first release with arduino-esp32 3.x support as far as I know |

The minimum versions are from memory and I couldn't check the current releases from here. **TODO:** once you install, write the exact versions you used in this table so you can rebuild the same thing later.

Why NeoPixel and not FastLED: it's only 3 pixels, NeoPixel's ESP32 path goes through the core's own RMT driver, and FastLED's ESP32 RMT backend has had compatibility churn across IDF 5 / core 3.x. FastLED would give nicer color math, but nothing here needs it.

## Flashing

Do this once per keychain.

1. Open `esp_keychain/esp_keychain.ino` in Arduino IDE.
2. In `config.h` set `#define DEVICE_ID 0`.
3. Select **Tools > Board > esp32 > XIAO_ESP32C3**, check **USB CDC On Boot: Enabled**, pick the port.
4. Plug in USB-C. The battery switch can be on or off.
5. Upload. If the upload won't start or the port disappears, hold **B** (BOOT), tap **R** (RESET), release **B**, then upload again.
6. Open the Serial Monitor at 115200 and check the boot lines: `# I2C devices: 0x3C` and `# esp_keychain DEVICE_ID=0`.
7. Put a label on this unit. Change `DEVICE_ID` to `1` and repeat with the second board.
8. When you're done with both, set `DEBUG_SERIAL 0` and `DEBUG_I2C_SCAN 0` if you want quieter output (optional).

## Testing with one board (SIMULATE_RSSI)

1. Set `SIMULATE_RSSI 1` in `config.h` and upload. The radio stays off.
2. Serial Monitor: 115200, line ending **Newline**.
3. Type a dBm value and press Enter, e.g. `-80` (FAR), `-65` (NEAR), `-50` (CLOSE). `60` also works and is read as `-60`.
4. Type `x` to stop the fake packets. After `PEER_TIMEOUT_MS` the unit should drop to OUT_OF_RANGE.
5. Set `SIMULATE_RSSI 0` again before the real flash.

Clock sync is skipped in simulation since there's no peer.

## Tuning guide

Use `DEBUG_SERIAL 1` and **Tools > Serial Plotter**. It plots `raw`, `smooth`, `zone` (drawn at its threshold dBm so it reads as a step line) and the three thresholds as flat lines.

1. **Fix TX power first.** `WIFI_TX_POWER_FIXED 1` with `WIFI_POWER_8_5dBm` keeps RSSI consistent between units. Raising power moves every threshold. 8.5 dBm is a conservative start that saves a bit of power; raise it if range is too short.
2. **Measure real distances.** Hold the units at the distances you want for FAR, NEAR and CLOSE, as they'll actually be carried (in hand, in a pocket, on keys). Read the `smooth` value for each.
3. **Set thresholds** `RSSI_FAR_DBM`, `RSSI_NEAR_DBM`, `RSSI_CLOSE_DBM` just below those readings.
4. **Flickering between zones:** raise `RSSI_HYST_DB` (5 to 8) or `ZONE_MIN_DWELL_MS`.
5. **Too sluggish:** raise `RSSI_EMA_ALPHA` (toward 0.5) or lower `RSSI_MEDIAN_N`.
6. **Too jumpy:** lower `RSSI_EMA_ALPHA` (0.2) or raise `RSSI_MEDIAN_N` (7 to 9). The median kills single-packet spikes, the EMA smooths the rest.
7. **Drops out too easily at range:** raise `PEER_TIMEOUT_MS`. With fast beacons at 150 ms, 2 s means ~13 missed packets in a row.
8. **Battery:** lower `LED_MAX_BRIGHTNESS`, set `LED_FAR_ENABLE` / `LED_NEAR_ENABLE` to 0, shorten `OLED_IDLE_BLANK_MS`, try `CPU_FREQ_MHZ 80`.
9. **OLED upside down:** `OLED_FLIP 1`. **Wrong address:** set `OLED_I2C_ADDR` to what the boot scan prints.
10. **LED chase:** `LED_CHASE_MODE 0` for a loop/triangle layout, `1` (ping-pong) for a row. `LED_CHASE_REVERSE` flips direction.

Body blocking matters a lot at 2.4 GHz. Expect RSSI to shift by 10 dB or more depending on how the keychain is held, so leave generous gaps between thresholds.

## Behavior

| Zone | OLED | LEDs |
|---|---|---|
| OUT_OF_RANGE | dim idle sprite for `OLED_IDLE_BLANK_MS`, then panel off | off |
| FAR | own sprite slow blink, 1 signal bar | middle LED faint blink |
| NEAR | sprite with pulsing rings + contrast pulse, 2 bars | soft breathing |
| CLOSE | own + peer's sprite swaying with sparkles, theme message every 6 s | chase alternating both theme colors |

Beacons go out every `BEACON_SLOW_MS` (500 ms) until the peer is heard, then every `BEACON_FAST_MS` (150 ms), with a small random jitter. In CLOSE, device 1 locks its animation clock to device 0's so both units animate in step.

## Battery sizing

The ESP-NOW receiver is on continuously, which dominates current draw. Rough estimate, **to be measured**:

| Load | Estimate |
|---|---|
| ESP32-C3 with WiFi RX always on | on the order of 80 mA (Espressif datasheet RX figures) |
| OLED 128x32 | a few mA up to ~20 mA depending on lit pixels, ~0 when off |
| 3x PL9823 at `LED_MAX_BRIGHTNESS 40` | ~1 mA each idle, up to ~10 to 15 mA total while animating |

Call it ~90 to 110 mA while active. Runtime ≈ capacity x 0.8 / current:

| LiPo | Runtime estimate |
|---|---|
| 250 mAh | ~2 h |
| 500 mAh | ~4 h |
| 1000 mAh | ~8 h |

LiPo names give size: `503035` = 5.0 mm thick, 30 mm wide, 35 mm long. A matchbox is roughly 50 x 35 x 15 mm, so a ~500 mAh pouch (e.g. 503035 or 603040 class) is likely the largest that fits next to the board. Measure the box before ordering. The XIAO is about 21 x 17.5 mm, and with no separate charger module there is more room for the cell. Buy one with a protection circuit (PCM). The XIAO has solder pads and no JST connector, so either solder the leads directly or add a small JST pigtail.

The slide switch is the real power saver: off in a bag, on when you want them to find each other. Measure actual draw with a multimeter in series with the switch before picking the final cell. The XIAO has no built-in battery voltage reading; a resistor divider into an analog pin can add one later.

## Adding a theme

1. Draw 1 or more frames as black-on-white PNGs, at most 32 px tall (24x24 is the placeholder size).
2. `pip install pillow`, then `python3 tools/png2xbm.py sprite_name frame0.png frame1.png`
3. Paste the output into `themes.h` and add a `Theme` entry to `THEMES[]`.
4. Point `SELF_THEME_INDEX` / `PEER_THEME_INDEX` in `config.h` at it. By default they follow `DEVICE_ID`.

## Untested / known unknowns

- Nothing has run on hardware. The code was syntax-checked on a PC with stub headers that copy the arduino-esp32 3.x, U8g2 and NeoPixel function signatures. That proves it's valid C++, not that the APIs behave as expected on the C3.
- Not compiled against the real esp32 core or libraries.
- OLED I2C address, panel orientation and whether contrast changes are visible on this module.
- `esp_wifi_set_channel` after `WiFi.mode(WIFI_STA)` without associating, which is the usual ESP-NOW pattern but unverified on this board.
- RSSI threshold values: starting points only.
- Calling `Wire.begin()` for the I2C scan before U8g2 calls it again (expected to only log a warning on core 3.x).
- Power path: XIAO battery charging, LEDs on the battery rail, behavior near empty.
- `CPU_FREQ_MHZ 80` with USB CDC.
- Each full OLED frame blocks for roughly 10 to 15 ms of I2C at 400 kHz. Radio receive runs in the WiFi task, so packets aren't lost, but the loop does pause during the transfer.
