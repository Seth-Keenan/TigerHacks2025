#include "game.h"
#include <cstdio>
#include <cmath>

// For web games
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Definitions
#define NUM_SHOOTS 100
#define MAX_PICKUPS 10
#define MAX_ENEMIES 5
#define TILE_SIZE   32
#define MAP_WIDTH   25
#define MAP_HEIGHT  23
#define MAX_ROOMS_PER_FLOOR 8



typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} FacingDirection;

typedef enum {
    HEALTH,
    AMMO,
    POWERUP,
    MONEY
} PickupType;

typedef struct Player {
    Vector2 speed;
    Rectangle rec;
    Color color;
    FacingDirection facing;
    int health;
    int ammo;
    int currency;
    int iframes;
} Player;

typedef struct Shoot {
    Vector2 speed;
    Rectangle rec;
    bool active;
    Color color;
    FacingDirection facing;
} Shoot;

typedef struct Pickup {
    Vector2 position;   
    float radius;       
    Color color;        
    bool active;
    PickupType type;        
} Pickup;

typedef struct Enemy {
    Vector2 speed;
    Rectangle rec;
    Color color;
    FacingDirection facing;
    bool active;
} Enemy;

typedef struct Room {
    int map[MAP_HEIGHT][MAP_WIDTH];
    bool cleared;
    bool isBoss;
} Room;

// --------------------
// statics / globals for this translation unit
// --------------------
static int gScreenWidth  = 800;
static int gScreenHeight = 600;

static Player player = { 0 };
static Shoot shoot[NUM_SHOOTS] = { 0 };
static Pickup pickups[MAX_PICKUPS] = { 0 };
static Enemy enemies[MAX_ENEMIES] = { 0 };
static int shootRate = 0;
static GameState gState = STATION;
static int gFloor = 1;
static int gCurrentRoom = 0;
static int gRoomsOnFloor = 0;
static Room gRooms[MAX_ROOMS_PER_FLOOR];

static int earthMap[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1},
    {1,0,0,0,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,1,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

static void InitPlayerOnce(void);
static void InitMission(void);
static void UpdateGame(void);
static void DrawGame(void);
static void UpdateStation(void);
static void DrawStation(void);
static void UpdateShop(void);
static void DrawShop(void);
static void DrawPlayer(const Player& player);
static void DrawMapCurrentRoom(void);
static void HandlePickup(Pickup *pickup);
static void UpdatePause(void);
static void DrawPause(void);
static void UpdateQuit(void);
static void DrawQuit(void);
static bool IsTileSolid(int tx, int ty);
static bool GetRandomFreeTilePos(float *outX, float *outY);

void InitGame(int screenWidth, int screenHeight)
{
    gScreenWidth = screenWidth;
    gScreenHeight = screenHeight;

    InitPlayerOnce();
    gState = STATION;
}

void GameUpdateDraw(void)
{
    switch (gState) {
        case STATION:
            UpdateStation();
            DrawStation();
            break;
        case SHOP:
            UpdateShop();
            DrawShop();
            break;
        case MISSION:
            UpdateGame();
            DrawGame();
            break;
        case PAUSE:
            UpdatePause();
            DrawPause();
            break;
        case QUIT:
            UpdateQuit();
            DrawQuit();
            break;
        case GAMEOVER:
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("GAME OVER - press ENTER for station", 80, 200, 20, RAYWHITE);
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)) {
                gState = STATION;
                player.health = 3;
            }
            break;
    }
}

void GameUnload(void)
{
    // if you load textures/sounds later, unload here
}

// Helpers for tile logic
static bool IsTileSolid(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_WIDTH || ty >= MAP_HEIGHT) return true;
    int t = earthMap[ty][tx];
    return (t == 1);  // walls block
}

static bool GetRandomFreeTilePos(float *outX, float *outY)
{
    // try a bunch of times to find a floor tile
    for (int tries = 0; tries < 100; tries++) {
        int tx = GetRandomValue(1, MAP_WIDTH  - 2);  // avoid outer wall
        int ty = GetRandomValue(1, MAP_HEIGHT - 2);

        int t = earthMap[ty][tx];
        if (t != 1) { // not a wall
            // return pixel position (center of tile)
            *outX = tx * TILE_SIZE + TILE_SIZE * 0.25f;   // small inset so 20x20 fits
            *outY = ty * TILE_SIZE + TILE_SIZE * 0.25f;
            return true;
        }
    }
    return false; // no spot found
}

