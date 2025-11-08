#include "game.h"
#include "maps.h"
#include <cstdio>
#include <cmath>

// For web games
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

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
    bool fromPlayer;
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
    bool isShooter;
    bool isBoss;
    int shootTimer;
    int health;      
    int maxHealth;
} Enemy;

typedef struct Room {
    int map[MAP_HEIGHT][MAP_WIDTH];
    bool cleared;
    bool isBoss;
} Room;

class BouncerEnemy {
    public:
        Rectangle rec{};
        Vector2 speed{};
        Color color{RED};
        bool active{false};

        void Init(float x, float y, float vx = 2.0f, float vy = 2.0f, Color c = RED) {
            rec.x = x;
            rec.y = y;
            rec.width = 20;
            rec.height = 20;
            speed.x = vx;
            speed.y = vy;
            color = c;
            active = true;
        }

        // map is [MAP_HEIGHT][MAP_WIDTH]
        void Update(int (*map)[MAP_WIDTH], int screenW, int screenH) {
            if (!active) return;

            // --- X move ---
            float newEx = rec.x + speed.x;
            {
                float left   = newEx;
                float right  = newEx + rec.width;
                float top    = rec.y;
                float bottom = rec.y + rec.height;

                int txL = (int)(left   / TILE_SIZE);
                int txR = (int)(right  / TILE_SIZE);
                int tyT = (int)(top    / TILE_SIZE);
                int tyB = (int)(bottom / TILE_SIZE);

                bool blocked = false;
                for (int ty = tyT; ty <= tyB; ty++) {
                    for (int tx = txL; tx <= txR; tx++) {
                        if (tx < 0 || ty < 0 || tx >= MAP_WIDTH || ty >= MAP_HEIGHT) {
                            blocked = true;
                            break;
                        }
                        if (map[ty][tx] == 1) { // wall
                            blocked = true;
                            break;
                        }
                    }
                    if (blocked) break;
                }

                if (!blocked) {
                    rec.x = newEx;
                } else {
                    speed.x *= -1;
                }
            }

            // --- Y move ---
            float newEy = rec.y + speed.y;
            {
                float left   = rec.x;
                float right  = rec.x + rec.width;
                float top    = newEy;
                float bottom = newEy + rec.height;

                int txL = (int)(left   / TILE_SIZE);
                int txR = (int)(right  / TILE_SIZE);
                int tyT = (int)(top    / TILE_SIZE);
                int tyB = (int)(bottom / TILE_SIZE);

                bool blocked = false;
                for (int ty = tyT; ty <= tyB; ty++) {
                    for (int tx = txL; tx <= txR; tx++) {
                        if (tx < 0 || ty < 0 || tx >= MAP_WIDTH || ty >= MAP_HEIGHT) {
                            blocked = true;
                            break;
                        }
                        if (map[ty][tx] == 1) {
                            blocked = true;
                            break;
                        }
                    }
                    if (blocked) break;
                }

                if (!blocked) {
                    rec.y = newEy;
                } else {
                    speed.y *= -1;
                }
            }

            // screen clamp bounce (optional)
            if (rec.x < 0) { rec.x = 0; speed.x *= -1; }
            if (rec.x + rec.width > screenW) {
                rec.x = screenW - rec.width;
                speed.x *= -1;
            }
            if (rec.y < 0) { rec.y = 0; speed.y *= -1; }
            if (rec.y + rec.height > screenH) {
                rec.y = screenH - rec.height;
                speed.y *= -1;
            }
        }
};

static int gScreenWidth  = 800;
static int gScreenHeight = 600;

static Player player = { 0 };
static Shoot shoot[NUM_SHOOTS] = { 0 };
static Pickup pickups[MAX_PICKUPS] = { 0 };
static Enemy enemies[MAX_ENEMIES] = { 0 };

static int shootRate = 0;
static int gBulletSpeed = 7;   
static int gFireDelay   = 20;  

static GameState gState = STATION;

int gFloor = 1;
static int gCurrentRoom = 0;
static int gRoomsOnFloor = 0;
static Room gRooms[MAX_ROOMS_PER_FLOOR];

