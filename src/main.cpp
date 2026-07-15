// Random Rogue — entry point. Owns the window and the 320x180 virtual canvas;
// the Game class does everything else.

#include "raylib.h"
#include "game.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

namespace {

constexpr int kVirtualW = 320;
constexpr int kVirtualH = 180;

RenderTexture2D gCanvas;
Game gGame;

void UpdateDrawFrame() {
    // Window-space -> virtual-canvas-space for the mouse.
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int scale = 1;
    while ((scale + 1) * kVirtualW <= screenW && (scale + 1) * kVirtualH <= screenH) scale++;
    int drawW = kVirtualW * scale;
    int drawH = kVirtualH * scale;
    int offX = (screenW - drawW) / 2;
    int offY = (screenH - drawH) / 2;
    Vector2 m = GetMousePosition();
    Vector2 vm = {(m.x - offX) / scale, (m.y - offY) / scale};

    BeginTextureMode(gCanvas);
    gGame.frame(vm);
    EndTextureMode();

    Rectangle src{0, 0, (float)kVirtualW, (float)-kVirtualH};
    Rectangle dst{(float)offX, (float)offY, (float)drawW, (float)drawH};
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(gCanvas.texture, src, dst, Vector2{0, 0}, 0.0f, WHITE);
    EndDrawing();
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(kVirtualW * 3, kVirtualH * 3, "Random Rogue");
    SetWindowMinSize(kVirtualW, kVirtualH);
    SetExitKey(KEY_NULL); // ESC shouldn't kill the game

#if !defined(PLATFORM_WEB)
    // Assets are copied next to the exe; run from anywhere.
    ChangeDirectory(GetApplicationDirectory());
#endif

    gCanvas = LoadRenderTexture(kVirtualW, kVirtualH);
    SetTextureFilter(gCanvas.texture, TEXTURE_FILTER_POINT);
    gGame.init();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    UnloadRenderTexture(gCanvas);
    CloseWindow();
    return 0;
}