static void InitPlayerOnce(void)
{
    player.rec.x =  90;
    player.rec.y = 60;
    player.rec.width = 20;
    player.rec.height = 20;
    player.speed.x = 5;
    player.speed.y = 5;
    player.color = WHITE;
    player.facing = UP;
    player.health = 3;
    player.ammo = 50;
    player.currency = 0;
    player.iframes = 0;
}

static void InitMission(void)
{
    // bullets
    for (int i = 0; i < NUM_SHOOTS; i++)
    {
        shoot[i].rec.x = player.rec.x;
        shoot[i].rec.y = player.rec.y + player.rec.height/4;
        shoot[i].rec.width = 10;
        shoot[i].rec.height = 5;
        shoot[i].speed = {0,0};
        shoot[i].active = false;
        shoot[i].color = MAROON;
    }

    // pickups
    for (int i = 0; i < MAX_PICKUPS; i++)
    {
        float fx, fy;
        if (GetRandomFreeTilePos(&fx, &fy)) {
            pickups[i].position.x = fx + 10.0f;  // center it nicer
            pickups[i].position.y = fy + 10.0f;
        } else {
            pickups[i].position.x = 100;
            pickups[i].position.y = 100;
        }
        pickups[i].radius = 10.0f;
        pickups[i].active = true;

        int r = GetRandomValue(0, 2);  // 0=HEALTH, 1=AMMO, 2=MONEY
        pickups[i].type = (r == 0) ? HEALTH : (r == 1) ? AMMO : MONEY;

        switch (pickups[i].type)
        {
            case HEALTH: pickups[i].color = GREEN;  break;
            case AMMO:   pickups[i].color = BLUE;   break;
            case MONEY:  pickups[i].color = YELLOW; break;
            default:     pickups[i].color = RAYWHITE; break;
        }
    }

    // enemies
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        float ex, ey;
        if (GetRandomFreeTilePos(&ex, &ey)) {
            enemies[i].rec.x = ex;
            enemies[i].rec.y = ey;
        } else {
            enemies[i].rec.x = 100;
            enemies[i].rec.y = 100;
        }

        enemies[i].rec.width = 20;
        enemies[i].rec.height = 20;
        enemies[i].speed.x = 2;
        enemies[i].speed.y = 2;
        enemies[i].color = RED;
        enemies[i].facing = DOWN;
        enemies[i].active = true;
    }
}