static bool gHasKey = false;
static float gKeyRadius = 10.0f;
static Vector2 gKeyPos = { 22 * TILE_SIZE + 8, 17 * TILE_SIZE + 8 };

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
static void UpdateInstructions(void);
static void DrawInstructions(void);
static void UpdateStart(void);
static void DrawStart(void);
static bool IsTileSolid(int tx, int ty);
static bool GetRandomFreeTilePos(float *outX, float *outY);
static bool FindTileOfType(int tileValue, float *outX, float *outY);
static void ResetToFloor1KeepMoney(void);

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

void InitGame(int screenWidth, int screenHeight)
{
    gScreenWidth = screenWidth;
    gScreenHeight = screenHeight;

    InitPlayerOnce();
    gState = START;
}

void GameUpdateDraw(void)
{
    switch (gState) {
        case START:
            UpdateStart();
            break;
        case STATION:
            UpdateStation();
            break;
        case SHOP:
            UpdateShop();
            break;
        case MISSION:
            UpdateGame();
            break;
        case PAUSE:
            UpdatePause();
            break;
        case QUIT:
            UpdateQuit();
            break;
        case INSTRUCTIONS:
            UpdateInstructions();
            break;
        case GAMEOVER:
            if (IsKeyPressed(KEY_ENTER)) {
                ResetToFloor1KeepMoney();
            }
            break;
    }

    switch (gState) {
        case START:
            DrawStart();
            break;
        case STATION:
            DrawStation();
            break;
        case SHOP:
            DrawShop();
            break;
        case MISSION:
            DrawGame();
            break;
        case PAUSE:
            DrawPause();
            break;
        case QUIT:
            DrawQuit();
            break;
        case INSTRUCTIONS:
            DrawInstructions();
            break;
        case GAMEOVER:
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("GAME OVER - press ENTER for station", 80, 200, 20, RAYWHITE);
            EndDrawing();
            break;
    }
}


void GameUnload(void)
{
    // if you load textures/sounds later, unload here
}

// Enemy helper
static void EnemyFireBullets(Enemy &e, int count)
{
    for (int k = 0; k < count; k++) {
        for (int i = 0; i < NUM_SHOOTS; i++) {
            if (!shoot[i].active) {
                shoot[i].active = true;

                float cx = e.rec.x + e.rec.width  * 0.5f;
                float cy = e.rec.y + e.rec.height * 0.5f;

                shoot[i].rec.width  = 10;
                shoot[i].rec.height = 5;

                // start at center
                shoot[i].rec.x = cx;
                shoot[i].rec.y = cy;

                // push out a bit in facing direction
                const float spawnOffset = 8.0f;
                switch (e.facing) {
                    case RIGHT: 
                        shoot[i].rec.x += spawnOffset; 
                        break;
                    case LEFT:  
                        shoot[i].rec.x -= spawnOffset; 
                        break;
                    case UP:    
                        shoot[i].rec.y -= spawnOffset; 
                        break;
                    case DOWN:  
                        shoot[i].rec.y += spawnOffset; 
                        break;
                }

                shoot[i].color = RED;          
                shoot[i].facing = e.facing;
                shoot[i].fromPlayer = false;    
                break;
            }
        }
    }
}

