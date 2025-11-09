#include "maps.h"
#include "game.h"
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

static Texture2D spaceTiger;
static Texture2D earthCutout;
static Texture2D marsCutout;
static Texture2D jupiterCutout;
static Texture2D saturnCutout;
static Texture2D trumanPortrait;

// Sprites
static Texture2D tigerShip;
static Texture2D earthBouncer;
static Texture2D squirrelSheet;
static Texture2D marsBouncer;
static Texture2D martianSheet;
static Texture2D Key;
static Texture2D coin;
static Texture2D ammoTexture;

static Texture2D GetBouncerTextureForLevel(Levels level)
{
    switch (level) {
        case EARTH:   return earthBouncer;
        case MARS:    return marsBouncer;
        case JUPITER: /* fall through */
        case SATURN:  /* fall through */ 
        default:      return earthBouncer; // fallback
    }
}

static Texture2D GetShooterTextureForLevel(Levels level)
{
    switch (level) {
        case MARS:    return martianSheet;
        case EARTH:   return squirrelSheet;
        case JUPITER: // fall through
        case SATURN:  // fall through
        default:      return squirrelSheet;
    }
}

static int gScreenWidth  = 800;
static int gScreenHeight = 600;

static Player player = { 0 };
static Shoot shoot[NUM_SHOOTS] = { 0 };
static Pickup pickups[MAX_PICKUPS] = { 0 };
static Enemy enemies[MAX_ENEMIES] = { 0 };

static int shootRate = 0;
static int gPlayerBulletSpeed = 7;   
static const int gEnemyBulletSpeed = 7;
static int gFireDelay = 20;


static GameState gState = START;
static int gDamageFlashTimer = 0;

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
static void UpdateWinEarth(void);
static void DrawWinEarth(void);
static void UpdateStoryEarth(void);
static void DrawStoryEarth(void);
static void UpdateStoryMars(void);
static void DrawStoryMars(void);
static void UpdateWinMars(void);
static void DrawWinMars(void);
static bool IsTileSolid(int tx, int ty);
static bool GetRandomFreeTilePos(float *outX, float *outY);
static bool FindTileOfType(int tileValue, float *outX, float *outY);
static void ResetToFloor1KeepMoney(void);
static bool gMarsUnlocked = false;

Levels gCurrentLevel = EARTH;

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

static void SpawnPlayerBullet(void)
{
    if (player.ammo <= 0) return;

    for (int i = 0; i < NUM_SHOOTS; i++) {
        if (!shoot[i].active) {
            shoot[i].rec.x = player.rec.x;
            shoot[i].rec.y = player.rec.y + player.rec.height/4;
            shoot[i].active = true;
            shoot[i].facing = player.facing;
            shoot[i].speed = (Vector2){0,0};
            shoot[i].color = MAROON;
            shoot[i].fromPlayer = true;
            player.ammo--;
            break;
        }
    }
}

void InitGame(int screenWidth, int screenHeight)
{
    gScreenWidth = screenWidth;
    gScreenHeight = screenHeight;

    InitPlayerOnce();
    spaceTiger = LoadTexture("assets/tigerinspace.png");
    tigerShip = LoadTexture("assets/tigerinship.png");
    earthCutout = LoadTexture("assets/earthnobg.png");
    marsCutout = LoadTexture("assets/marsnobg.png");
    jupiterCutout = LoadTexture("assets/jupiternobg.png");
    saturnCutout = LoadTexture("assets/saturnnobg.png");
    trumanPortrait = LoadTexture("assets/trumanportrait.png");
    earthBouncer = LoadTexture("assets/earthslime.png");
    squirrelSheet = LoadTexture("assets/sqrls.png");
    marsBouncer = LoadTexture("assets/marsslime.png");
    martianSheet = LoadTexture("assets/martianSheet.png");
    Key = LoadTexture("assets/key.png");
    coin = LoadTexture("assets/coin.png");
    ammoTexture = LoadTexture("assets/ammo.png");
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
        case WIN_EARTH:
            UpdateWinEarth();
            break;
        case STORY_EARTH:
            UpdateStoryEarth();
            break;
        case STORY_MARS:
            UpdateStoryMars();
            break;
        case WIN_MARS:
            UpdateWinMars();
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
        case WIN_EARTH:
            DrawWinEarth();
            break;
        case STORY_EARTH:
            DrawStoryEarth();
            break;
        case STORY_MARS:
            DrawStoryMars();
            break;
        case WIN_MARS:
            DrawWinMars();
            break;
        case GAMEOVER:
            BeginDrawing();
            ClearBackground(BLACK);
            if (player.health <= 0)
                 DrawText("GAME OVER - You Died! Press ENTER for station", 80, 200, 20, RAYWHITE);
            else {
                DrawText("GAME OVER - press ENTER for station", 80, 200, 20, RAYWHITE);
            }
            EndDrawing();
            break;
    }
}

