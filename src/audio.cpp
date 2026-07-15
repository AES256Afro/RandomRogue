#include "audio.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int kRate = 22050;

// Square wave with a little decay envelope — the whole chiptune food group.
void addNote(std::vector<short>& buf, int start, int len, float freq, float vol) {
    if (freq <= 0.0f) return; // rest
    float period = (float)kRate / freq;
    for (int i = 0; i < len && start + i < (int)buf.size(); i++) {
        float t = (float)i;
        float env = 1.0f - (t / (float)len) * 0.6f;
        float phase = fmodf(t, period) / period;
        float sq = phase < 0.5f ? 1.0f : -1.0f;
        int v = buf[start + i] + (int)(sq * vol * env * 32767.0f);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        buf[start + i] = (short)v;
    }
}

struct Personality {
    float baseFreq;   // tonic of the melody register
    int stepLen;      // samples per melody step (tempo)
    int restChance;   // % of steps that are silence
    bool minor;
};

Personality personalityFor(const std::string& tag) {
    if (tag == "dungeon") return {110.0f, kRate / 3, 30, true};
    if (tag == "cave")    return {98.0f,  kRate / 2, 45, true};
    if (tag == "tavern")  return {196.0f, kRate / 5, 10, false};
    if (tag == "city")    return {165.0f, kRate / 4, 15, false};
    if (tag == "forest")  return {147.0f, kRate / 4, 25, false};
    if (tag == "road")    return {131.0f, kRate / 4, 20, false};
    return {123.0f, kRate / 4, 20, true}; // title / default
}

} // namespace

void AudioBank::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) return;
    sBlip_ = makeSfx(660, 880, 0.06f, 0);
    sCoin_ = makeSfx(880, 1320, 0.09f, 0);
    sThud_ = makeSfx(120, 55, 0.16f, 1);
    sDice_ = makeSfx(440, 440, 0.05f, 2);
    sChime_ = makeSfx(523, 1047, 0.22f, 0);
}

void AudioBank::shutdown() {
    if (!ready_) return;
    if (hasTrack_) UnloadSound(current_);
    UnloadSound(sBlip_); UnloadSound(sCoin_); UnloadSound(sThud_);
    UnloadSound(sDice_); UnloadSound(sChime_);
    CloseAudioDevice();
}

Sound AudioBank::makeSfx(float baseFreq, float endFreq, float seconds, int kind) {
    int n = (int)(kRate * seconds);
    std::vector<short> buf(n, 0);
    Rng noise(12345, 7);
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float freq = baseFreq + (endFreq - baseFreq) * t;
        float env = 1.0f - t;
        float sample = 0;
        if (kind == 0) { // square sweep
            float period = (float)kRate / freq;
            sample = fmodf((float)i, period) / period < 0.5f ? 1.0f : -1.0f;
        } else if (kind == 1) { // sine thump
            sample = sinf(2.0f * PI * freq * i / kRate);
        } else { // noise rattle
            sample = ((int)(noise.next() % 2000) - 1000) / 1000.0f;
        }
        buf[i] = (short)(sample * env * 0.35f * 32767.0f);
    }
    Wave w{(unsigned)n, kRate, 16, 1, buf.data()};
    return LoadSoundFromWave(w);
}

Sound AudioBank::generateTrack(const std::string& tag, uint64_t seed) {
    Personality p = personalityFor(tag);
    Rng rng(seed ^ 0xC0FFEE, 55);
    // Pentatonic-ish scale; minor variant flattens the third.
    float scale[5] = {1.0f, p.minor ? 1.189f : 1.26f, 1.335f, 1.498f, 1.782f};

    const int steps = 32;
    int n = steps * p.stepLen;
    std::vector<short> buf(n, 0);

    // Melody: wandering walk over the scale, seeded per world.
    int deg = 0;
    for (int s = 0; s < steps; s++) {
        if (rng.chance(p.restChance)) continue;
        deg += rng.range(-2, 2);
        if (deg < 0) deg = 0;
        if (deg > 8) deg = 8;
        float mult = scale[deg % 5] * (deg >= 5 ? 2.0f : 1.0f);
        addNote(buf, s * p.stepLen, (int)(p.stepLen * 0.9f), p.baseFreq * 2.0f * mult, 0.10f);
    }
    // Bass: roots on the beat, an octave down, steady as a dungeon janitor.
    for (int s = 0; s < steps; s += 4) {
        float mult = scale[(s / 4) % 3];
        addNote(buf, s * p.stepLen, p.stepLen * 3, p.baseFreq * 0.5f * mult, 0.08f);
    }
    Wave w{(unsigned)n, kRate, 16, 1, buf.data()};
    return LoadSoundFromWave(w);
}

void AudioBank::playMusicFor(const std::string& tag, uint64_t worldSeed) {
    if (!ready_) return;
    std::string key = tag + "#" + std::to_string(worldSeed % 1000);
    if (key == currentTag_) return;
    if (hasTrack_) {
        StopSound(current_);
        UnloadSound(current_); // regeneration is cheap; heap is not
    }
    current_ = generateTrack(tag, worldSeed);
    hasTrack_ = true;
    currentTag_ = key;
    if (!muted_) PlaySound(current_);
}

void AudioBank::update() {
    if (!ready_ || !hasTrack_ || muted_) return;
    if (!IsSoundPlaying(current_)) PlaySound(current_); // loop, lofi seam included
}

void AudioBank::toggleMute() {
    muted_ = !muted_;
    if (!ready_ || !hasTrack_) return;
    if (muted_) StopSound(current_);
    else PlaySound(current_);
}

void AudioBank::blip()  { if (ready_ && !muted_) PlaySound(sBlip_); }
void AudioBank::coin()  { if (ready_ && !muted_) PlaySound(sCoin_); }
void AudioBank::thud()  { if (ready_ && !muted_) PlaySound(sThud_); }
void AudioBank::dice()  { if (ready_ && !muted_) PlaySound(sDice_); }
void AudioBank::chime() { if (ready_ && !muted_) PlaySound(sChime_); }
