// Procedural chiptune. No audio assets exist: every track is a seeded
// square-wave loop generated at runtime, per location type — the recipe
// philosophy applied to sound. SFX are tiny synthesized waves.
#pragma once
#include <map>
#include <string>
#include "raylib.h"
#include "rng.h"

class AudioBank {
public:
    void init();
    void shutdown();
    // Play (generating on first request) the loop for a location tag.
    // Seed makes each world's soundtrack its own.
    void playMusicFor(const std::string& tag, uint64_t worldSeed);
    void update(); // call once per frame: restarts loop, applies mute
    void toggleMute();
    bool muted() const { return muted_; }

    void blip();   // UI select
    void coin();   // money gained
    void thud();   // damage
    void dice();   // check rolled
    void chime();  // item gained

private:
    Sound generateTrack(const std::string& tag, uint64_t seed);
    Sound makeSfx(float baseFreq, float endFreq, float seconds, int kind);

    std::map<std::string, Sound> tracks_;
    std::string currentTag_;
    Sound* current_ = nullptr;
    Sound sBlip_{}, sCoin_{}, sThud_{}, sDice_{}, sChime_{};
    bool ready_ = false;
    bool muted_ = false;
};
