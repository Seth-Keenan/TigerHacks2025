#include "raylib.h"
#include <cstdio>

// For web games
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif
#include <cmath>

// Definitions
#define NUM_SHOOTS 100
#define MAX_PICKUPS 10
#define MAX_ENEMIES 5
#define TILE_SIZE   32
#define MAP_WIDTH   25
#define MAP_HEIGHT  19
#define MAX_ROOMS_PER_FLOOR 8

// Structs
typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} FacingDirection;

typedef enum {
    STATION,
    SHOP,     
    MISSION,  
    GAMEOVER
} GameState;

typedef enum {
    HEALTH,
    AMMO,
    POWERUP,
    MONEY
} PickupType;

typedef enum {
    EARTH,
    MARS,
    JUPITER,
    SATURN
} Levels;

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

// Globals
static int screenWidth = 800;
static int screenHeight = 600;
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

// Give this a better home later
// Earth station map (simple)
static int earthMap[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1},
    {1,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1},
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
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// Function Declarations
void UpdateDrawFrame(void); 

void UpdateGame(void);       
void DrawGame(void);
void UpdateStation(void);
void DrawStation(void);

void UpdateShop(void);
void DrawShop(void);

void UnloadGame(void);       
void DrawPlayer(Player player);
void DrawMap(Levels level);
void HandlePickup(Pickup *pickup);

void InitPlayerOnce(void)
{
    player.rec.x =  20;
    player.rec.y = 50;
    player.rec.width = 20;
    player.rec.height = 20;
    player.speed.x = 5;
    player.speed.y = 5;
    player.color = BLACK;
    player.facing = UP;
    player.health = 3;
    player.ammo = 50;
    player.currency = 0;
    player.iframes = 0;
}

void InitMission(void)
{
    // Initialize shoots
    for (int i = 0; i < NUM_SHOOTS; i++)
    {
        shoot[i].rec.x = player.rec.x;
        shoot[i].rec.y = player.rec.y + player.rec.height/4;
        shoot[i].rec.width = 10;
        shoot[i].rec.height = 5;
        shoot[i].speed.x = 0;
        shoot[i].speed.y = 0;
        shoot[i].active = false;
        shoot[i].color = MAROON;
    }

    for (int i = 0; i < MAX_PICKUPS; i++)
    {
        pickups[i].position.x = GetRandomValue(50, screenWidth - 50);
        pickups[i].position.y = GetRandomValue(50, screenHeight - 50);
        pickups[i].radius = 10.0f;
        pickups[i].active = true;

        // Randomly assign one of the 3 visible types: HEALTH, AMMO, MONEY
        int r = GetRandomValue(0, 2);  // 0=HEALTH, 1=AMMO, 2=MONEY
        pickups[i].type = (r == 0) ? HEALTH : (r == 1) ? AMMO : MONEY;

        switch (pickups[i].type)
        {
            case HEALTH:
                pickups[i].color = GREEN;   
                break;
            case AMMO:
                pickups[i].color = BLUE;    
                break;
            case MONEY:
                pickups[i].color = YELLOW;  
                break;
            default:
                pickups[i].color = RAYWHITE;
                break;
        }
    }


    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].rec.x = GetRandomValue(100, screenWidth - 100);
        enemies[i].rec.y = GetRandomValue(100, screenHeight - 100);
        enemies[i].rec.width = 20;
        enemies[i].rec.height = 20;
        enemies[i].speed.x = 2;
        enemies[i].speed.y = 2;
        enemies[i].color = RED;
        enemies[i].facing = DOWN;
        enemies[i].active = true;
    }
}

