#include "raylib.h"
#include "game.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "TigerHacks2025 Game");
    SetExitKey(KEY_NULL);

    InitGame(screenWidth, screenHeight);

    SetTargetFPS(60);

    bool running = true;
    while (running)
    {
        if (WindowShouldClose())
            running = false;

        GameUpdateDraw();
    }

    GameUnload();
    CloseWindow();
    return 0;
}
