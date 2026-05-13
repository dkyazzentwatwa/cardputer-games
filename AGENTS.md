# AGENTS.md

Guidance for Codex and other coding agents working in this repository.

## Project Purpose

`cardputer-games` is a standalone Arduino CLI firmware project for the M5Stack
Cardputer ADV / Stamp-S3A. It boots directly into a games launcher and keeps the
game runtime separate from AI chat, WiFi/BLE setup, pet mode, journals, or other
features from the original source project.

Keep this repo focused on the Cardputer game launcher.

## Repository Layout

- `cardputer-games.ino` initializes M5Cardputer, reads keyboard/Button A input,
  and owns the main Arduino `setup()` / `loop()`.
- `sketch.yaml` defines the canonical Arduino CLI `adv` profile.
- `src/games/GameScreen.*` implements the launcher screen and game selection.
- `src/games/GamerEngine.*` provides the small game runtime, input queue,
  display wrapper, and game utility APIs.
- `src/games/GamerConfig.h` defines Cardputer display/input constants.
- `src/games/Games.*` declares and registers the game library.
- `src/games/*Game*.cpp` and category files contain individual game logic.

## Build And Flash

Use Arduino CLI as the source of truth.

```bash
arduino-cli compile --profile adv /Users/cypher/Documents/GitHub/cardputer-games
```

Equivalent explicit board profile:

```bash
arduino-cli compile --fqbn 'm5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=cdc,USBMode=hwcdc' /Users/cypher/Documents/GitHub/cardputer-games
```

Before flashing an ESP32-S3 Cardputer, prefer the 1200-baud touch flow on the
current runtime port, then upload to the newly enumerated bootloader port.

```bash
arduino-cli board list
python3 - <<'PY'
import serial, time
port = "/dev/cu.usbmodemXXXX"
s = serial.Serial(port, 1200)
s.close()
time.sleep(2)
PY
arduino-cli board list
arduino-cli upload --profile adv -p /dev/cu.usbmodemYYYY /Users/cypher/Documents/GitHub/cardputer-games
```

Serial monitor:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200
```

## Design Constraints

- Preserve the `128x64` logical game canvas. The Cardputer display is `240x135`,
  but older imported game loops expect the smaller Pico Gamer-style geometry.
- Keep game rendering routed through the `GamerEngine` / `CardputerGameDisplay`
  wrapper so the offscreen `M5Canvas` path can reduce frame flicker.
- Keep the firmware standalone and offline. Do not add network setup, web
  portals, OpenAI/API features, chat modes, or persistent save systems unless
  the user explicitly asks.
- Favor small, predictable game loops over dynamic allocation-heavy designs.
- Keep controls consistent: movement through arrows, `WASD`, or `HJKL`; action
  through `Enter`, `Space`, or `BtnA`; exit through `Delete`, `q`, or `Tab`.
- Avoid broad auto-formatting. Match the existing compact Arduino/C++ style.

## Adding Or Editing Games

1. Add or update the runner function in the appropriate game/category `.cpp`
   file under `src/games/`.
2. Declare the runner in `src/games/Games.h`.
3. Register the game in `GAME_LIBRARY` in `src/games/Games.cpp`.
4. Use `GamerEngine` APIs for drawing, input, timing, and exit checks.
5. Ensure each game regularly services input and honors `engine.shouldExitGame()`.
6. Compile with `arduino-cli compile --profile adv` before calling the change
   complete.

## Smoke Test Expectations

After compile or flash-sensitive changes, verify at least:

- Firmware boots to `CARDPUTER GAMES`.
- Launcher navigation works.
- `Snake` launches and exits cleanly.
- One game from each changed category launches.
- `Delete`, `q`, or `Tab` returns from a game to the launcher.
- Serial breadcrumbs still include boot, reset reason, launch, and return logs.
