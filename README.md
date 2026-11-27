# IoT Sentinel

ESP32 firmware for observing nearby 2.4 GHz Wi-Fi metadata. It tracks device
MAC addresses, RSSI, channel, frame counts, and signal trends. RSSI distance
and risk scores are rough estimates, not proof of device type or intent.

## Build, upload, and monitor

```sh
make help
make build
make ports
make upload PORT=/dev/ttyUSB0
make monitor PORT=/dev/ttyUSB0
```

With one detected USB serial device, `make upload` and `make monitor`
automatically use it. If more than one device is connected, run `make ports`
and select the ESP32 explicitly, for example `make upload PORT=/dev/ttyUSB0`.
On boards that do not enter the bootloader automatically, hold **BOOT** while
upload begins; release it when the uploader starts connecting. On Linux,
serial-port access may also require membership in the system's serial-device
group, followed by logging out and back in. Upload and monitor use the same
`PORT` value.

The firmware uses passive promiscuous mode and channel hopping by default. To
use periodic Wi-Fi scans instead, change `useSniffer` to `false` near the top
of `src/main.cpp`, then build again. If sniffer startup fails, firmware tries
the scanner automatically.

The sniffer uses a callback because the ESP32 Wi-Fi library calls our code
when a packet arrives. That callback only copies packet details into a small
queue; the main loop processes them later. The queue lock prevents both parts
from changing the queue at the same time.

Set `PORT` to your board's serial port when automatic detection does not select
the correct one. `make run` uploads the firmware and opens the serial monitor.

For a debug session, connect a supported JTAG debugger and run
`make debug ENV=esp32dev`.

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

## Tests and checks

```sh
make test
make check
```

`make test` runs the host-side tests and does not need the ESP32.
Connect the ESP32 by USB and upload the current firmware with
`make upload PORT=/dev/ttyUSB0`. Then run `make simulator`; the Makefile
automatically uses the only detected `/dev/ttyUSB*` or `/dev/ttyACM*` port.
If you have more than one board, choose one explicitly with
`make simulator PORT=/dev/ttyUSB0` (use `make ports` to see port names).
The terminal program reads live device observations from the board and sends
button-like commands back to its firmware. Type a key and press Enter:

- `1` refreshes the terminal screen
- `2` switches between the device list and details
- `3` selects the next detected device
- `4` selects the previous detected device
- `b` returns from help or details to the device list
- `5` marks the selected device known or unknown
- `h` opens help and keeps it visible until `b` returns to the list; `q` quits

The device list and detail screen include a cautious Wi-Fi role guess. The
firmware counts beacon, probe-request, and probe-response frames over time
instead of changing its guess based only on the latest packet. Beacon and
probe-response evidence suggests an access point; probe requests suggest a
client. A network name may also show a keyword clue such as `camera` or
`printer`, but it can be just the Wi-Fi network name, not the detected
device's model. Role confidence depends on repeated matching frames, and
conflicting frame evidence is shown as mixed activity. On Linux, the
simulator can also show a manufacturer hint from the local Nmap
`nmap-mac-prefixes` database; set `NMAP_MAC_PREFIXES` to use a different local
copy. Locally administered MAC addresses are not looked up because they may be
randomized. A manufacturer is not a device type, and none of these hints
confirms a device's identity.

The firmware keeps scanning while the simulator is open. The terminal display
is a readable version of the board's data; it does not draw exact OLED pixels
or create simulated Wi-Fi devices. `make check` runs host tests, builds the
serial terminal program, and builds the firmware for `ENV` (default
`esp32dev`). `make size` reports memory use, and `make clean` removes generated
build files for `ENV`.

RSSI calibration and checks on a physical ESP32, OLED, buttons, and LED are
still needed. The default distance model uses placeholder values until it is
calibrated in the target environment.
