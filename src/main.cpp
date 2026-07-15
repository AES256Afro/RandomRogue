// Random Rogue — Milestone 0 skeleton.
// Proves: raylib window, 320x180 virtual resolution with integer scaling,
// pixel-crisp text, native + WASM builds from one codebase.

#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

namespace {

constexpr int kVirtualW = 320;
constexpr int kVirtualH = 180;

RenderTexture2D gCanvas;
double gStartTime = 0.0;

// Draw one frame into the virtual canvas, then blit it integer-scaled and
// centered onto the real window.
void UpdateDrawFrame() {
    // --- draw the game at 320x180 ---
    BeginTextureMode(gCanvas);
    ClearBackground(Color{24, 20, 37, 255}); // deep purple-black

    const char* title = "RANDOM ROGUE";
    int titleSize = 20;
    int titleW = MeasureText(title, titleSize);
    DrawText(title, (kVirtualW - titleW) / 2, 56, titleSize, Color{255, 205, 117, 255});

    // Blinking prompt, classic attract-screen style.
    double t = GetTime() - gStartTime;
    if (((long)(t * 2.0)) % 2 == 0) {
        const char* prompt = "press any key";
        int pSize = 10;
        int pW = MeasureText(prompt, pSize);
        DrawText(prompt, (kVirtualW - pW) / 2, 100, pSize, Color{139, 155, 180, 255});
    }

    const char* ver = "m0 skeleton";
    DrawText(ver, 4, kVirtualH - 12, 10, Color{90, 105, 136, 255});
    EndTextureMode();

    // --- blit to window, integer scale, letterboxed ---
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int scale = 1;
    while ((scale + 1) * kVirtualW <= screenW && (scale + 1) * kVirtualH <= screenH) scale++;
    int drawW = kVirtualW * scale;
    int drawH = kVirtualH * scale;
    Rectangle src{0, 0, (float)kVirtualW, (float)-kVirtualH}; // flip Y (render texture)
    Rectangle dst{(float)((screenW - drawW) / 2), (float)((screenH - drawH) / 2),
                  (float)drawW, (float)drawH};

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

    gCanvas = LoadRenderTexture(kVirtualW, kVirtualH);
    SetTextureFilter(gCanvas.texture, TEXTURE_FILTER_POINT); // crisp pixels
    gStartTime = GetTime();

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