// boss fires bullets in a circle
static void BossFireCircle(Enemy &e)
{
    const int bullets = 12;            // 12-way spread
    const float speed = 4.0f;

    float cx = e.rec.x + e.rec.width  * 0.5f;
    float cy = e.rec.y + e.rec.height * 0.5f;

    for (int b = 0; b < bullets; b++) {
        float angle = (2.0f * PI * b) / bullets;
        float vx = cosf(angle) * speed;
        float vy = sinf(angle) * speed;

        for (int i = 0; i < NUM_SHOOTS; i++) {
            if (!shoot[i].active) {
                shoot[i].active = true;
                shoot[i].fromPlayer = false;
                shoot[i].color = RED;
                shoot[i].rec.width = 10;
                shoot[i].rec.height = 10;
                shoot[i].rec.x = cx;
                shoot[i].rec.y = cy;
                shoot[i].speed.x = vx;
                shoot[i].speed.y = vy;
                shoot[i].facing = RIGHT;
                break;
            }
        }
    }
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
    gHasKey = false;
    int (*currMap)[MAP_WIDTH] = GetMapForFloor(gFloor);

    // Player Spawn
    float sx, sy;
    if (Map_FindTile(currMap, 3, &sx, &sy)) {
        player.rec.x = sx;
        player.rec.y = sy;
    } else {
        player.rec.x = 90;
        player.rec.y = 60;
    }

    // place key from map tile 10, fallback to old hardcoded spot
    float kx, ky;
    if (Map_FindKeyTile(currMap, &kx, &ky)) {
        gKeyPos.x = kx;
        gKeyPos.y = ky;
    } else {
        if (!Map_HasBossTile(currMap)) {
            gKeyPos = { 22 * TILE_SIZE + 8, 17 * TILE_SIZE + 8 };
        } else {
            gKeyPos = { -1000.0f, -1000.0f };
        }
    }

    // bullets
    for (int i = 0; i < NUM_SHOOTS; i++)
    {
        shoot[i].rec.x = player.rec.x;
        shoot[i].rec.y = player.rec.y + player.rec.height/4;
        shoot[i].rec.width = 10;
        shoot[i].rec.height = 5;
        shoot[i].speed = {0,0};
        shoot[i].active = false;
        shoot[i].color = RAYWHITE;
        shoot[i].fromPlayer = false;
    }

    // pickups
    for (int i = 0; i < MAX_PICKUPS; i++)
    {
        float fx, fy;
        if (Map_GetRandomFreeTile(currMap, &fx, &fy)) {
            pickups[i].position.x = fx + 10.0f;  // center it nicer
            pickups[i].position.y = fy + 10.0f;
        } else {
            pickups[i].position.x = 100;
            pickups[i].position.y = 100;
        }

        pickups[i].radius = 10.0f;
        pickups[i].active = true;

        int r = GetRandomValue(0, 1);  // 0=AMMO, 1=MONEY
        pickups[i].type = (r == 0) ? AMMO : MONEY;

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

    // enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
        enemies[i].isShooter = false;
        enemies[i].isBoss = false;
        enemies[i].shootTimer = 0;
        enemies[i].health = 1;
        enemies[i].maxHealth = 1;
    }

    int enemyIndex = 0;

    // scan map for enemy tiles
    for (int y = 0; y < MAP_HEIGHT && enemyIndex < MAX_ENEMIES; y++) {
        for (int x = 0; x < MAP_WIDTH && enemyIndex < MAX_ENEMIES; x++) {
            int t = currMap[y][x];

            // 4 = bouncer
            if (t == 4) {
                enemies[enemyIndex].rec.x = x * TILE_SIZE + (TILE_SIZE - 20) * 0.5f;
                enemies[enemyIndex].rec.y = y * TILE_SIZE + (TILE_SIZE - 20) * 0.5f;
                enemies[enemyIndex].rec.width  = 20;
                enemies[enemyIndex].rec.height = 20;
                enemies[enemyIndex].speed = {2, 2};
                enemies[enemyIndex].color = RED;
                enemies[enemyIndex].facing = DOWN;
                enemies[enemyIndex].active = true;
                enemies[enemyIndex].isShooter = false;
                enemies[enemyIndex].shootTimer = 0;
                enemyIndex++;
            }

            // 5–8 = shooter with direction
            else if (t >= 5 && t <= 8) {
                enemies[enemyIndex].rec.x = x * TILE_SIZE + (TILE_SIZE - 20) * 0.5f;
                enemies[enemyIndex].rec.y = y * TILE_SIZE + (TILE_SIZE - 20) * 0.5f;
                enemies[enemyIndex].rec.width  = 20;
                enemies[enemyIndex].rec.height = 20;
                enemies[enemyIndex].speed = {0, 0}; 
                enemies[enemyIndex].color = (Color){200, 80, 40, 255};
                enemies[enemyIndex].active = true;
                enemies[enemyIndex].isShooter = true;
                enemies[enemyIndex].shootTimer = 180;

                switch (t) {
                    case 5: 
                        enemies[enemyIndex].facing = UP;    
                        break;
                    case 6: 
                        enemies[enemyIndex].facing = RIGHT; 
                        break;
                    case 7: 
                        enemies[enemyIndex].facing = DOWN;  
                        break;
                    case 8: 
                        enemies[enemyIndex].facing = LEFT;  
                        break;
                }
                
                enemyIndex++;
            }
            else if (t == 9) {
                enemies[enemyIndex].rec.x = x * TILE_SIZE + (TILE_SIZE - 80) * 0.5f;
                enemies[enemyIndex].rec.y = y * TILE_SIZE + (TILE_SIZE - 80) * 0.5f;
                enemies[enemyIndex].rec.width  = 80;
                enemies[enemyIndex].rec.height = 80;
                enemies[enemyIndex].speed = {1.2f, 1.2f};  // slow
                enemies[enemyIndex].color = (Color){180, 30, 30, 255};
                enemies[enemyIndex].facing = DOWN;
                enemies[enemyIndex].active = true;
                enemies[enemyIndex].isShooter = true;      
                enemies[enemyIndex].isBoss = true;
                enemies[enemyIndex].shootTimer = 120;      
                enemies[enemyIndex].health = 40;
                enemies[enemyIndex].maxHealth = 40;
                enemyIndex++;
            }
        }
    }
}