void UpdateGame(void)
{
    // Player movement    
    float dx = 0.0f, dy = 0.0f;
    
    if (IsKeyDown(KEY_D)) dx += 1.0f;
    if (IsKeyDown(KEY_A)) dx -= 1.0f;
    if (IsKeyDown(KEY_S)) dy += 1.0f;
    if (IsKeyDown(KEY_W)) dy -= 1.0f;
    
    float len = sqrtf(dx*dx + dy*dy);
    if (len > 0.0f) {
        dx /= len;
        dy /= len;
    }
    
    player.rec.x += dx * player.speed.x;
    player.rec.y += dy * player.speed.y;

    if (IsKeyDown(KEY_RIGHT)) player.facing = RIGHT;
    if (IsKeyDown(KEY_LEFT))  player.facing = LEFT;
    if (IsKeyDown(KEY_UP))    player.facing = UP;
    if (IsKeyDown(KEY_DOWN))  player.facing = DOWN;

    // Keep player in screen bounds
    if (player.rec.x < 0) player.rec.x = 0;
    if (player.rec.x + player.rec.width > screenWidth) player.rec.x = screenWidth - player.rec.width;
    if (player.rec.y < 0) player.rec.y = 0;
    if (player.rec.y + player.rec.height > screenHeight) player.rec.y = screenHeight - player.rec.height;

    for(int i = 0; i < MAX_PICKUPS; i++) {
        if (pickups[i].active) {
            if (CheckCollisionCircleRec(pickups[i].position, pickups[i].radius, player.rec)) {
                HandlePickup(&pickups[i]);
            }
        }
    }

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        
        enemies[i].rec.x += enemies[i].speed.x;
        enemies[i].rec.y += enemies[i].speed.y;

        if (enemies[i].rec.x < 0 || enemies[i].rec.x + enemies[i].rec.width > screenWidth) {
            enemies[i].speed.x *= -1;
        }
        if (enemies[i].rec.y < 0 || enemies[i].rec.y + enemies[i].rec.height > screenHeight) {
            enemies[i].speed.y *= -1;
        }

        Vector2 enemyCenter = {
            enemies[i].rec.x + enemies[i].rec.width / 2.0f,
            enemies[i].rec.y + enemies[i].rec.height / 2.0f
        };

        // Check collision with player
        if (CheckCollisionCircleRec(enemyCenter, enemies[i].rec.width/2, player.rec)) {
            if (player.iframes <= 0) {
                player.health -= 1;
                player.iframes = 60;

                // knockback away from enemy
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

        // Check collision with shoots
        for (int j = 0; j < NUM_SHOOTS; j++) {
            if (shoot[j].active && CheckCollisionCircleRec((Vector2){enemies[i].rec.x + enemies[i].rec.width/2, enemies[i].rec.y + enemies[i].rec.height/2}, enemies[i].rec.width/2, shoot[j].rec)) {
                shoot[j].active = false;
                enemies[i].active = false;
                player.currency += 5;
            }
        }
    }

    if (IsKeyDown(KEY_SPACE)) {
        shootRate += 3;

        // fire only on cadence
        if (shootRate % 20 == 0 && player.ammo > 0) {
            // find a free bullet slot
            for (int i = 0; i < NUM_SHOOTS; i++) {
                if (!shoot[i].active) {
                    shoot[i].rec.x = player.rec.x;
                    shoot[i].rec.y = player.rec.y + player.rec.height/4;
                    shoot[i].active = true;
                    shoot[i].facing = player.facing;
                    shoot[i].speed = (Vector2){0, 0};

                    player.ammo--;
                    break;
                }
            }
        }
    }
    else {
        if (shootRate > 0) shootRate--;
    }


    for (int i = 0; i < NUM_SHOOTS; i++) {
        if (shoot[i].active) {

            switch (shoot[i].facing)
            {
                case RIGHT:
                    shoot[i].speed.x = 7;
                    shoot[i].speed.y = 0;
                    shoot[i].rec.x += shoot[i].speed.x;
                    break;
            
                case LEFT:
                    shoot[i].speed.x = -7;
                    shoot[i].speed.y = 0;
                    shoot[i].rec.x += shoot[i].speed.x;
                    break;

                case UP:
                    shoot[i].speed.x = 0;
                    shoot[i].speed.y = -7;
                    shoot[i].rec.y += shoot[i].speed.y;
                    break;

                case DOWN:
                    shoot[i].speed.x = 0;
                    shoot[i].speed.y = 7;
                    shoot[i].rec.y += shoot[i].speed.y;
                    break;
            }
        }
    }

    if (player.iframes > 0) {
        player.iframes--;
    }
}

void DrawGame(void)
{
    BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawMap(EARTH); // placeholder

        DrawPlayer(player);

        int shotsLeft = player.ammo;
        for (int i = 0; i < NUM_SHOOTS; i++) {
            if (shoot[i].active) {
                DrawRectangleRec(shoot[i].rec, shoot[i].color);
            }
        }

        for(int i = 0; i < MAX_PICKUPS; i++) {
            if (pickups[i].active) {
                DrawCircleV(pickups[i].position, pickups[i].radius, pickups[i].color);
            }
        }

        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                DrawRectangleRec(enemies[i].rec, enemies[i].color);
            }
        }

        DrawText("Use arrow keys to move the player", 10, 10, 20, DARKGRAY);

        char ammoBuffer[32];
        snprintf(ammoBuffer, sizeof(ammoBuffer), "Shots left: %d", shotsLeft);
        DrawText(ammoBuffer, 10, 40, 20, DARKGRAY);

        char healthBuffer[32];
        snprintf(healthBuffer, sizeof(healthBuffer), "Health: %d", player.health);
        DrawText(healthBuffer, 10, 70, 20, DARKGRAY);

        char currencyBuffer[32];
        snprintf(currencyBuffer, sizeof(currencyBuffer), "Currency: %d", player.currency);
        DrawText(currencyBuffer, 10, 100, 20, DARKGRAY);

    EndDrawing();
}