void GameUnload(void)
{
    UnloadTexture(spaceTiger);
    UnloadTexture(tigerShip);
    UnloadTexture(earthCutout);
    UnloadTexture(marsCutout);
    UnloadTexture(jupiterCutout);
    UnloadTexture(saturnCutout);
    UnloadTexture(trumanPortrait);
    UnloadTexture(earthBouncer);
    UnloadTexture(squirrelSheet);
    UnloadTexture(marsBouncer);
    UnloadTexture(martianSheet);
    UnloadTexture(Key);
    UnloadTexture(coin);
    UnloadTexture(ammoTexture);
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
    const int bullets = 12;
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
                float size = 20;
                float verticalOffset = 6.0f;
                enemies[enemyIndex].rec.x = x * TILE_SIZE + (TILE_SIZE - size) * 0.5f;
                enemies[enemyIndex].rec.y = y * TILE_SIZE + (TILE_SIZE - size) * 0.5f + verticalOffset;
                enemies[enemyIndex].rec.width  = size;
                enemies[enemyIndex].rec.height = size;
                enemies[enemyIndex].speed = {2, 2};
                enemies[enemyIndex].color = RED;
                enemies[enemyIndex].facing = DOWN;
                enemies[enemyIndex].active = true;
                enemies[enemyIndex].isShooter = false;
                enemies[enemyIndex].isBoss = false;
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
                enemies[enemyIndex].isBoss = false;
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
    // If player had bought health upgrades, don't reset below 3
    if (player.health < 3) {
        player.health = 3;
    }
    if (player.ammo < 50) {
        player.ammo = 50;
    }
    player.iframes = 0;
    gFireDelay   = 20;

    player.currency = savedMoney;

    InitMission();

    gState = STATION;
}

