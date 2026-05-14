#pragma once

#include "GamerEngine.h"

typedef void (*GameRunner)(GamerEngine& engine);

struct GameEntry {
  const char* title;
  const char* category;
  GameRunner run;
};

void runPong(GamerEngine& engine);
void runSnake(GamerEngine& engine);
void runFullSpeed(GamerEngine& engine);
void runLunarModule(GamerEngine& engine);
void runBreakout(GamerEngine& engine);
void runFlappyPico(GamerEngine& engine);
void runDinoRunner(GamerEngine& engine);
void runJetpackRunner(GamerEngine& engine);
void runDodgeRain(GamerEngine& engine);
void runCatchStar(GamerEngine& engine);
void runBasketCatch(GamerEngine& engine);
void runBalloonPop(GamerEngine& engine);
void runCaveFlyer(GamerEngine& engine);
void runTunnelRunner(GamerEngine& engine);
void runWallBounce(GamerEngine& engine);
void runGravityFlip(GamerEngine& engine);
void runPlatformHopper(GamerEngine& engine);
void runBrickDrop(GamerEngine& engine);
void runLaneRacer(GamerEngine& engine);
void runTrafficDodge(GamerEngine& engine);
void runSkiSlalom(GamerEngine& engine);
void runBoatSlalom(GamerEngine& engine);
void runRailRunner(GamerEngine& engine);
void runRoadDrift(GamerEngine& engine);
void runAsteroidsLite(GamerEngine& engine);
void runInvadersLite(GamerEngine& engine);
void runMissileCommandLite(GamerEngine& engine);
void runTurretDefense(GamerEngine& engine);
void runUfoDefender(GamerEngine& engine);
void runMeteorBlaster(GamerEngine& engine);
void runLightsOut(GamerEngine& engine);
void runMinefield(GamerEngine& engine);
void runSokobanMicro(GamerEngine& engine);
void runSlidingPuzzle(GamerEngine& engine);
void runMemoryMatch(GamerEngine& engine);
void runSimon(GamerEngine& engine);
void runMastermind(GamerEngine& engine);
void runNumberGuess(GamerEngine& engine);
void runTwentyFortyEight(GamerEngine& engine);
void runFloodFill(GamerEngine& engine);
void runBoxPush(GamerEngine& engine);
void runLaserMirror(GamerEngine& engine);
void runTicTacToe(GamerEngine& engine);
void runConnectFour(GamerEngine& engine);
void runNim(GamerEngine& engine);
void runDotsBoxesLite(GamerEngine& engine);
void runReactionTimer(GamerEngine& engine);
void runQuickDraw(GamerEngine& engine);
void runStopTheBar(GamerEngine& engine);
void runStackTower(GamerEngine& engine);
void runLockPick(GamerEngine& engine);
void runPixelWhack(GamerEngine& engine);
void runPulseMatch(GamerEngine& engine);

extern const GameEntry GAME_LIBRARY[];
extern const uint8_t GAME_COUNT;
