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
    // Up to two notable carried items become findable relics.
    std::vector<std::pair<std::string, std::string>> relics; // {name, quirk}
};

// Legacies are stored per world seed. Only the most recent worlds are kept.
std::vector<LegacyRecord> LoadLegacy(uint64_t seed);
void AppendLegacy(uint64_t seed, const LegacyRecord& rec);