static int CountActiveAmmoPickups(void)
{
    int count = 0;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (pickups[i].active && pickups[i].type == AMMO) {
            count++;
        }
    }
    return count;
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
        if (gCurrentLevel == EARTH && gFloor >= MAP_COUNT) {
            gMarsUnlocked = true;

            player.currency += 50;

            gFloor = 1;
            gHasKey = false;
            gState = WIN_EARTH;
        }
        else if (gCurrentLevel == MARS && gFloor >= MAP_COUNT) {
            gMarsUnlocked = true;

            player.currency += 50;

            gFloor = 1;
            gHasKey = false;
            gState = WIN_MARS;
        }
        else {
            gFloor++;
            InitMission();
            player.currency += 5;
        }
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

        bool isBoss   = enemies[i].isBoss;
        bool isShooter= enemies[i].isShooter;

        Texture2D bouncerTex = GetBouncerTextureForLevel(gCurrentLevel);

        bool isBouncer =
            (!enemies[i].isBoss &&
            !enemies[i].isShooter &&
            (fabsf(enemies[i].speed.x) > 0.0f || fabsf(enemies[i].speed.y) > 0.0f));

        if (isBouncer && bouncerTex.id > 0) {
            float scale = enemies[i].rec.width / (float)bouncerTex.width * 1.4f;
            float w = bouncerTex.width * scale;
            float h = bouncerTex.height * scale;

            Rectangle dest = {
                enemies[i].rec.x + enemies[i].rec.width  / 2.0f - w/2,
                enemies[i].rec.y + enemies[i].rec.height / 2.0f - h/2,
                w, h
            };

            DrawTexturePro(
                bouncerTex,
                (Rectangle){0,0,(float)bouncerTex.width,(float)bouncerTex.height},
                dest,
                (Vector2){0,0},
                0.0f,
                RAYWHITE
            );
        }
        else {
            DrawRectangleRec(enemies[i].rec, enemies[i].color);
        }

        // shooter first
        if (enemies[i].isShooter && !enemies[i].isBoss) {
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
                    gDamageFlashTimer = 20;
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
                        gMarsUnlocked = true;          
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
        } else if (enemies[i].isShooter) {
            if (enemies[i].shootTimer > 0) {
                enemies[i].shootTimer--;
            } else {
                EnemyFireBullets(enemies[i], 5);
                enemies[i].shootTimer = 180;
            }
        }
        
    }

    // shooting
    if (IsKeyPressed(KEY_SPACE)) {
        SpawnPlayerBullet();
    }
    if (IsKeyDown(KEY_SPACE)) {
        shootRate += 3;
        if (shootRate % gFireDelay == 0) {
            SpawnPlayerBullet();
        }
    } else {
        if (shootRate > 0) shootRate--;
    }


    // bullet movement
    for (int i = 0; i < NUM_SHOOTS; i++) {
        if (!shoot[i].active) continue;
        
        if (fabsf(shoot[i].speed.x) > 0.001f || fabsf(shoot[i].speed.y) > 0.001f) {
            shoot[i].rec.x += shoot[i].speed.x;
            shoot[i].rec.y += shoot[i].speed.y;
        } else {
            bool fromPlayer = shoot[i].fromPlayer;

            int speed = fromPlayer ? gPlayerBulletSpeed : gEnemyBulletSpeed;
            switch (shoot[i].facing)
            {
                case RIGHT: 
                    shoot[i].rec.x += speed; 
                    break;
                case LEFT:  
                    shoot[i].rec.x -= speed; 
                    break;
                case UP:    
                    shoot[i].rec.y -= speed; 
                    break;
                case DOWN:  
                    shoot[i].rec.y += speed; 
                    break;
            }
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
                    shoot[i].active = false;
                    gDamageFlashTimer = 20;
    
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

    if (player.health <= 0) {
        gState = GAMEOVER;
        return;
    }

    if (player.ammo <= 0) {
        int ammoLeft = CountActiveAmmoPickups();
        if (ammoLeft == 0) {
            gState = GAMEOVER;
            return;
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
            if(pickups[i].type == MONEY && coin.id > 0) {
                float desiredHeight = 28.0f; // same as your key sprite
                float scale = desiredHeight / (float)coin.height;

                float texW = coin.width * scale;
                float texH = coin.height * scale;

                Vector2 drawPos = {
                    pickups[i].position.x - texW / 2.0f,
                    pickups[i].position.y - texH / 2.0f
                };

                DrawTextureEx(coin, drawPos, 0.0f, scale, RAYWHITE);
            } else if (pickups[i].type == AMMO && ammoTexture.id > 0) {
                float desiredHeight = 28.0f; // same as your key sprite
                float scale = desiredHeight / (float)ammoTexture.height;

                float texW = ammoTexture.width * scale;
                float texH = ammoTexture.height * scale;

                Vector2 drawPos = {
                    pickups[i].position.x - texW / 2.0f,
                    pickups[i].position.y - texH / 2.0f
                };

                DrawTextureEx(ammoTexture, drawPos, 0.0f, scale, RAYWHITE);
            } else {
                DrawCircleV(pickups[i].position, pickups[i].radius, pickups[i].color);
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        bool isBoss    = enemies[i].isBoss;
        bool isShooter = enemies[i].isShooter;

        bool isBouncer = (!isBoss && !isShooter &&
                        (fabsf(enemies[i].speed.x) > 0.0f || fabsf(enemies[i].speed.y) > 0.0f));

        if (isBouncer) {
            Texture2D bouncerTex = GetBouncerTextureForLevel(gCurrentLevel);
            if (bouncerTex.id > 0) {
                float scale = (enemies[i].rec.width / (float)bouncerTex.width) * 2.0f;
                float w = bouncerTex.width * scale;
                float h = bouncerTex.height * scale;

                Rectangle dest = {
                    enemies[i].rec.x + enemies[i].rec.width  * 0.5f - w * 0.5f,
                    enemies[i].rec.y + enemies[i].rec.height * 0.5f - h * 0.5f,
                    w, h
                };

                DrawTexturePro(
                    bouncerTex,
                    (Rectangle){0, 0, (float)bouncerTex.width, (float)bouncerTex.height},
                    dest,
                    (Vector2){0, 0},
                    0.0f,
                    RAYWHITE
                );
                continue;
            }
        }

        // shooter squirrels
        if (enemies[i].isShooter) {
            Texture2D shooterTex = GetShooterTextureForLevel(gCurrentLevel);
            if (shooterTex.id > 0) {
                int cols = 2;
                int rows = 2;
                float frameW = shooterTex.width  / cols;
                float frameH = shooterTex.height / rows;

                int sx = 0;
                int sy = 0;

                switch (enemies[i].facing)
                {
                    case UP:    sx = 1; sy = 0; break; // back
                    case RIGHT: sx = 0; sy = 1; break; // right
                    case DOWN:  sx = 0; sy = 0; break; // front
                    case LEFT:  sx = 1; sy = 1; break; // left
                }

                Rectangle src = { sx * frameW, sy * frameH, frameW, frameH };

                float scale = enemies[i].rec.width / frameW * 1.6f;
                float destW = frameW * scale;
                float destH = frameH * scale;

                Rectangle dest = {
                    enemies[i].rec.x + enemies[i].rec.width  / 2.0f - destW / 2.0f,
                    enemies[i].rec.y + enemies[i].rec.height / 2.0f - destH / 2.0f,
                    destW,
                    destH
                };

                DrawTexturePro(shooterTex, src, dest, (Vector2){0,0}, 0.0f, RAYWHITE);
            }
        } else {
            DrawRectangleRec(enemies[i].rec, enemies[i].color);
        }
    }

    // --- HUD BAR ---
    int fontSize = 20;
    int padding = 20;
    int x = 10;
    int y = 10;
    char buf[64];

    snprintf(buf, sizeof(buf), "Ammo: %d", player.ammo);
    DrawText(buf, x, y, fontSize, SKYBLUE);
    x += MeasureText(buf, fontSize) + padding;

    snprintf(buf, sizeof(buf), "Health: %d", player.health);
    DrawText(buf, x, y, fontSize, RED);
    x += MeasureText(buf, fontSize) + padding;

    snprintf(buf, sizeof(buf), "Credits: %d", player.currency);
    DrawText(buf, x, y, fontSize, GOLD);
    x += MeasureText(buf, fontSize) + padding;

    snprintf(buf, sizeof(buf), "Floor: %d", gFloor);
    DrawText(buf, x, y, fontSize, RAYWHITE);


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
            if(gCurrentLevel == EARTH) {
                DrawText("EVIL SQUIRREL", x, y - 20, 20, RED);
            }
            if(gCurrentLevel == MARS) {
                DrawText("MARTIAN", x, y - 20, 20, RED);
            }
            break;
        }
    }

    if (gDamageFlashTimer > 0) {
        // fade out alpha (start strong red → transparent)
        float alpha = (float)gDamageFlashTimer / 20.0f; 
        DrawRectangle(0, 0, gScreenWidth, gScreenHeight, Fade(RED, alpha * 0.5f));
        gDamageFlashTimer--;
    }

    EndDrawing();
}

static void DrawMapCurrentRoom(void)
{
    int (*currentMap)[MAP_WIDTH] = GetCurrentMap();

    // --- Base color palette depends on current planet ---
    Color wallColor, floorColor, exitColor;

    switch (gCurrentLevel)
    {
        case EARTH:
            wallColor  = (Color){ 74, 56, 42, 255 };   // brown stone walls
            floorColor = (Color){ 46, 82, 53, 255 };   // green ground
            exitColor  = (Color){ 30, 180, 210, 200 }; // teal portal
            break;

        case MARS:
            wallColor  = (Color){ 100, 40, 30, 255 };  // rusty red rock
            floorColor = (Color){ 70, 35, 25, 255 };   // darker red soil
            exitColor  = (Color){ 255, 100, 50, 200 }; // orange portal glow
            break;

        case JUPITER:
            wallColor  = (Color){ 110, 90, 50, 255 };  // tan clouds
            floorColor = (Color){ 80, 60, 40, 255 };
            exitColor  = (Color){ 240, 210, 80, 200 };
            break;

        case SATURN:
            wallColor  = (Color){ 70, 70, 100, 255 };  // dark purple tones
            floorColor = (Color){ 50, 50, 80, 255 };
            exitColor  = (Color){ 150, 120, 255, 200 };
            break;

        default:
            wallColor  = DARKGRAY;
            floorColor = GRAY;
            exitColor  = SKYBLUE;
            break;
    }

    // --- Draw the tiles ---
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int t = currentMap[y][x];
            float tileX = (float)(x * TILE_SIZE);
            float tileY = (float)(y * TILE_SIZE);
            Rectangle tile = { tileX, tileY, (float)TILE_SIZE, (float)TILE_SIZE };

            switch (t)
            {
                case 1: // wall
                    DrawRectangleRec(tile, wallColor);
                    DrawRectangleLinesEx(tile, 2, BLACK);
                    break;

                case 2: // teleporter
                    if (gHasKey) {
                        DrawRectangleRec(tile, exitColor);
                        DrawRectangleLinesEx(tile, 2, RAYWHITE);
                    } else {
                        Color off = Fade(exitColor, 0.4f);
                        DrawRectangleRec(tile, off);
                        DrawRectangleLinesEx(tile, 2, DARKGRAY);
                        DrawRectangle(tile.x + tile.width*0.25f,
                                      tile.y + tile.height*0.4f,
                                      tile.width*0.5f,
                                      4,
                                      (Color){30,30,40,255});
                    }
                    break;

                default: // floor
                    DrawRectangleRec(tile, floorColor);
                    break;
            }
        }
    }

    if (!gHasKey && Key.id > 0) {
        float desiredHeight = 28.0f;
        float scale = desiredHeight / (float)Key.height;

        float texW = Key.width * scale;
        float texH = Key.height * scale;

        Vector2 drawPos = {
            gKeyPos.x - texW / 2.0f,
            gKeyPos.y - texH / 2.0f
        };

        DrawTextureEx(Key, drawPos, 0.0f, scale, RAYWHITE);
    }

}

