# Contributor Setup

This page is the shortest path for someone who wants to clone the repo, build it, flash it, and run the current board-backed checks.

## 1. Clone

```bash
git clone https://github.com/jaimebv/Edge-aware-Tasks-RTOS.git
cd Edge-aware-Tasks-RTOS
```

## 2. Build

```bash
pio run -d . -e nodemcu-32s
```

## 3. Flash

```bash
pio run -d . -e nodemcu-32s -t upload
```

## 4. Monitor

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## 5. Test

- Run the project tests that match the module you changed.
- Prefer the board-backed harness for runtime, task lifecycle, and port-layer changes.
- Keep the PR focused on one change set so verification stays readable.

## Practical notes

- Build failures should be fixed before a pull request is opened.
- Board or serial regressions should be verified on the hardware before merge.
- If a change touches public behavior, update the docs and the changelog in the same PR.

