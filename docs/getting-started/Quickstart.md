# FreeRTOS First Onboarding Quickstart

This quickstart is the shortest path for a new ESP32 user who wants to see the
framework run on real hardware.

## What you get

- a hello-world edge-enrichment example
- a minimal PlatformIO build target
- a serial heartbeat that proves the board is running

## Prerequisites

- ESP32 NodeMCU-32S board
- USB serial cable
- PlatformIO installed
- access to the serial port used by the board

## Build

```bash
pio run -d . -e nodemcu-32s-hello
```

If you want to compare against the richer demos:

```bash
pio run -d . -e nodemcu-32s-happy
pio run -d . -e nodemcu-32s-advanced
```

## Flash

```bash
pio run -d . -e nodemcu-32s-hello -t upload --upload-port /dev/ttyUSB1
```

If your board appears on another port, replace `/dev/ttyUSB1` with the
correct device path.

## Monitor

```bash
pio device monitor -p /dev/ttyUSB1 -b 115200
```

## What you should see

The board should print:

- a runtime start line
- a hello message from the client task
- a reply line from the server task
- a heartbeat line every few seconds

If the output is missing, check the upload port and confirm the board is
powered and reset after flashing.

## Switching demos

- Use `nodemcu-32s-hello` for the shortest onboarding path.
- Use `nodemcu-32s-happy` for the standard friendly demo.
- Use `nodemcu-32s-advanced` for the explicit public-contract runtime path.