static void DrawPlayer(const Player& player)
{
    // sprite sheet info
    int cols = 2;
    int rows = 2;
    float frameW = (float)tigerShip.width  / cols;
    float frameH = (float)tigerShip.height / rows;

    // pick frame based on facing
    int sx = 0;
    int sy = 0;
    // flipped UP/DOWN
    switch (player.facing)
    {
        case UP:    sx = 0; sy = 1; break;
        case RIGHT: sx = 1; sy = 0; break;
        case DOWN:  sx = 0; sy = 0; break;
        case LEFT:  sx = 1; sy = 1; break;
    }

    Rectangle src = {
        sx * frameW,
        sy * frameH,
        frameW,
        frameH
    };

    float visualScale = 2.0f;
    float visualW = player.rec.width  * visualScale;
    float visualH = player.rec.height * visualScale;

    Rectangle dest = {
        player.rec.x + player.rec.width  / 2.0f - visualW / 2.0f,
        player.rec.y + player.rec.height / 2.0f - visualH / 2.0f,
        visualW,
        visualH
    };

    DrawTexturePro(
        tigerShip,
        src,
        dest,
        (Vector2){0, 0},
        0.0f,
        RAYWHITE
    );
}


static void DrawStation(void)
{
    BeginDrawing();

    // pick a background per level (placeholder)
    Color bg = (Color){5, 8, 20, 255};
    switch (gCurrentLevel) {
        case EARTH:   bg = (Color){5, 8, 20, 255}; break;
        case MARS:    bg = (Color){40, 10, 10, 255}; break;
        case JUPITER: bg = (Color){40, 30, 10, 255}; break;
        case SATURN:  bg = (Color){20, 20, 35, 255}; break;
    }
    ClearBackground(bg);

    DrawText("SPACE STATION: ORBITAL HUB", 40, 40, 30, RAYWHITE);

    const char *levelName = "EARTH";
    bool levelLocked = false;
    switch (gCurrentLevel) {
        case EARTH:   
        levelName = "EARTH";   
        levelLocked = false; 
        if (earthCutout.id > 0) 
        {
            float earthScale = 1.3f;
            float texW = earthCutout.width * earthScale;
            float texH = earthCutout.height * earthScale;
            float yOffset = sinf(GetTime() * 2.0f) * 5.0f;

            DrawTextureEx(
                earthCutout,
                (Vector2){
                    gScreenWidth/2.0f - texW/2.0f, yOffset + 180
                },
                0.0f,
                earthScale,
                RAYWHITE
            );
        }
        break;

        case MARS:
        if (gMarsUnlocked) {
            levelName = "MARS";
            levelLocked = false;
        } else {
            levelName = "MARS (LOCKED)";
            levelLocked = true;
        }    
        
        if (marsCutout.id > 0) 
        {
            float marsScale = 1.3f;
            float texW = marsCutout.width * marsScale;
            float texH = marsCutout.height * marsScale;
            float yOffset = sinf(GetTime() * 2.0f) * 5.0f;

            DrawTextureEx(
                marsCutout,
                (Vector2){
                    gScreenWidth/2.0f - texW/2.0f, yOffset + 180
                },
                0.0f,
                marsScale,
                RAYWHITE
            );
        }  
        break;

        case JUPITER: 
        levelName = "JUPITER (LOCKED)"; 
        levelLocked = true;  
        if(jupiterCutout.id > 0) 
        {
            float jupiterScale = 1.3f;
            float texW = jupiterCutout.width * jupiterScale;
            float texH = jupiterCutout.height * jupiterScale;
            float yOffset = sinf(GetTime() * 2.0f) * 5.0f;

            DrawTextureEx(
                jupiterCutout,
                (Vector2){
                    gScreenWidth/2.0f - texW/2.0f, yOffset + 180
                },
                0.0f,
                jupiterScale,
                RAYWHITE
            );
        }
        break;
        
        case SATURN:  
        levelName = "SATURN (LOCKED)";  
        levelLocked = true;
        if (saturnCutout.id > 0) 
        {
            float saturnScale = 1.3f;
            float texW = saturnCutout.width * saturnScale;
            float texH = saturnCutout.height * saturnScale;
            float yOffset = sinf(GetTime() * 2.0f) * 5.0f;

            DrawTextureEx(
                saturnCutout,
                (Vector2){
                    gScreenWidth/2.0f - texW/2.0f, yOffset + 110
                },
                0.0f,
                saturnScale,
                RAYWHITE
            );
        }  
        break;
    }

    DrawText("Select Planet:", 60, 90, 20, RAYWHITE);
    DrawText(levelName, 60, 120, 28, GOLD);
    DrawText("<- / -> to change", 60, 155, 18, GRAY);

    DrawText("1) Shop", 60, 200, 20, RAYWHITE);
    DrawText("2) Launch Mission", 60, 230, 20, levelLocked ? DARKGRAY : RAYWHITE);

    char buf[64];
    snprintf(buf, sizeof(buf), "Credits: %d", player.currency);
    DrawText(buf, 60, 270, 20, GOLD);

    if (levelLocked) {
        if (gCurrentLevel == MARS) {
            // locked because Mars not unlocked yet
            DrawText("This planet is locked. Beat Earth boss to unlock Mars.", 
                    60, 305, 18, (Color){255, 200, 200, 255});
        } else {
            // generic locked text for other planets
            DrawText("This planet is locked. Only EARTH and MARS are playable right now.", 
                    60, 305, 18, (Color){255, 200, 200, 255});
        }
    }

    EndDrawing();
}


