# Hello-World Template

This template is the smallest FreeRTOS-first starting point for the project.

## What it includes

- a single PlatformIO environment for `nodemcu-32s`
- a hello-world edge-enrichment example
- a runtime start path using the local-first helper
- a small task pair that prints a serial hello and heartbeat

## Build

```bash
pio run -d . -e nodemcu-32s-hello
```

## Flash

```bash
pio run -d . -e nodemcu-32s-hello -t upload --upload-port /dev/ttyUSB1
```

## Monitor

```bash
pio device monitor -p /dev/ttyUSB1 -b 115200
```

## Expected output

You should see the runtime start, the hello task pair report itself, and a
heartbeat line that shows the board is alive.
