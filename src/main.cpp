// Random Rogue — entry point. Owns the window and the 320x180 virtual canvas;
// the Game class does everything else.

#include <cmath>
#include "raylib.h"
#include "game.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace {

constexpr int kVirtualW = 320;
constexpr int kVirtualH = 180;

RenderTexture2D gCanvas;
Game gGame;

#if defined(PLATFORM_WEB)
// The GLFW mouse path is unreliable under emscripten, and touch needs first-
// class handling for iPad anyway — so the web build listens to the canvas
// directly and feeds the game one unified pointer.
float gPtrX = 0.0f, gPtrY = 0.0f; // canvas CSS coords
bool gPtrPressed = false;         // edge: a press happened since last frame

EM_BOOL onMouse(int type, const EmscriptenMouseEvent* e, void*) {
    gPtrX = (float)e->targetX;
    gPtrY = (float)e->targetY;
    if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) gPtrPressed = true;
    return EM_FALSE; // don't consume; raylib/GLFW may still want these
}

EM_BOOL onTouch(int type, const EmscriptenTouchEvent* e, void*) {
    if (e->numTouches > 0) {
        gPtrX = (float)e->touches[0].targetX;
        gPtrY = (float)e->touches[0].targetY;
    }
    if (type == EMSCRIPTEN_EVENT_TOUCHSTART) gPtrPressed = true;
    return EM_TRUE; // consume: no ghost mouse events / page gestures
}

// Hand a MEMFS file to the browser as a download (the death card).
EM_JS(void, rr_download_file, (const char* path, const char* fname), {
    try {
        var data = FS.readFile(UTF8ToString(path));
        var blob = new Blob([data], { type: 'image/png' });
        var a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = UTF8ToString(fname);
        a.click();
        setTimeout(function(){ URL.revokeObjectURL(a.href); }, 5000);
    } catch (e) {}
});
#endif

// The death card: the virtual canvas, upscaled 3x, saved as a PNG.
void ExportDeathCard() {
    Image img = LoadImageFromTexture(gCanvas.texture);
    ImageFlipVertical(&img); // render textures are stored upside down
    ImageResizeNN(&img, kVirtualW * 3, kVirtualH * 3);
#if defined(PLATFORM_WEB)
    ExportImage(img, "/death_card.png");
    rr_download_file("/death_card.png", "random_rogue_death.png");
#else
    ExportImage(img, "death_card.png"); // lands next to the exe
#endif
    UnloadImage(img);
}

void UpdateDrawFrame() {
    // Window-space -> virtual-canvas-space for the pointer.
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int intScale = 1;
    while ((intScale + 1) * kVirtualW <= screenW && (intScale + 1) * kVirtualH <= screenH)
        intScale++;
    // Integer scaling keeps pixels perfect on big screens; on small ones
    // (phones, split-screen iPad) it wastes most of the viewport, so fall
    // back to exact-fit fractional scaling below 2x.
    float scale = (float)intScale;
    if (intScale < 2) {
        float fit = fminf((float)screenW / kVirtualW, (float)screenH / kVirtualH);
        if (fit > 0.1f) scale = fit;
    }
    float drawW = kVirtualW * scale;
    float drawH = kVirtualH * scale;
    float offX = (screenW - drawW) / 2.0f;
    float offY = (screenH - drawH) / 2.0f;

#if defined(PLATFORM_WEB)
    // Canvas CSS pixels -> framebuffer pixels (they can differ on retina).
    double cssW = 1, cssH = 1;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    float sx = cssW > 0 ? (float)screenW / (float)cssW : 1.0f;
    float sy = cssH > 0 ? (float)screenH / (float)cssH : 1.0f;
    Vector2 m = {gPtrX * sx, gPtrY * sy};
    bool pressed = gPtrPressed;
    gPtrPressed = false;
#else
    Vector2 m = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
#endif
    Vector2 vm = {(m.x - offX) / scale, (m.y - offY) / scale};

    BeginTextureMode(gCanvas);
    gGame.frame(vm, pressed);
    EndTextureMode();

    // The frame the player asked to keep (death card) is now in the texture.
    if (gGame.consumeCardRequest()) ExportDeathCard();

    // Screen shake: jitter the blit, never the game state.
    float shake = gGame.shakeAmount();
    if (shake > 0.0f) {
        double t = GetTime();
        offX += sinf((float)t * 61.0f) * shake * scale;
        offY += cosf((float)t * 83.0f) * shake * scale;
    }

    Rectangle src{0, 0, (float)kVirtualW, (float)-kVirtualH};
    Rectangle dst{offX, offY, drawW, drawH};
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
    emscripten_set_mousedown_callback("#canvas", nullptr, 1, onMouse);
    emscripten_set_mousemove_callback("#canvas", nullptr, 1, onMouse);
    emscripten_set_touchstart_callback("#canvas", nullptr, 1, onTouch);
    emscripten_set_touchmove_callback("#canvas", nullptr, 1, onTouch);
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    gGame.shutdown();
    UnloadRenderTexture(gCanvas);
    CloseWindow();
    return 0;
}