static void ResetToFloor1KeepMoney(void)
{
    int savedMoney = player.currency;

    gFloor = 1;
    gHasKey = false;
    player.health = 3;
    player.ammo   = 50;
    player.iframes = 0;
    gBulletSpeed = 7;
    gFireDelay   = 20;

    player.currency = savedMoney;

    InitMission();

    gState = STATION;
}

static void UpdateGame(void) 
{
    int (*currMap)[MAP_WIDTH] = GetCurrentMap();
    
    if (IsKeyPressed(KEY_ESCAPE) && gState == MISSION) {
        gState = PAUSE;
        return;
    }

    if (!gHasKey) {
        if (CheckCollisionCircleRec(gKeyPos, gKeyRadius, player.rec)) {
            gHasKey = true;
            // maybe play sound later
        }
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
                if (Map_IsTileSolid(currMap, tx, ty)) {
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
                if (Map_IsTileSolid(currMap, tx, ty)) {
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

    float px = player.rec.x + player.rec.width * 0.5f;
    float py = player.rec.y + player.rec.height * 0.5f;

    int tx = (int)(px / TILE_SIZE);
    int ty = (int)(py / TILE_SIZE);

    if (gHasKey && !Map_IsTileSolid(currMap, tx, ty)) {
        int (*currentMap)[MAP_WIDTH] = GetCurrentMap();
        int t = currentMap[ty][tx];
        if (t == 2) {
            gFloor++;
            InitMission();
            player.currency += 5;
        }
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

        // shooter first
        if (enemies[i].isShooter) {
            if (enemies[i].shootTimer > 0) {
                enemies[i].shootTimer--;
            } else {
                EnemyFireBullets(enemies[i], 5);
                enemies[i].shootTimer = 180;
            }
        }

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
                    if (Map_IsTileSolid(currMap, tx, ty)) {
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
                    if (Map_IsTileSolid(currMap, tx, ty)) {
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

        if (enemies[i].rec.x < 0) { 
            enemies[i].rec.x = 0; enemies[i].speed.x *= -1; 
        }
        if (enemies[i].rec.x + enemies[i].rec.width > gScreenWidth) {
            enemies[i].rec.x = gScreenWidth - enemies[i].rec.width;
            enemies[i].speed.x *= -1;
        }
        if (enemies[i].rec.y < 0) { 
            enemies[i].rec.y = 0; enemies[i].speed.y *= -1; 
        }
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
        if (!shoot[i].fromPlayer) {
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
                        player.rec.x += knock.x * 10.0f;
                        player.rec.y += knock.y * 10.0f;
                    }
                    player.rec.x += knock.x * 10.0f;
                    player.rec.y += knock.y * 10.0f;
    
                    if (player.health <= 0) {
                        gState = GAMEOVER;
                        return;
                    }
                }
            }
        }

        // enemy hit by bullet (only player bullets = MAROON)
        for (int j = 0; j < NUM_SHOOTS; j++) {
            if (!shoot[j].active) continue;
            if (!shoot[j].fromPlayer) continue;

            // check color manually
            Color c = shoot[j].color;
            bool isPlayerBullet =
                (c.r == MAROON.r) &&
                (c.g == MAROON.g) &&
                (c.b == MAROON.b) &&
                (c.a == MAROON.a);

            if (!isPlayerBullet) continue;

            if (CheckCollisionCircleRec(enemyCenter, enemies[i].rec.width/2, shoot[j].rec)) {
                shoot[j].active = false;

            if (enemies[i].isBoss) {
                    enemies[i].health -= 1;
                    if (enemies[i].health <= 0) {
                        enemies[i].active = false;
                        player.currency += 50;   
                        gHasKey = true;          
                    }
                } else {
                    enemies[i].active = false;
                    player.currency += 5;
                }
            }
        }

            // BOSS SPECIAL LOGIC
        if (enemies[i].isBoss) {
            if (enemies[i].shootTimer > 0) {
                enemies[i].shootTimer--;
            } else {
                BossFireCircle(enemies[i]);
                enemies[i].shootTimer = 150;
            }
        } else {
            if (enemies[i].isShooter) {
                if (enemies[i].shootTimer > 0) {
                    enemies[i].shootTimer--;
                } else {
                    EnemyFireBullets(enemies[i], 5);
                    enemies[i].shootTimer = 180;
                }
            }
        }
    }

    // shooting
    if (IsKeyDown(KEY_SPACE)) {
        shootRate += 3;
        if (shootRate % gFireDelay == 0 && player.ammo > 0) {
            for (int i = 0; i < NUM_SHOOTS; i++) {
                if (!shoot[i].active) {
                    shoot[i].rec.x = player.rec.x;
                    shoot[i].rec.y = player.rec.y + player.rec.height/4;
                    shoot[i].active = true;
                    shoot[i].facing = player.facing;
                    shoot[i].speed = {0,0};
                    shoot[i].color = MAROON;
                    shoot[i].fromPlayer = true;
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
            case RIGHT: shoot[i].rec.x += gBulletSpeed; break;
            case LEFT:  shoot[i].rec.x -= gBulletSpeed; break;
            case UP:    shoot[i].rec.y -= gBulletSpeed; break;
            case DOWN:  shoot[i].rec.y += gBulletSpeed; break;
        }

        // check tile the bullet is now inside
        float bx = shoot[i].rec.x + shoot[i].rec.width  * 0.5f;
        float by = shoot[i].rec.y + shoot[i].rec.height * 0.5f;

        int tx = (int)(bx / TILE_SIZE);
        int ty = (int)(by / TILE_SIZE);

        if (Map_IsTileSolid(currMap, tx, ty)) {
            // bullet hit wall -> destroy
            shoot[i].active = false;
            continue;
        }

        // off-screen deactivate
        if (shoot[i].rec.x > gScreenWidth || shoot[i].rec.x + shoot[i].rec.width < 0 ||
            shoot[i].rec.y > gScreenHeight || shoot[i].rec.y + shoot[i].rec.height < 0) {
            shoot[i].active = false;
        }

        // identify if this bullet is from an enemy (RED)
        Color c = shoot[i].color;
        bool isEnemyBullet =
            (c.r == RED.r) &&
            (c.g == RED.g) &&
            (c.b == RED.b) &&
            (c.a == RED.a);
    
        if (isEnemyBullet) {
            if (CheckCollisionRecs(shoot[i].rec, player.rec)) {
                if (player.iframes <= 0) {
                    player.health -= 1;
                    player.iframes = 60;
    
                    Vector2 knock = {
                        (player.rec.x + player.rec.width/2)  - (shoot[i].rec.x + shoot[i].rec.width/2),
                        (player.rec.y + player.rec.height/2) - (shoot[i].rec.y + shoot[i].rec.height/2)
                    };
                    float mag = sqrtf(knock.x*knock.x + knock.y*knock.y);
                    if (mag > 0.0f) {
                        knock.x /= mag;
                        knock.y /= mag;
                        player.rec.x += knock.x * 10.0f;
                        player.rec.y += knock.y * 10.0f;
                    }
    
                    // bullet is gone
                    shoot[i].active = false;
    
                    // death check
                    if (player.health <= 0) {
                        gState = GAMEOVER;
                        return;
                    }
                } else {
                    shoot[i].active = false;
                }
            }
        }
    }

    if (player.ammo <= 0 || player.health <= 0) {
        gState = GAMEOVER;
        return;
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
    
    snprintf(buf, sizeof(buf), "Floor: %d", gFloor);
    DrawText(buf, 10, 130, 20, DARKGRAY);

    // draw boss HP bar (first active boss we find)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].isBoss) {
            float barW = 400;
            float barH = 20;
            float x = (gScreenWidth - barW) * 0.5f;
            float y = 560; // near bottom, or put at 10

            DrawRectangle(x, y, barW, barH, DARKGRAY);
            float pct = (float)enemies[i].health / (float)enemies[i].maxHealth;
            DrawRectangle(x, y, barW * pct, barH, RED);
            DrawRectangleLines(x, y, barW, barH, BLACK);
            DrawText("BOSS", x, y - 20, 20, RED);
            break;
        }
    }

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
            
            int (*currentMap)[MAP_WIDTH] = GetCurrentMap();
            int t = currentMap[y][x];
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

    // Draw Key 
    if (!gHasKey) {
        DrawCircleV(gKeyPos, gKeyRadius, GOLD);
        DrawCircleLines(gKeyPos.x, gKeyPos.y, gKeyRadius, RAYWHITE);
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
        gState = INSTRUCTIONS;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        gState = START;
    }
}

static void UpdateShop(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }

    // 1) Health
    if (IsKeyPressed(KEY_ONE)) {
        if (player.currency >= 500) {
            player.currency -= 500;
            player.health += 1;
        }
    }

    if (IsKeyPressed(KEY_TWO)) {
        if (player.currency >= 5) {
            player.currency -= 5;
            player.ammo += 10;
        }
    }

    if (IsKeyPressed(KEY_THREE)) {
        int cost = 15;
        if (player.currency >= cost) {
            player.currency -= cost;
            if (gBulletSpeed < 15) {
                gBulletSpeed += 1;
            }
        }
    }

    if (IsKeyPressed(KEY_FOUR)) {
        int cost = 20;
        if (player.currency >= cost) {
            if (gFireDelay > 4) {
                player.currency -= cost;
                gFireDelay -= 1;
            }
        }
    }
}


static void DrawShop(void)
{
    BeginDrawing();
    ClearBackground((Color){12, 12, 24, 255});
    DrawText("SHOP (ESC to return)", 40, 40, 30, RAYWHITE);

    DrawText("1) +1 Health (500 cr)", 60, 110, 20, RAYWHITE);
    DrawText("2) +10 Ammo (5 cr)",   60, 140, 20, RAYWHITE);
    DrawText("3) Bullet Speed +1 (15 cr)", 60, 170, 20, RAYWHITE);
    DrawText("4) Fire Rate + (20 cr)",     60, 200, 20, RAYWHITE);

    // current stats
    char buf[64];
    snprintf(buf, sizeof(buf), "Credits: %d", player.currency);
    DrawText(buf, 60, 240, 20, GOLD);

    snprintf(buf, sizeof(buf), "Ammo: %d", player.ammo);
    DrawText(buf, 60, 270, 20, RAYWHITE);

    snprintf(buf, sizeof(buf), "Bullet speed: %d", gBulletSpeed);
    DrawText(buf, 60, 300, 20, RAYWHITE);

    snprintf(buf, sizeof(buf), "Fire delay: %d (lower = faster)", gFireDelay);
    DrawText(buf, 60, 330, 18, RAYWHITE);

    EndDrawing();
}

static void UpdatePause(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = MISSION;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        gState = STATION;
    }
}

static void DrawPause(void)
{
    BeginDrawing();
    DrawRectangle(0, 0, gScreenWidth, gScreenHeight, Fade(BLACK, 0.6f));

    DrawText("PAUSED", gScreenWidth/2 - 80, gScreenHeight/2 - 80, 40, RAYWHITE);
    DrawText("Press ESC to Resume", gScreenWidth/2 - 120, gScreenHeight/2, 20, GRAY);
    DrawText("Press ENTER to Return to Station", gScreenWidth/2 - 170, gScreenHeight/2 + 40, 20, GRAY);

    EndDrawing();
}

static void UpdateQuit(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = START;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        CloseWindow();
        exit(0); 
    }
}