static void UpdateGame(void) {
    // Exit state
    if (IsKeyPressed(KEY_ESCAPE) && gState == MISSION) {
        gState = PAUSE;
        return;
    }

    // movement
    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_D)) dx += 1.0f;
    if (IsKeyDown(KEY_A)) dx -= 1.0f;
    if (IsKeyDown(KEY_S)) dy += 1.0f;
    if (IsKeyDown(KEY_W)) dy -= 1.0f;

    // how far we want to move this frame
    float moveX = dx * player.speed.x;
    float moveY = dy * player.speed.y;

    // --- move on X first ---
    if (moveX != 0.0f) {
        float newX = player.rec.x + moveX;

        // figure out which tiles the player's rectangle would cover
        // we check the top and bottom edges
        float left   = newX;
        float right  = newX + player.rec.width;
        float top    = player.rec.y;
        float bottom = player.rec.y + player.rec.height;

        int tileLeft   = (int)(left   / TILE_SIZE);
        int tileRight  = (int)(right  / TILE_SIZE);
        int tileTop    = (int)(top    / TILE_SIZE);
        int tileBottom = (int)(bottom / TILE_SIZE);

        bool blocked = false;
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            for (int tx = tileLeft; tx <= tileRight; tx++) {
                if (IsTileSolid(tx, ty)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) break;
        }

        if (!blocked) {
            player.rec.x = newX;
        }
    }

    // --- move on Y second ---
    if (moveY != 0.0f) {
        float newY = player.rec.y + moveY;

        float left   = player.rec.x;
        float right  = player.rec.x + player.rec.width;
        float top    = newY;
        float bottom = newY + player.rec.height;

        int tileLeft   = (int)(left   / TILE_SIZE);
        int tileRight  = (int)(right  / TILE_SIZE);
        int tileTop    = (int)(top    / TILE_SIZE);
        int tileBottom = (int)(bottom / TILE_SIZE);

        bool blocked = false;
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            for (int tx = tileLeft; tx <= tileRight; tx++) {
                if (IsTileSolid(tx, ty)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) break;
        }

        if (!blocked) {
            player.rec.y = newY;
        }
    }

    float len = sqrtf(dx*dx + dy*dy);
    if (len > 0.0f) {
        dx /= len;
        dy /= len;
    }

    if (IsKeyDown(KEY_RIGHT)) player.facing = RIGHT;
    if (IsKeyDown(KEY_LEFT))  player.facing = LEFT;
    if (IsKeyDown(KEY_UP))    player.facing = UP;
    if (IsKeyDown(KEY_DOWN))  player.facing = DOWN;

    // bounds
    if (player.rec.x < 0) player.rec.x = 0;
    if (player.rec.x + player.rec.width > gScreenWidth) player.rec.x = gScreenWidth - player.rec.width;
    if (player.rec.y < 0) player.rec.y = 0;
    if (player.rec.y + player.rec.height > gScreenHeight) player.rec.y = gScreenHeight - player.rec.height;

    // pickups
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (pickups[i].active &&
            CheckCollisionCircleRec(pickups[i].position, pickups[i].radius, player.rec)) {
            HandlePickup(&pickups[i]);
        }
    }

    // enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        // --- try move on X ---
        float newEx = enemies[i].rec.x + enemies[i].speed.x;

        {
            float left   = newEx;
            float right  = newEx + enemies[i].rec.width;
            float top    = enemies[i].rec.y;
            float bottom = enemies[i].rec.y + enemies[i].rec.height;

            int txL = (int)(left   / TILE_SIZE);
            int txR = (int)(right  / TILE_SIZE);
            int tyT = (int)(top    / TILE_SIZE);
            int tyB = (int)(bottom / TILE_SIZE);

            bool blocked = false;
            for (int ty = tyT; ty <= tyB; ty++) {
                for (int tx = txL; tx <= txR; tx++) {
                    if (IsTileSolid(tx, ty)) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) break;
            }

            if (!blocked) {
                enemies[i].rec.x = newEx;
            } else {
                // bounce off wall
                enemies[i].speed.x *= -1;
            }
        }

        // --- try move on Y ---
        float newEy = enemies[i].rec.y + enemies[i].speed.y;

        {
            float left   = enemies[i].rec.x;
            float right  = enemies[i].rec.x + enemies[i].rec.width;
            float top    = newEy;
            float bottom = newEy + enemies[i].rec.height;

            int txL = (int)(left   / TILE_SIZE);
            int txR = (int)(right  / TILE_SIZE);
            int tyT = (int)(top    / TILE_SIZE);
            int tyB = (int)(bottom / TILE_SIZE);

            bool blocked = false;
            for (int ty = tyT; ty <= tyB; ty++) {
                for (int tx = txL; tx <= txR; tx++) {
                    if (IsTileSolid(tx, ty)) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) break;
            }

            if (!blocked) {
                enemies[i].rec.y = newEy;
            } else {
                enemies[i].speed.y *= -1;
            }
        }

        // you can still keep a screen clamp if you want
        if (enemies[i].rec.x < 0)                    { enemies[i].rec.x = 0; enemies[i].speed.x *= -1; }
        if (enemies[i].rec.x + enemies[i].rec.width > gScreenWidth) {
            enemies[i].rec.x = gScreenWidth - enemies[i].rec.width;
            enemies[i].speed.x *= -1;
        }
        if (enemies[i].rec.y < 0)                    { enemies[i].rec.y = 0; enemies[i].speed.y *= -1; }
        if (enemies[i].rec.y + enemies[i].rec.height > gScreenHeight) {
            enemies[i].rec.y = gScreenHeight - enemies[i].rec.height;
            enemies[i].speed.y *= -1;
        }

        // --- now do collisions with player / bullets ---
        Vector2 enemyCenter = {
            enemies[i].rec.x + enemies[i].rec.width  / 2.0f,
            enemies[i].rec.y + enemies[i].rec.height / 2.0f
        };

        // enemy hits player
        if (CheckCollisionCircleRec(enemyCenter, enemies[i].rec.width/2, player.rec)) {
            if (player.iframes <= 0) {
                player.health -= 1;
                player.iframes = 60;

                Vector2 knock = {
                    player.rec.x + player.rec.width/2  - enemyCenter.x,
                    player.rec.y + player.rec.height/2 - enemyCenter.y
                };
                float mag = sqrtf(knock.x*knock.x + knock.y*knock.y);
                if (mag > 0.0f) {
                    knock.x /= mag;
                    knock.y /= mag;
                }
                player.rec.x += knock.x * 10.0f;
                player.rec.y += knock.y * 10.0f;

                if (player.health <= 0) {
                    gState = GAMEOVER;
                }
            }
        }

        // enemy hit by bullet
        for (int j = 0; j < NUM_SHOOTS; j++) {
            if (!shoot[j].active) continue;
            if (CheckCollisionCircleRec(enemyCenter, enemies[i].rec.width/2, shoot[j].rec)) {
                shoot[j].active = false;
                enemies[i].active = false;
                player.currency += 5;
            }
        }
    }


    // shooting
    if (IsKeyDown(KEY_SPACE)) {
        shootRate += 3;
        if (shootRate % 20 == 0 && player.ammo > 0) {
            for (int i = 0; i < NUM_SHOOTS; i++) {
                if (!shoot[i].active) {
                    shoot[i].rec.x = player.rec.x;
                    shoot[i].rec.y = player.rec.y + player.rec.height/4;
                    shoot[i].active = true;
                    shoot[i].facing = player.facing;
                    shoot[i].speed = {0,0};
                    player.ammo--;
                    break;
                }
            }
        }
    } else {
        if (shootRate > 0) shootRate--;
    }

    // bullet movement
    for (int i = 0; i < NUM_SHOOTS; i++) {
        if (!shoot[i].active) continue;
        switch (shoot[i].facing)
        {
            case RIGHT: shoot[i].rec.x += 7; break;
            case LEFT:  shoot[i].rec.x -= 7; break;
            case UP:    shoot[i].rec.y -= 7; break;
            case DOWN:  shoot[i].rec.y += 7; break;
        }

        // check tile the bullet is now inside
        float bx = shoot[i].rec.x + shoot[i].rec.width  * 0.5f;
        float by = shoot[i].rec.y + shoot[i].rec.height * 0.5f;

        int tx = (int)(bx / TILE_SIZE);
        int ty = (int)(by / TILE_SIZE);

        if (IsTileSolid(tx, ty)) {
            // bullet hit wall -> destroy
            shoot[i].active = false;
            continue;
        }

        // off-screen deactivate
        if (shoot[i].rec.x > gScreenWidth || shoot[i].rec.x + shoot[i].rec.width < 0 ||
            shoot[i].rec.y > gScreenHeight || shoot[i].rec.y + shoot[i].rec.height < 0) {
            shoot[i].active = false;
        }
    }

    if (player.iframes > 0) player.iframes--;
}

