#pragma once
#include "raylib.h"

// if you want to expose these enums to other files later, keep them here
typedef enum {
    STATION,
    SHOP,
    MISSION,
    GAMEOVER,
    PAUSE,
    QUIT
} GameState;

// optional, but you already had it
typedef enum {
    EARTH,
    MARS,
    JUPITER,
    SATURN
} Levels;

// init all game state
void InitGame(int screenWidth, int screenHeight);

// per-frame update + draw
void GameUpdateDraw(void);

// clean up
void GameUnload(void);
