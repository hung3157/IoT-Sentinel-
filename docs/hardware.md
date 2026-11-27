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
specific development board and wiring before powering the circuit. Passive
Wi-Fi metadata monitoring is enabled by default. Set `useSniffer` to `false`
in `src/main.cpp` to use periodic scans.

Connect the alert LED through a current-limiting resistor. Test the screen,
buttons, and LED on the actual board before using the handheld UI.
