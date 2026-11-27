.DEFAULT_GOAL := help

PIO ?= pio
CXX ?= g++
ENV ?= esp32dev
PORT ?=
NATIVE_TEST := .build/test_core
SIMULATOR := .build/simulator

USB_PORTS := $(wildcard /dev/ttyUSB* /dev/ttyACM*)
SERIAL_PORT = $(if $(strip $(PORT)),$(PORT),$(if $(filter 1,$(words $(USB_PORTS))),$(firstword $(USB_PORTS)),))
UPLOAD_PORT = $(if $(strip $(SERIAL_PORT)),--upload-port $(SERIAL_PORT),)
MONITOR_PORT = $(if $(strip $(SERIAL_PORT)),--port $(SERIAL_PORT),)
SIMULATOR_PORT = $(SERIAL_PORT)

.PHONY: help build upload monitor run debug size test simulator check ports clean serial-port-check

help:
	@printf '%s\n' \
	  'Targets:' \
	  '  make build          Build the firmware' \
	  '  make upload         Upload default firmware (ENV=esp32dev)' \
	  '  make run            Upload default firmware, then open serial monitor' \
	  '  make monitor        Open serial monitor' \
	  '  make debug          Start PlatformIO debugger for ENV' \
	  '  make size           Show firmware memory usage' \
	  '  make test           Build and run host-side logic tests' \
	  '  make simulator      Show live ESP32 data in the terminal' \
	  '  make check          Run tests and build firmware for ENV' \
	  '  make ports          List serial ports' \
	  '  make clean          Remove PlatformIO and host test build files' \
	  '' \
	  'Options: ENV=esp32dev (build environment)' \
	  '         PORT=/dev/ttyUSB0 (select serial port; auto-selects one detected port)'

build:
	$(PIO) run -e $(ENV)

upload: serial-port-check
	$(PIO) run -e $(ENV) -t upload $(UPLOAD_PORT)

monitor: serial-port-check
	$(PIO) device monitor $(MONITOR_PORT)

run: upload
	$(PIO) device monitor $(MONITOR_PORT)

debug:
	$(PIO) debug -e $(ENV)

size:
	$(PIO) run -e $(ENV) -t size

$(NATIVE_TEST): test/native/test_core.cpp src/rssi_filter.cpp \
                src/distance_estimator.cpp src/device_manager.cpp \
                src/risk_engine.cpp include/device_manager.h \
                include/distance_estimator.h include/rssi_filter.h \
                include/risk_engine.h include/radio_observation.h \
                test/native/Arduino.h
	mkdir -p .build
	$(CXX) -std=c++11 -Wall -Wextra -Werror \
	  -Itest/native -Iinclude \
	  test/native/test_core.cpp \
	  src/rssi_filter.cpp src/distance_estimator.cpp \
	  src/device_manager.cpp src/risk_engine.cpp \
	  -o $(NATIVE_TEST)

test: $(NATIVE_TEST)
	./$(NATIVE_TEST)

$(SIMULATOR): test/native/simulator.cpp
	mkdir -p .build
	$(CXX) -std=c++11 -Wall -Wextra -Werror \
	  test/native/simulator.cpp \
	  -o $(SIMULATOR)

simulator: $(SIMULATOR)
ifeq ($(strip $(SIMULATOR_PORT)),)
	@echo "Could not choose one USB serial port automatically."
	@echo "Detected ports: $(if $(USB_PORTS),$(USB_PORTS),none)"
	@echo "Run 'make ports', then use: make simulator PORT=/dev/ttyUSB0"
	@exit 1
else
	./$(SIMULATOR) $(SIMULATOR_PORT)
endif

serial-port-check:
ifeq ($(strip $(PORT)),)
ifneq ($(strip $(USB_PORTS)),)
ifneq ($(words $(USB_PORTS)),1)
	@echo "Multiple USB serial ports detected: $(USB_PORTS)"
	@echo "Choose the ESP32 explicitly with PORT=/dev/ttyUSB0 (see 'make ports')."
	@exit 1
endif
endif
endif

check: test $(SIMULATOR)
	$(PIO) run -e $(ENV)

ports:
	$(PIO) device list

clean:
	$(PIO) run -e $(ENV) -t clean
	rm -rf .build