static void DrawGame(void)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawMapCurrentRoom();
    DrawPlayer(player);

    for (int i = 0; i < NUM_SHOOTS; i++) {
        if (shoot[i].active) {
            DrawRectangleRec(shoot[i].rec, shoot[i].color);
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (pickups[i].active) {
            DrawCircleV(pickups[i].position, pickups[i].radius, pickups[i].color);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            DrawRectangleRec(enemies[i].rec, enemies[i].color);
        }
    }

    DrawText("Use WASD to move, arrows to aim, SPACE to shoot", 10, 10, 20, DARKGRAY);

    char buf[64];
    snprintf(buf, sizeof(buf), "Ammo: %d", player.ammo);
    DrawText(buf, 10, 40, 20, DARKGRAY);

    snprintf(buf, sizeof(buf), "Health: %d", player.health);
    DrawText(buf, 10, 70, 20, DARKGRAY);

    snprintf(buf, sizeof(buf), "Currency: %d", player.currency);
    DrawText(buf, 10, 100, 20, DARKGRAY);

    EndDrawing();
}

static void DrawMapCurrentRoom(void)
{
    // for now just draw the earthMap
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float tileX = (float)(x * TILE_SIZE);
            float tileY = (float)(y * TILE_SIZE);
            Rectangle tile = { tileX, tileY, (float)TILE_SIZE, (float)TILE_SIZE };

            int t = earthMap[y][x];
            if (t == 1) {
                DrawRectangleRec(tile, (Color){30, 30, 40, 255});
            } else if (t == 2) {
                DrawRectangleRec(tile, (Color){0, 230, 255, 180});
                DrawRectangleLinesEx(tile, 2, WHITE);
            } else {
                DrawRectangleRec(tile, (Color){10, 15, 25, 255});
            }
        }
    }
}

