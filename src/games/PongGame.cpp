#include "Games.h"
#include "GameUtils.h"

// ---------------------------------------------------------------------------
// PONG -- a deepened two-paddle duel against a CPU opponent.
//
// Player paddle is on the left, CPU on the right. Move: Up/Left raise the
// paddle, Down/Right lower it (the engine folds Up->LEFT, Down->RIGHT).
// SELECT/Enter/Space fires a CHARGE DASH: when the meter is full the paddle
// lunges, snapping to the ball and launching a fast, low-spin "smash" return.
//
// Depth added on top of classic Pong:
//   * Rally-based ball acceleration plus contact-point spin.
//   * Charge meter that fills on rallies and is spent on a smash return.
//   * Drifting POWER-UPS that the ball can collect mid-court:
//       G grow paddle, S slow time, F fast/curve ball, + extra point gift.
//   * Serve countdown between points, match point banner, CPU "thinks".
//   * First to 7 wins; difficulty tunes CPU reaction speed and tracking error.
//
// Persistent best (SD key "pong") = the longest single rally achieved, shown
// as a HI tag and flagged NEW BEST via resultScreenBest.
// ---------------------------------------------------------------------------

namespace {

const int16_t kW = 128;
const int16_t kH = 64;
const int16_t kTop = 10;            // play area top (HUD lives above)
const int16_t kBottom = kH - 1;     // play area bottom
const int16_t kPadH = 14;           // base paddle height
const int16_t kPadHBig = 22;        // grown paddle height
const int16_t kPadW = 3;            // paddle thickness
const int16_t kBall = 3;            // ball size
const int16_t kPlayerX = 3;         // left paddle x
const int16_t kCpuX = kW - 3 - kPadW;
const int16_t kWinScore = 7;
const int16_t kFP = 16;             // fixed-point shift for ball state

struct CpuTune {
  int16_t speed;     // max pixels CPU paddle moves per frame
  int16_t error;     // random aim jitter added to its target (px)
  int16_t deadzone;  // ignore the ball until it is within this x of the CPU
  uint8_t lag;       // frames between AI aim refreshes
};

// Easy / Normal / Hard
const CpuTune kTune[3] = {
  {2, 16, 38, 4},
  {3, 9, 72, 3},
  {4, 3, 128, 2},
};

struct Ball {
  int32_t x, y;    // top-left, fixed point
  int32_t vx, vy;  // fixed point per frame
};

// Power-up kinds. NONE marks an empty/inactive drop.
enum PowerKind : uint8_t { PK_NONE = 0, PK_GROW, PK_SLOW, PK_FAST, PK_POINT, PK_COUNT };

struct Power {
  int16_t x, y;     // pixel position (drifts vertically)
  int8_t vy;        // drift speed
  PowerKind kind;
  bool active;
};

int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

char powerGlyph(PowerKind k) {
  switch (k) {
    case PK_GROW: return 'G';
    case PK_SLOW: return 'S';
    case PK_FAST: return 'F';
    case PK_POINT: return '+';
    default: return '?';
  }
}

uint16_t powerColor(PowerKind k) {
  switch (k) {
    case PK_GROW: return GAMER_ACCENT;
    case PK_SLOW: return GAMER_WHITE;
    case PK_FAST: return GAMER_INVERSE;
    case PK_POINT: return GAMER_ACCENT;
    default: return GAMER_DIM;
  }
}

void serveBall(Ball& b, int8_t towardCpu, int16_t baseSpeed) {
  b.x = (int32_t)(kW / 2 - kBall / 2) * kFP;
  b.y = (int32_t)(kTop + (kH - kTop) / 2) * kFP;
  int32_t vx = (int32_t)baseSpeed;
  b.vx = towardCpu ? vx : -vx;
  b.vy = ((random(0, 2) == 0) ? -1 : 1) * (int32_t)(kFP / 2 + random(0, kFP / 2));
}

void drawCourt(GamerEngine& engine, int16_t shake) {
  CardputerGameDisplay& g = engine.screen();
  for (int16_t y = kTop; y < kH; y += 6) {
    g.fillRect(kW / 2 - 1 + shake, y, 2, 3, GAMER_DIM);
  }
  g.drawFastHLine(0, kTop - 1, kW, GAMER_DIM);
}

// Top HUD: player score | rally + charge meter | cpu score.
void drawHud(GamerEngine& engine, int playerScore, int cpuScore, int rally,
             uint8_t charge, bool chargeReady) {
  CardputerGameDisplay& g = engine.screen();
  char buf[8];
  g.setTextSize(1);

  // scores flanking the net
  g.setTextColor(GAMER_WHITE);
  snprintf(buf, sizeof(buf), "%d", playerScore);
  g.setCursor(kW / 2 - 14, 1);
  g.print(buf);
  snprintf(buf, sizeof(buf), "%d", cpuScore);
  g.setCursor(kW / 2 + 9, 1);
  g.print(buf);

  // rally counter, top-left
  snprintf(buf, sizeof(buf), "x%d", rally);
  g.setTextColor(rally >= 6 ? GAMER_INVERSE : GAMER_ACCENT);
  g.setCursor(2, 1);
  g.print(buf);

  // charge meter, top-right (8 segments)
  const int16_t mx = kW - 28;
  const int16_t my = 2;
  g.drawRect(mx, my, 26, 6, GAMER_DIM);
  int16_t fill = (int16_t)charge * 24 / 100;
  if (fill > 0) {
    g.fillRect(mx + 1, my + 1, fill, 4, chargeReady ? GAMER_INVERSE : GAMER_ACCENT);
  }
}

// Reflect the ball off a paddle, applying spin and a rally speed ramp.
// `smash` boosts horizontal speed and flattens spin (a charged return).
void paddleBounce(Ball& b, int16_t padY, int16_t padH, bool fromLeft,
                  int16_t rally, bool smash) {
  int32_t ballCenter = b.y + (kBall * kFP) / 2;
  int32_t padCenter = (int32_t)(padY + padH / 2) * kFP;
  int32_t off = ballCenter - padCenter;
  int32_t spin = smash ? off / 8 : off / 4;

  int32_t baseVx = kFP * 2 + clampi(rally, 0, 14) * (kFP / 4);
  if (smash) baseVx += kFP * 2;
  baseVx = clampi(baseVx, kFP, kFP * 7);
  b.vx = fromLeft ? baseVx : -baseVx;
  b.vy = clampi(b.vy + spin, -(int32_t)(kFP * 4), (int32_t)(kFP * 4));
  if (b.vy > -kFP / 4 && b.vy < kFP / 4) b.vy = (off >= 0) ? kFP / 2 : -kFP / 2;
}

// Brief blocking serve countdown drawn over the frozen court. Returns false if
// the player exits during the count.
bool serveCountdown(GamerEngine& engine, int playerScore, int cpuScore,
                    bool matchPoint) {
  for (int n = 3; n >= 1; --n) {
    uint32_t until = millis() + 500;
    while (millis() < until) {
      engine.tick();
      if (engine.shouldExitGame()) return false;
      engine.clear();
      drawCourt(engine, 0);
      drawHud(engine, playerScore, cpuScore, 0, 0, false);
      char c[2] = {(char)('0' + n), 0};
      engine.centerText(c, kH / 2 - 8, 2, GAMER_WHITE);
      if (matchPoint) {
        engine.centerText("MATCH POINT", kH / 2 + 10, 1, GAMER_INVERSE);
      }
      engine.show();
      delay(8);
    }
    engine.playSound(SOUND_UI_MOVE);
  }
  engine.playSound(SOUND_TIMER_GO);
  return true;
}

}  // namespace