static void DrawQuit(void)
{
    BeginDrawing();
    DrawRectangle(0, 0, gScreenWidth, gScreenHeight, Fade(BLACK, 0.6f));

    DrawText("QUIT", gScreenWidth/2 - 80, gScreenHeight/2 - 80, 40, RAYWHITE);
    DrawText("Press ESC to resume", gScreenWidth/2 - 120, gScreenHeight/2, 20, GRAY);
    DrawText("Press ENTER to quit the game", gScreenWidth/2 - 170, gScreenHeight/2 + 40, 20, GRAY);

    EndDrawing();
}

static void UpdateInstructions(void)
{
    // Any key press starts the game
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ONE))
    {
        InitMission();
        gState = MISSION;
    }
}

static void DrawInstructions(void)
{
    BeginDrawing();
    ClearBackground((Color){5, 8, 20, 255});

    DrawText("HOW TO PLAY", gScreenWidth/2 - 100, 60, 40, GOLD);

    int x = 80;
    int y = 140;
    int line = 30;

    DrawText("WASD  - Move your ship", x, y, 20, RAYWHITE);
    DrawText("Arrow Keys - Aim direction", x, y + line, 20, RAYWHITE);
    DrawText("SPACE - Shoot (uses ammo)", x, y + line*2, 20, RAYWHITE);
    DrawText("Collect BLUE for ammo", x, y + line*3, 20, SKYBLUE);
    DrawText("Collect YELLOW for money", x, y + line*4, 20, YELLOW);
    DrawText("Get the GOLD key to use the teleporter", x, y + line*5, 20, GOLD);
    DrawText("Avoid RED bullets and enemies!", x, y + line*6, 20, RED);
    DrawText("If you run out of ammo OR run out of HP,", x, y + line*7, 20, RAYWHITE);
    DrawText("you restart at Floor 1 with only your money!", x, y + line*8, 20, RAYWHITE);
    DrawText("Press ENTER to begin your mission", gScreenWidth/2 - 180, y + line*10, 20, GRAY);

    EndDrawing();
}

