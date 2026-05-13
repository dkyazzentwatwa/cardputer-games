# Cardputer Games

Standalone Arduino CLI firmware for running the Cardputer game launcher only.
This folder was extracted from `cardputer-bot` so the games can be tuned without
the AI chat, pet, web portal, WiFi/BLE, journal, OpenAI, or setup-wizard code.

## Hardware

- M5Stack Cardputer ADV / Stamp-S3A
- 240 x 135 ST7789 display
- 56-key TCA8418 keyboard
- 8 MB flash

The game engine keeps the original Pico Gamer-style `128x64` logical canvas and
maps it into the Cardputer display. Keep that shield in place when tuning older
game loops; exposing the full `240x135` geometry can make imported games reset.
Game frames render through an offscreen `M5Canvas` when available, then push into
the centered viewport to keep animation smoother.

## Build

```bash
arduino-cli compile --profile adv /Users/cypher/Documents/GitHub/cardputer-games
```

Equivalent explicit board profile:

```bash
arduino-cli compile --fqbn 'm5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=cdc,USBMode=hwcdc' /Users/cypher/Documents/GitHub/cardputer-games
```

## Flash

Use the touch-first ESP32-S3 flow:

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

Monitor:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200
```

## Controls

- Move through the launcher: arrows, `WASD`, or `HJKL`
- Launch or act: `Enter`, `Space`, or `BtnA`
- Toggle sound: `m`
- Exit a running game: `Delete`, `q`, or `Tab`

The firmware shows a 4-second `Cypher-Gamer` intro splash by littlehakr, then
boots directly into `CARDPUTER GAMES`. There is no home dashboard, app launcher,
network setup, chat state, or persistent save state in v1.
Sound uses short procedural Cardputer speaker tones only; there are no embedded
audio assets or background music.

## Smoke Test

After flashing:

1. Confirm the device boots directly into the games launcher.
2. Confirm the `Cypher-Gamer` splash is readable and lasts about 4 seconds.
3. Scroll through all 52 games.
4. Launch `Snake` first.
5. Launch one game from each category.
6. Confirm movement and action keys work.
7. Confirm launcher/game event sounds play at a conservative volume.
8. Confirm `m` toggles sound in the launcher and inside a game.
9. Confirm `Delete`, `q`, or `Tab` exits back to the games launcher.

Useful serial breadcrumbs:

```text
cardputer-games boot
reset reason=<reason> heap=<bytes>
intro splash start
intro splash done
game launch index=<n> title="<title>" category="<category>"
game returned index=<n> title="<title>" heap=<bytes>
sound muted=<true|false>
```
