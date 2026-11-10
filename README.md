# IoT Sentinel

ESP32 firmware for observing nearby 2.4 GHz Wi-Fi metadata. It tracks device
MAC addresses, RSSI, channel, frame counts, and signal trends. RSSI distance
and risk scores are rough estimates, not proof of device type or intent.

## Build and run

```sh
pio run
pio run -e esp32dev-scan
pio upload
pio device monitor
```

The default `esp32dev` build uses passive promiscuous mode and channel hopping.
The `esp32dev-scan` build uses periodic access-point scans instead.

## Project layout

- `include/` — shared data types and module interfaces
- `src/main.cpp` — firmware setup and main loop
- `src/` — scanner, sniffer, device tracking, filtering, UI, and risk modules
- `test/native/` — small host tests for logic that does not need an ESP32
- `docs/hardware.md` — current pin defaults

## Wiring defaults

| Part | ESP32 pin |
|---|---:|
| SSD1306 SDA | 21 |
| SSD1306 SCL | 22 |
| NEXT button | 32 |
| MODE button | 33 |
| Alert LED | 2 |

Buttons connect to ground when pressed. Put a current-limiting resistor in
series with the LED. Check the board and module before wiring.

## Host tests

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Itest/native -Iinclude \
  test/native/test_core.cpp \
  src/rssi_filter.cpp src/distance_estimator.cpp \
  src/device_manager.cpp src/risk_engine.cpp \
  -o /tmp/iot-sentinel-core-test
/tmp/iot-sentinel-core-test
```

RSSI calibration and checks on a physical ESP32, OLED, buttons, and LED are
still needed. The default distance model uses placeholder values until it is
calibrated in the target environment.