static void UpdateStation(void)
{
    // cycle left
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        if (gCurrentLevel == EARTH)
            gCurrentLevel = SATURN;
        else
            gCurrentLevel = (Levels)(gCurrentLevel - 1);
    }

    // cycle right
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        if (gCurrentLevel == SATURN)
            gCurrentLevel = EARTH;
        else
            gCurrentLevel = (Levels)(gCurrentLevel + 1);
    }

    // shop still works
    if (IsKeyPressed(KEY_ONE)) {
        gState = SHOP;
    }

    if (IsKeyPressed(KEY_TWO)) {
    if (gCurrentLevel == EARTH) {
        gState = STORY_EARTH;
    } else if (gCurrentLevel == MARS && gMarsUnlocked) {
        gFloor = 1;
        gState = STORY_MARS;
    } else {
        // still locked, do nothing (or show a message later)
    }
}


    if (IsKeyPressed(KEY_ESCAPE)) {
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
            if (gPlayerBulletSpeed < 15) {
                player.currency -= cost;
                gPlayerBulletSpeed += 1;
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

    snprintf(buf, sizeof(buf), "Bullet speed: %d", gPlayerBulletSpeed);
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
        ResetToFloor1KeepMoney();
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
    DrawText("If you run out of HP,", x, y + line*7, 20, RAYWHITE);
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

    if (spaceTiger.id > 0) 
    {
        float tigerScale = 1.0f;
        float texW = spaceTiger.width * tigerScale;
        float texH = spaceTiger.height * tigerScale;

        float yOffset = sinf(GetTime() * 2.0f) * 5.0f;

        DrawTextureEx(
            spaceTiger,
            (Vector2){
                gScreenWidth/2.0f - texW/2.0f, yOffset - 25
            },
            0.0f,
            tigerScale,
            RAYWHITE
        );
    }
    else
    {
        DrawText("tigerinspace.png not found", 20, gScreenHeight - 40, 20, RED);
    }
    
    const char *title = "Tiger Space Program";
    int titleFont = 40;
    int titleWidth = MeasureText(title, titleFont);
    DrawText(title, gScreenWidth/2 - titleWidth/2, 60, titleFont, RAYWHITE);

    const char *sub = "Top-down missions, credits, upgrades.";
    int subFont = 20;
    int subWidth = MeasureText(sub, subFont);
    DrawText(sub, gScreenWidth/2 - subWidth/2, 110, subFont, RAYWHITE);

    DrawText("Press ENTER to start", gScreenWidth/2 - 130, 160, 24, GOLD);
    DrawText("Press ESC to quit",   gScreenWidth/2 - 100, 195, 20, GRAY);

    EndDrawing();
}

static void UpdateWinEarth(void)
{
    // ENTER -> station
    if (IsKeyPressed(KEY_ENTER)) {
        ResetToFloor1KeepMoney();
        gState = STATION;
    }
    // ESC -> station too, why not
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

static void DrawWinEarth(void)
{
    BeginDrawing();
    ClearBackground((Color){8, 10, 20, 255});

    // Title
    DrawText("MISSION COMPLETE", 120, 80, 40, GOLD);

    // Narrative line — short story moment
    DrawText("Pilot Truman has once again saved a students", 100, 140, 22, RAYWHITE);
    DrawText("from the clutches of the evil cosmic squirrels!", 140, 170, 22, RAYWHITE);

    // Unlock message
    if (gMarsUnlocked) {
        DrawText("New destination unlocked: MARS", 130, 210, 22, ORANGE);
    }

    // Stats & reward line
    char buf[64];
    snprintf(buf, sizeof(buf), "Credits Earned: %d", player.currency);
    DrawText(buf, 130, 250, 20, GOLD);

    DrawText("Press ENTER to return to Station", 130, 300, 20, GRAY);

    // Optional: celebratory sparkle overlay
    float pulse = (sinf(GetTime() * 2.0f) + 1.0f) * 0.5f;
    Color flash = Fade(GOLD, 0.3f + 0.3f * pulse);
    DrawRectangle(0, 0, gScreenWidth, gScreenHeight, flash);

    EndDrawing();
}

static void UpdateStoryEarth(void)
{
    // ENTER or SPACE moves to the instructions/tutorial screen
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        gState = INSTRUCTIONS;
    }
    // ESC cancels back to the station
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

static void DrawStoryEarth(void)
{
    BeginDrawing();
    ClearBackground((Color){6, 9, 20, 255});

    DrawText("EARTH: THE FIRST RESCUE", gScreenWidth/2 - 190, 60, 32, GOLD);

    int x = 80;
    int y = 130;
    int line = 30;

    DrawText("Pilot Truman has set course for Earth,", x, y, 22, RAYWHITE);
    DrawText("where a group of students are trapped", x, y + line, 22, RAYWHITE);
    DrawText("in the ruins of an abandoned academy.", x, y + line*2, 22, RAYWHITE);

    DrawText("Reports speak of an evil cosmic beast", x, y + line*4, 22, RED);
    DrawText("lurking beneath the surface, feeding on fear.", x, y + line*5, 22, RAYWHITE);

    DrawText("Armed with courage, credits, and caffeine,", x, y + line*7, 22, RAYWHITE);
    DrawText("Truman descends through the atmosphere...", x, y + line*8, 22, RAYWHITE);

    DrawText("Press ENTER to begin your mission.", gScreenWidth/2 - 200, y + line*10, 20, GRAY);

    if (trumanPortrait.id > 0)
    {
        float scale = 0.25f;
        float texW = trumanPortrait.width * scale;
        float texH = trumanPortrait.height * scale;

        float posX = gScreenWidth - texW - 20;
        float posY = gScreenHeight - texH - 20;

        DrawTextureEx(trumanPortrait, (Vector2){ posX, posY }, 0.0f, scale, RAYWHITE);
    }

    EndDrawing();
}

static void UpdateStoryMars(void)
{
    // ENTER or SPACE -> instructions / mission
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        // start Mars mission
        gFloor = 1;
        InitMission();
        gState = MISSION;
    }
    // ESC -> back to station
    if (IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

static void DrawStoryMars(void)
{
    BeginDrawing();
    ClearBackground((Color){40, 10, 10, 255}); // Mars-y red

    DrawText("MARS: THE DUSTY OUTPOST", gScreenWidth/2 - 200, 60, 32, ORANGE);

    int x = 80;
    int y = 130;
    int line = 30;

    DrawText("Truman heads to Mars, following the beacon", x, y, 22, RAYWHITE);
    DrawText("of another stranded class of students.",     x, y + line, 22, RAYWHITE);
    DrawText("The tunnels are older, angrier... and alien.", x, y + line*2, 22, RAYWHITE);

    DrawText("Martian sentries guard the ruins.", x, y + line*4, 22, RED);
    DrawText("Take them out and grab the key.",   x, y + line*5, 22, RAYWHITE);

    DrawText("Press ENTER to begin your Mars mission.", gScreenWidth/2 - 230, y + line*8, 20, GRAY);

    EndDrawing();
}

static void UpdateWinMars(void)
{
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        gState = STATION;
    }
}

static void DrawWinMars(void)
{
    BeginDrawing();
    ClearBackground((Color){40, 10, 10, 255});

    DrawText("MARS MISSION COMPLETE", 120, 80, 40, ORANGE);
    DrawText("The martian patrols have been cleared.", 130, 140, 22, RAYWHITE);
    DrawText("Students are safe... for now.",           130, 170, 22, RAYWHITE);

    char buf[64];
    snprintf(buf, sizeof(buf), "Credits Earned: %d", player.currency);
    DrawText(buf, 130, 220, 22, GOLD);

    DrawText("Press ENTER to return to Station", 130, 270, 20, GRAY);

    EndDrawing();
}