static void UpdateStart(void)
{
    // ENTER starts the game, you could also allow SPACE
    if (IsKeyPressed(KEY_ENTER))
    {
        gState = STATION;   // go to your hub
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        gState = QUIT;
    }
}

static void DrawStart(void)
{
    BeginDrawing();
    ClearBackground((Color){ 6, 9, 20, 255 });

    // Title
    const char *title = "SPACE STATION STRIKER"; // change to your game name
    int titleFont = 40;
    int titleWidth = MeasureText(title, titleFont);
    DrawText(title, gScreenWidth/2 - titleWidth/2, 120, titleFont, RAYWHITE);

    // Subtitle
    const char *sub = "Top-down missions, credits, upgrades.";
    int subFont = 20;
    int subWidth = MeasureText(sub, subFont);
    DrawText(sub, gScreenWidth/2 - subWidth/2, 180, subFont, GRAY);

    // Instructions
    DrawText("Press ENTER to start", gScreenWidth/2 - 130, 280, 24, GOLD);
    DrawText("Press ESC to quit",   gScreenWidth/2 - 100, 320, 20, GRAY);

    // Optional: quick controls preview
    DrawText("WASD to move  |  Arrows to aim  |  SPACE to shoot", 
             gScreenWidth/2 - 240, 380, 18, (Color){180, 180, 200, 255});

    EndDrawing();
}