static void DrawPlayer(const Player& player)
{
    // body
    DrawRectangleRec(player.rec, player.color);

    float tipLen = 10.0f;
    float halfH  = player.rec.height / 2.0f;
    float halfW  = player.rec.width  / 2.0f;

    Vector2 p1, p2, p3;
    switch (player.facing)
    {
        case RIGHT:
            p1 = { player.rec.x + player.rec.width + tipLen, player.rec.y + halfH };
            p2 = { player.rec.x + player.rec.width, player.rec.y };
            p3 = { player.rec.x + player.rec.width, player.rec.y + player.rec.height };
            break;
        case LEFT:
            p1 = { player.rec.x - tipLen, player.rec.y + halfH };
            p2 = { player.rec.x, player.rec.y };
            p3 = { player.rec.x, player.rec.y + player.rec.height };
            break;
        case UP:
            p1 = { player.rec.x + halfW, player.rec.y - tipLen };
            p2 = { player.rec.x, player.rec.y };
            p3 = { player.rec.x + player.rec.width, player.rec.y };
            break;
        case DOWN:
            p1 = { player.rec.x + halfW, player.rec.y + player.rec.height + tipLen };
            p2 = { player.rec.x, player.rec.y + player.rec.height };
            p3 = { player.rec.x + player.rec.width, player.rec.y + player.rec.height };
            break;
    }

    DrawTriangle(p1, p2, p3, YELLOW);
    DrawTriangle(p1, p3, p2, YELLOW);
}

static void HandlePickup(Pickup *p)
{
    if (!p) return;
    p->active = false;

    switch (p->type) {
        case HEALTH: player.health += 1; break;
        case AMMO:   player.ammo   += 5; break;
        case MONEY:  player.currency += 10; break;
        case POWERUP: /*todo*/ break;
    }
}

static void DrawStation(void)
{
    BeginDrawing();
    ClearBackground((Color){5, 8, 20, 255});

    DrawText("SPACE STATION: ORBITAL HUB", 40, 40, 30, RAYWHITE);
    DrawText("1) Shop", 60, 100, 20, RAYWHITE);
    DrawText("2) Launch Mission", 60, 130, 20, RAYWHITE);

    char buf[64];
    snprintf(buf, sizeof(buf), "Credits: %d", player.currency);
    DrawText(buf, 60, 170, 20, GOLD);

    EndDrawing();
}

static void UpdateStation(void)
{
    if (IsKeyPressed(KEY_ONE)) {
        gState = SHOP;
    }
    if (IsKeyPressed(KEY_TWO)) {
        InitMission();
        gState = MISSION;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        gState = QUIT;
    }
}

static void UpdateShop(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

static void DrawShop(void)
{
    BeginDrawing();
    ClearBackground((Color){12, 12, 24, 255});
    DrawText("SHOP (ESC to return)", 40, 40, 30, RAYWHITE);
    EndDrawing();
}

static void UpdatePause(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        // Resume game
        gState = MISSION;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        gState = STATION;
    }
}

static void DrawPause(void)
{
    BeginDrawing();
    // Keep a translucent overlay so the game background is still visible
    DrawRectangle(0, 0, gScreenWidth, gScreenHeight, Fade(BLACK, 0.6f));

    DrawText("PAUSED", gScreenWidth/2 - 80, gScreenHeight/2 - 80, 40, RAYWHITE);
    DrawText("Press ESC to Resume", gScreenWidth/2 - 120, gScreenHeight/2, 20, GRAY);
    DrawText("Press ENTER to Return to Station", gScreenWidth/2 - 170, gScreenHeight/2 + 40, 20, GRAY);

    EndDrawing();
}

static void UpdateQuit(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        // Resume game
        gState = STATION;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        CloseWindow();
        exit(0); 
    }
}

static void DrawQuit(void)
{
    BeginDrawing();
    // Keep a translucent overlay so the game background is still visible
    DrawRectangle(0, 0, gScreenWidth, gScreenHeight, Fade(BLACK, 0.6f));

    DrawText("QUIT", gScreenWidth/2 - 80, gScreenHeight/2 - 80, 40, RAYWHITE);
    DrawText("Press ESC to resume", gScreenWidth/2 - 120, gScreenHeight/2, 20, GRAY);
    DrawText("Press ENTER to quit the game", gScreenWidth/2 - 170, gScreenHeight/2 + 40, 20, GRAY);

    EndDrawing();
}