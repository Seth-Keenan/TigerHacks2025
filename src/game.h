#pragma once
#include "raylib.h"

// Definitions
#define NUM_SHOOTS 100
#define MAX_PICKUPS 10
#define MAX_ENEMIES 12
#define TILE_SIZE   32
#define MAP_WIDTH   25
#define MAP_HEIGHT  23
#define MAX_ROOMS_PER_FLOOR 8

typedef enum {
    STATION,
    SHOP,
    MISSION,
    GAMEOVER,
    PAUSE,
    QUIT,
    INSTRUCTIONS,
    START,
    STORY_EARTH,
    WIN_EARTH,
} GameState;

// optional, but you already had it
typedef enum {
    EARTH,
    MARS,
    JUPITER,
    SATURN
} Levels;

extern Levels gCurrentLevel;

// init all game state
void InitGame(int screenWidth, int screenHeight);

// per-frame update + draw
void GameUpdateDraw(void);

// clean up
void GameUnload(void);
