// Persistent player profile — file on desktop, localStorage in the browser.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Profile {
    int deaths = 0;
    int daysTotal = 0;
    int bestDays = 0;
    int booksRead = 0;
    int ambitionsDone = 0;
    int livesCompleted = 0;
    int wordsLearned = 0; // lexicon of the old tongue, kept across runs (R4)
    // Deeds that unlock starts (R7).
    int beastsSlain = 0;
    int miracles = 0;
    int horizons = 0;    // sailed beyond the chart
    int guildmaster = 0; // took the chair
    int vendettas = 0;   // grudges settled
    // Settings. textSpeed: 0 slow, 1 normal, 2 fast, 3 instant.
    // volume: 0 low, 1 normal, 2 high.
    int textSpeed = 1;
    int volume = 2;
    bool musicOff = true;
    bool reducedMotion = false;
    bool analyticsOff = false;
    bool seenIntro = false;

    std::string toJson() const;
    static Profile fromJson(const std::string& text);
};

Profile LoadProfile();
void SaveProfile(const Profile& p);

// A dead character, remembered by the world that killed them (ROADMAP P3).
struct LegacyRecord {
    std::string name;      // conlang name
    std::string meaning;
    std::string epitaph;
    std::string deathSite;
    int days = 0;
    bool blessing = false; // an afterlife bargain: the heir starts blessed
    // Up to two notable carried items become findable relics.
    std::vector<std::pair<std::string, std::string>> relics; // {name, quirk}
};

// Legacies are stored per world seed. Only the most recent worlds are kept.
std::vector<LegacyRecord> LoadLegacy(uint64_t seed);
void AppendLegacy(uint64_t seed, const LegacyRecord& rec);
// Every stored life across every remembered world, oldest world first (R5).
std::vector<std::pair<uint64_t, LegacyRecord>> LoadAllLegacy();

// Mid-run autosave: an opaque JSON blob owned by the Game (R3).
std::string LoadRawRun();
void SaveRawRun(const std::string& text); // "" clears

// NPC memory that outlives you: marks persist per world seed (R3).
std::string LoadMarks(uint64_t seed);
void SaveMarks(uint64_t seed, const std::string& json);