void DrawMap(Levels level)
{
    // Fix this up with map cleanup later
    // pick which map to use
    int (*map)[MAP_WIDTH] = earthMap;  // default

    // later you can switch on level and pick different maps:
    switch (level) {
        case EARTH:
            map = earthMap;
            break;
        // case MARS: map = marsMap; break;
        // case JUPITER: map = jupiterMap; break;
        default:
            map = earthMap;
            break;
    }

    // draw tiles
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float tileX = (float)(x * TILE_SIZE);
            float tileY = (float)(y * TILE_SIZE);

            Rectangle tile = { tileX, tileY, (float)TILE_SIZE, (float)TILE_SIZE };

            int t = map[y][x];

            // Walls: 1; Teleporter: 2;
            if (t == 1) {
                DrawRectangleRec(tile, (Color){30, 30, 40, 255});
            } else if (t == 2) {
                DrawRectangleRec(tile, (Color){0, 230, 255, 180});
                DrawRectangleLinesEx(tile, 2, WHITE);
            } else {
                // floor
                DrawRectangleRec(tile, (Color){10, 15, 25, 255});
            }
        }
    }
}


void DrawPlayer(Player player)
{
    // draw the body
    DrawRectangleRec(player.rec, player.color);

    // triangle size
    float tipLen = 10.0f;
    float halfH  = player.rec.height / 2.0f;
    float halfW  = player.rec.width  / 2.0f;

    Vector2 p1, p2, p3;

    switch (player.facing)
    {
        case RIGHT:
            // tip to the right, base on right edge
            p1 = (Vector2){ player.rec.x + player.rec.width + tipLen, player.rec.y + halfH };
            p2 = (Vector2){ player.rec.x + player.rec.width, player.rec.y };
            p3 = (Vector2){ player.rec.x + player.rec.width, player.rec.y + player.rec.height };
            break;

        case LEFT:
            // tip to the left, base on left edge (mirror of RIGHT)
            p1 = (Vector2){ player.rec.x - tipLen, player.rec.y + halfH };
            p2 = (Vector2){ player.rec.x, player.rec.y };
            p3 = (Vector2){ player.rec.x, player.rec.y + player.rec.height };
            break;

        case UP:
            // tip above, base on top edge
            p1 = (Vector2){ player.rec.x + halfW, player.rec.y - tipLen };
            p2 = (Vector2){ player.rec.x, player.rec.y };
            p3 = (Vector2){ player.rec.x + player.rec.width, player.rec.y };
            break;

        case DOWN:
            // tip below, base on bottom edge
            p1 = (Vector2){ player.rec.x + halfW, player.rec.y + player.rec.height + tipLen };
            p2 = (Vector2){ player.rec.x, player.rec.y + player.rec.height };
            p3 = (Vector2){ player.rec.x + player.rec.width, player.rec.y + player.rec.height };
            break;
    }

    DrawTriangle(p1, p2, p3, YELLOW);
    DrawTriangle(p1, p3, p2, YELLOW);
}

void HandlePickup(Pickup *p) {
    if (!p) return;

    p->active = false;

    switch (p->type) {
        case HEALTH: 
            player.health += 1; 
            break;
        case AMMO:   
            player.ammo  += 5;  
            break;
        case MONEY:  
            player.currency += 10; 
            break;
        case POWERUP: 
            /* do something */ 
            break;
    }
}

void DrawStation(void)
{
    BeginDrawing();
    ClearBackground((Color){5, 8, 20, 255});  // dark spacey

    DrawText("SPACE STATION: ORBITAL HUB", 40, 40, 30, RAYWHITE);
    DrawText("1) Shop", 60, 100, 20, RAYWHITE);
    DrawText("2) Launch Mission", 60, 130, 20, RAYWHITE);

    char buf[64];
    snprintf(buf, sizeof(buf), "Credits: %d", player.currency);
    DrawText(buf, 60, 170, 20, GOLD);

    EndDrawing();
}

void UpdateStation(void)
{
    if (IsKeyPressed(KEY_ONE)) {
        gState = SHOP;
    }
    if (IsKeyPressed(KEY_TWO)) {
        InitMission();
        gState = MISSION;
    }
}

void UpdateShop(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

void DrawShop(void)
{
    BeginDrawing();
    ClearBackground((Color){12, 12, 24, 255});
    DrawText("SHOP (ESC to return)", 40, 40, 30, RAYWHITE);
    EndDrawing();
}

void UpdateDrawFrame(void)
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

void UnloadGame(void)
{
    // TODO: Unload game variables (textures, sounds, models...)
}

int main(void)
{
    InitWindow(screenWidth, screenHeight, "classic game: space invaders");
    InitPlayerOnce();
    SetExitKey(KEY_NULL);

    SetTargetFPS(60);

    // Main game loop
    bool running = true;
    while (running) {
        if (WindowShouldClose()) running = false; // allow window X but not ESC
        UpdateDrawFrame();
    }

    // UnloadGame();         // Unload loaded data (textures, sounds, models...)
    CloseWindow();        // Close window and OpenGL context
    return 0;
}