void runPong(GamerEngine& engine) {
  const char* kKey = "pong";

  while (true) {
    uint8_t diff = chooseDifficulty(engine, "PONG", "Charge=SELECT  to 7");
    if (diff == 255) {
      engine.waitForRelease();
      return;
    }
    const CpuTune& cpu = kTune[diff];
    const int16_t serveSpeed = kFP * 2;

    uint16_t best = bestLoad(kKey);

    int playerScore = 0;
    int cpuScore = 0;
    int rally = 0;
    int bestRally = 0;
    int16_t playerY = (kTop + kH) / 2 - kPadH / 2;
    int16_t cpuY = playerY;

    // Power-up / ability state.
    uint8_t charge = 0;         // 0..100, fills on hits
    uint8_t dashFrames = 0;     // active charge-dash window
    uint8_t growTimer = 0;      // frames of grown player paddle
    uint8_t slowTimer = 0;      // frames of slow-motion
    Power drop;                 // single drifting power-up
    drop.active = false;
    drop.kind = PK_NONE;
    uint8_t dropCooldown = 80;  // frames until the next drop may appear

    Ball ball;
    SparkField sparks;
    uint8_t shake = 0;
    int16_t cpuAim = cpuY;
    uint8_t aimTimer = 0;
    bool matchOver = false;
    bool playerWon = false;
    uint32_t nextFrame = 0;

    // First serve (with countdown).
    if (!serveCountdown(engine, playerScore, cpuScore, false)) {
      engine.waitForRelease();
      return;
    }
    serveBall(ball, random(0, 2), serveSpeed);

    // --- match loop ---------------------------------------------------------
    while (!matchOver) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }

      bool slow = slowTimer > 0;
      uint16_t frameMs = slow ? 34 : 24;
      if (!frameDue(nextFrame, frameMs)) {
        delay(2);
        continue;
      }

      int16_t curPadH = growTimer > 0 ? kPadHBig : kPadH;

      // --- player input ---
      int8_t axis = heldAxis(engine);
      int16_t moveStep = dashFrames > 0 ? 9 : 4;  // dash also moves faster
      playerY += axis * moveStep;
      playerY = clampi(playerY, kTop, kH - curPadH);

      // fire charge-dash on a SELECT tap when the meter is full
      if (selectTap(engine) && charge >= 100 && dashFrames == 0) {
        dashFrames = 7;
        charge = 0;
        engine.playSound(SOUND_POWERUP);
        engine.ledPulse(CRGB::Aqua, 90);
        sparksSpawn(sparks, kPlayerX + kPadW + 2, playerY + curPadH / 2,
                    GAMER_ACCENT, 10);
      }
      bool dashing = dashFrames > 0;
      if (dashFrames > 0) dashFrames--;

      // --- CPU AI ---
      if (aimTimer == 0) {
        bool incoming = ball.vx > 0;
        int16_t ballX = ball.x / kFP;
        if (incoming && (kCpuX - ballX) <= cpu.deadzone) {
          int16_t target = ball.y / kFP + kBall / 2 - kPadH / 2;
          cpuAim = target + (int16_t)random(-cpu.error, cpu.error + 1);
        } else {
          cpuAim = (kTop + kH) / 2 - kPadH / 2;
        }
        aimTimer = cpu.lag;
      } else {
        aimTimer--;
      }
      if (cpuY < cpuAim) cpuY += min<int16_t>(cpu.speed, cpuAim - cpuY);
      else if (cpuY > cpuAim) cpuY -= min<int16_t>(cpu.speed, cpuY - cpuAim);
      cpuY = clampi(cpuY, kTop, kH - kPadH);

      // --- advance ball ---
      ball.x += ball.vx;
      ball.y += ball.vy;

      int16_t bx = ball.x / kFP;
      int16_t by = ball.y / kFP;
      bool wallHit = false;

      // top / bottom walls
      if (by <= kTop) {
        ball.y = (int32_t)kTop * kFP;
        ball.vy = -ball.vy;
        wallHit = true;
        sparksSpawn(sparks, bx + kBall / 2, kTop, GAMER_DIM, 4);
      } else if (by + kBall >= kBottom) {
        ball.y = (int32_t)(kBottom - kBall) * kFP;
        ball.vy = -ball.vy;
        wallHit = true;
        sparksSpawn(sparks, bx + kBall / 2, kBottom, GAMER_DIM, 4);
      }

      // player paddle (left), only when moving left
      if (ball.vx < 0 && bx <= kPlayerX + kPadW && bx + kBall >= kPlayerX &&
          by + kBall >= playerY && by <= playerY + curPadH) {
        ball.x = (int32_t)(kPlayerX + kPadW) * kFP;
        rally++;
        if (rally > bestRally) bestRally = rally;
        bool smash = dashing;
        paddleBounce(ball, playerY, curPadH, true, rally, smash);
        sparksSpawn(sparks, kPlayerX + kPadW + 2, by + kBall / 2,
                    smash ? GAMER_INVERSE : GAMER_ACCENT, smash ? 12 : 8);
        engine.playSound(smash ? SOUND_COMBO : (rally % 5 == 0 ? SOUND_COMBO : SOUND_HIT));
        engine.ledPulse(smash ? CRGB::White : CRGB::Aqua, 50);
        if (charge < 100) {
          charge = (uint8_t)min<int16_t>(100, charge + (smash ? 0 : 20));
          if (charge >= 100) engine.playSound(SOUND_POWERUP);
        }
      }
      // cpu paddle (right), only when moving right
      else if (ball.vx > 0 && bx + kBall >= kCpuX && bx <= kCpuX + kPadW &&
               by + kBall >= cpuY && by <= cpuY + kPadH) {
        ball.x = (int32_t)(kCpuX - kBall) * kFP;
        rally++;
        if (rally > bestRally) bestRally = rally;
        paddleBounce(ball, cpuY, kPadH, false, rally, false);
        sparksSpawn(sparks, kCpuX - 2, by + kBall / 2, GAMER_INVERSE, 8);
        engine.playSound(SOUND_HIT);
        engine.ledPulse(CRGB::Orange, 40);
      }

      // --- power-up drop spawn / drift / collect ---
      if (!drop.active) {
        if (dropCooldown > 0) {
          dropCooldown--;
        } else if (rally >= 2 && random(0, 100) < 3) {
          drop.active = true;
          drop.kind = (PowerKind)random(PK_GROW, PK_COUNT);
          drop.x = kW / 2 - 3 + (int16_t)random(-10, 11);
          drop.y = kTop + 4 + (int16_t)random(0, kH - kTop - 12);
          drop.vy = (random(0, 2) == 0) ? 1 : -1;
        }
      } else {
        drop.y += drop.vy;
        if (drop.y <= kTop + 1 || drop.y + 7 >= kBottom) drop.vy = -drop.vy;
        // ball collects it
        if (rectsOverlap(ball.x / kFP, ball.y / kFP, kBall, kBall,
                         drop.x, drop.y, 7, 7)) {
          PowerKind got = drop.kind;
          drop.active = false;
          drop.kind = PK_NONE;
          dropCooldown = 90;
          sparksSpawn(sparks, drop.x + 3, drop.y + 3, powerColor(got), 12);
          engine.playSound(SOUND_POWERUP);
          engine.ledPulse(CRGB::Magenta, 90);
          switch (got) {
            case PK_GROW: growTimer = 220; break;
            case PK_SLOW: slowTimer = 130; break;
            case PK_FAST:
              ball.vx += (ball.vx > 0 ? kFP : -kFP);
              ball.vy += (ball.vy > 0 ? kFP / 2 : -kFP / 2);
              break;
            case PK_POINT:
              playerScore++;
              engine.playSound(SOUND_SCORE);
              if (playerScore >= kWinScore) { matchOver = true; playerWon = true; }
              break;
            default: break;
          }
        }
      }

      if (growTimer > 0) growTimer--;
      if (slowTimer > 0) slowTimer--;

      // --- scoring ---
      bx = ball.x / kFP;
      if (!matchOver && bx + kBall < 0) {
        cpuScore++;
        rally = 0;
        charge = (uint8_t)(charge / 2);
        shake = 6;
        engine.playSound(SOUND_LOSE);
        engine.ledPulse(CRGB::Red, 220);
        sparksSpawn(sparks, kPlayerX, by + kBall / 2, GAMER_INVERSE, 10);
        if (cpuScore >= kWinScore) { matchOver = true; playerWon = false; }
        else {
          bool mp = (cpuScore == kWinScore - 1) || (playerScore == kWinScore - 1);
          if (!serveCountdown(engine, playerScore, cpuScore, mp)) {
            engine.waitForRelease();
            return;
          }
          serveBall(ball, true, serveSpeed);
          nextFrame = 0;
        }
      } else if (!matchOver && bx > kW) {
        playerScore++;
        rally = 0;
        engine.playSound(SOUND_SCORE);
        engine.ledPulse(CRGB::Green, 180);
        sparksSpawn(sparks, kCpuX, by + kBall / 2, GAMER_ACCENT, 10);
        if (playerScore >= kWinScore) { matchOver = true; playerWon = true; }
        else {
          bool mp = (cpuScore == kWinScore - 1) || (playerScore == kWinScore - 1);
          if (!serveCountdown(engine, playerScore, cpuScore, mp)) {
            engine.waitForRelease();
            return;
          }
          serveBall(ball, false, serveSpeed);
          nextFrame = 0;
        }
      }

      if (wallHit) engine.playSound(SOUND_HIT);
      if (matchOver) break;

      // --- render ---
      int16_t sh = shake > 0 ? shakeOffset(shake) : 0;
      if (shake > 0) shake--;

      engine.clear();
      drawCourt(engine, sh);
      drawHud(engine, playerScore, cpuScore, rally, charge, charge >= 100);
      drawBestTag(engine, best);

      CardputerGameDisplay& g = engine.screen();

      // power-up drop
      if (drop.active) {
        g.drawRect(drop.x + sh, drop.y, 7, 7, powerColor(drop.kind));
        g.setTextSize(1);
        g.setTextColor(powerColor(drop.kind));
        g.setCursor(drop.x + 2 + sh, drop.y);
        char gl[2] = {powerGlyph(drop.kind), 0};
        g.print(gl);
      }

      // paddles (player flashes white while dashing)
      uint16_t playerCol = dashing ? GAMER_WHITE
                         : (charge >= 100 ? GAMER_INVERSE : GAMER_ACCENT);
      g.fillRect(kPlayerX + sh, playerY, kPadW, curPadH, playerCol);
      g.fillRect(kCpuX + sh, cpuY, kPadW, kPadH, GAMER_INVERSE);

      // ball (white; orange when boosted by FAST/slow-mo trail dot)
      int16_t drawX = ball.x / kFP + sh;
      int16_t drawY = ball.y / kFP;
      g.fillRect(drawX, drawY, kBall, kBall, GAMER_WHITE);
      if (slow) g.drawPixel(drawX - (ball.vx > 0 ? 2 : -2), drawY + 1, GAMER_DIM);

      sparksUpdateDraw(sparks, engine);
      engine.show();
    }

    // --- match over ---------------------------------------------------------
    engine.playSound(playerWon ? SOUND_WIN : SOUND_LOSE);
    engine.ledPulse(playerWon ? CRGB::Green : CRGB::Red, 400);

    bool record = bestSubmit(kKey, (uint16_t)bestRally);
    uint16_t shownBest = bestLoad(kKey);

    char title[20];
    snprintf(title, sizeof(title), playerWon ? "YOU WIN %d-%d" : "CPU WINS %d-%d",
             playerScore, cpuScore);

    if (!resultScreenBest(engine, title, bestRally, shownBest, record)) {
      engine.waitForRelease();
      return;
    }
  }
}
