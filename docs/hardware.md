# Hardware integration notes

## Current firmware pin defaults

| Component | ESP32 pin | Notes |
|---|---:|---|
| SSD1306 SDA | GPIO 21 | I2C |
| SSD1306 SCL | GPIO 22 | I2C |
| NEXT button | GPIO 32 | Active-low, `INPUT_PULLUP` |
| MODE button | GPIO 33 | Active-low, `INPUT_PULLUP` |
| Alert LED | GPIO 2 | Drive through a current-limiting resistor |

The OLED address defaults to `0x3C`. These are firmware defaults; verify the
specific development board and wiring before powering the circuit.
The default build uses passive Wi-Fi metadata monitoring; build the retained
scan-only mode with `pio run -e esp32dev-scan`.

## Semiconductor diagnostics

The calculation API in `semiconductor_diagnostics.h` consumes measured
voltages/currents and resistor values. It does not configure or read ADC pins.
The Diagnostic UI explicitly reports that ADC input is disabled.

The project does not yet specify a transistor test-circuit schematic, ADC pin
map, input protection, or voltage-divider values. Do not connect a transistor
node or an external supply directly to an ESP32 GPIO/ADC input. Define and
verify the analog front-end and ensure every ADC input remains within the
board's permitted input range before adding a hardware measurement adapter.

`BjtAnalysis` assumes an NPN collector resistor connected to the specified
supply. The RSSI distance model also uses provisional defaults
(`RSSI_1m = -50 dBm`, path-loss exponent `n = 2`); replace them using measured
calibration data before interpreting displayed estimates.
