# Cypher-Gamer for Cardputer

Standalone Arduino CLI firmware that turns the M5Stack Cardputer ADV into a
boot-to-games handheld. Built by littlehakr, tuned for quick play sessions, and
kept intentionally offline: no chat stack, no WiFi setup, no web portal, no
journal, no cloud account, and no setup wizard.

On boot, the device shows a 4-second `Cypher-Gamer` intro splash, then opens the
`CARDPUTER GAMES` launcher with 53 tiny games ready to play.

## Highlights

- 53 built-in games across arcade, racing, motion, shooter, puzzle, board, and
  reflex categories.
- Full Cardputer ADV / Stamp-S3A target with the canonical Arduino CLI `adv`
  profile in `sketch.yaml`.
- Centered `128x64` logical game canvas mapped onto the Cardputer `240x135`
  ST7789 display for compatibility with Pico Gamer-style loops.
- Flicker-reduced rendering through an offscreen `M5Canvas` path when available.
- Keyboard-first controls with arrows, `WASD`, `HJKL`, `Enter`, `Space`, and
  Button A support.
- Short procedural sound cues with an in-launcher mute toggle.
- Every game deepened with `EASY/NORMAL/HARD` difficulty, lives and difficulty
  curves, combo/power-up mechanics, smarter board-game AI (up to minimax), and
  juice (particles, screen shake, richer cues).
- Persistent high scores saved to the microSD card (`/cpgames.hi`); games still
  play normally with no card inserted.

## Hardware

| Part | Target |
| --- | --- |
| Board | M5Stack Cardputer ADV / Stamp-S3A |
| Display | 240 x 135 ST7789 |
| Input | 56-key TCA8418 keyboard plus Button A |
| Flash | 8 MB |
| Runtime | Standalone offline Arduino firmware |

## Game Library

For player-facing notes on every title, see [GAME_CATALOG.md](GAME_CATALOG.md).

| Category | Games |
| --- | --- |
| Arcade | Pong, Snake, Breakout, Flappy Pico, Dino Runner, Jetpack, Dodge Rain, Catch Star, Basket Catch, Balloon Pop, Cave Flyer, Tunnel Run, Wall Bounce, Gravity Flip, Platform Hop, Brick Drop |
| Racing | Full Speed |
| Skill | Lunar Module |
| Motion | Lane Racer, Traffic Dodge, Ski Slalom, Boat Slalom, Rail Runner, Road Drift |
| Shooter | Asteroids, Invaders, Missile Cmd, Turret Def, UFO Defender, Meteor Blast |
| Puzzle | Lights Out, Minefield, Sokoban, Sliding, Memory, Simon, Mastermind, Number Guess, 2048, Flood Fill, Box Push, Laser Mirror |
| Board | Tic Tac Toe, Connect Four, Nim, Dots Boxes |
| Reflex | Reaction, Quick Draw, Stop Bar, Stack Tower, Lock Pick, Pixel Whack, Pulse Match |

## Controls

| Action | Keys |
| --- | --- |
| Move / navigate | Arrow keys, `WASD`, or `HJKL` |
| Launch / action | `Enter`, `Space`, or Button A |
| Toggle sound | `m` |
| Exit a game | `Delete`, `q`, or `Tab` |

## Build

Install Arduino CLI with the M5Stack ESP32 platform, then compile with the
included profile:

```bash
arduino-cli compile --profile adv /Users/cypher/Documents/GitHub/cardputer-games
```

Equivalent explicit board profile:

```bash
arduino-cli compile --fqbn 'm5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=cdc,USBMode=hwcdc' /Users/cypher/Documents/GitHub/cardputer-games
```

## Flash

For ESP32-S3 Cardputer hardware, use the touch-first bootloader flow. Start from
the current runtime port, trigger 1200-baud reset, then upload to the newly
enumerated bootloader port.

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

Monitor after upload:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200
```

## Project Layout

```text
cardputer-games.ino        Arduino setup, loop, input bridge, and boot splash
sketch.yaml                Canonical Arduino CLI adv profile
src/games/GameScreen.*     Launcher UI and game selection
src/games/GamerEngine.*    Runtime, input queue, display wrapper, sound hooks
src/games/GamerConfig.h    Cardputer display and input constants
src/games/Games.*          Game registry and category catalog
src/games/*Game*.cpp       Individual games and category implementations
```

## Design Notes

`Cypher-Gamer` preserves the small `128x64` logical canvas even though the
Cardputer display is larger. That keeps the imported game loops predictable and
lets the firmware scale the play area cleanly into the physical display.

The firmware is deliberately narrow: it is a games launcher, not a general
Cardputer dashboard. Network setup, BLE tools, AI chat, web portals, persistent
save systems, and other non-game modes are outside this release.

## Smoke Test

After flashing a release build:

1. Confirm the `Cypher-Gamer` splash is readable and lasts about 4 seconds.
2. Confirm the launcher opens to `CARDPUTER GAMES`.
3. Scroll through all 53 games.
4. Launch `Snake` first.
5. Launch one game from each category.
6. Confirm movement and action keys work.
7. Confirm sound cues play at a conservative volume.
8. Confirm `m` toggles sound in the launcher and inside a game.
9. Confirm `Delete`, `q`, or `Tab` exits back to the launcher.

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
