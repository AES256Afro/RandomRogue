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
    void dirge();   // death / the afterlife gate
    void fanfare(); // triumph: beast slain, career crowned

private:
    Sound generateTrack(const std::string& tag, uint64_t seed);
    Sound makeSfx(float baseFreq, float endFreq, float seconds, int kind);
    // A short note sequence (freq, steps) — musical stingers, not sweeps.
    Sound makeStinger(const float* freqs, int count, float stepSeconds);

    // Exactly ONE track lives in memory. Decoded audio is ~16x the source
    // wave (device-format resampling); caching tracks OOMs the wasm heap.
    std::string currentTag_;
    Sound current_{};
    bool hasTrack_ = false;
    Sound sBlip_{}, sCoin_{}, sThud_{}, sDice_{}, sChime_{};
    Sound sDirge_{}, sFanfare_{};
    bool ready_ = false;
    bool muted_ = false;
};
