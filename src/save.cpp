#include "save.h"
#include <nlohmann/json.hpp>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include "raylib.h"
#endif

using json = nlohmann::json;

std::string Profile::toJson() const {
    json j = {{"deaths", deaths},
              {"daysTotal", daysTotal},
              {"bestDays", bestDays},
              {"booksRead", booksRead},
              {"ambitionsDone", ambitionsDone},
              {"livesCompleted", livesCompleted}};
    return j.dump();
}

Profile Profile::fromJson(const std::string& text) {
    Profile p;
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return p;
    p.deaths = j.value("deaths", 0);
    p.daysTotal = j.value("daysTotal", 0);
    p.bestDays = j.value("bestDays", 0);
    p.booksRead = j.value("booksRead", 0);
    p.ambitionsDone = j.value("ambitionsDone", 0);
    p.livesCompleted = j.value("livesCompleted", 0);
    return p;
}

// ---- world legacies ---------------------------------------------------------

static json legacyToJson(const LegacyRecord& r) {
    json items = json::array();
    for (auto& [name, quirk] : r.relics) items.push_back({{"name", name}, {"quirk", quirk}});
    return {{"name", r.name},   {"meaning", r.meaning},   {"epitaph", r.epitaph},
            {"deathSite", r.deathSite}, {"days", r.days}, {"relics", items}};
}

static LegacyRecord legacyFromJson(const json& j) {
    LegacyRecord r;
    r.name = j.value("name", "");
    r.meaning = j.value("meaning", "");
    r.epitaph = j.value("epitaph", "");
    r.deathSite = j.value("deathSite", "");
    r.days = j.value("days", 0);
    if (j.contains("relics"))
        for (auto& it : j["relics"])
            r.relics.emplace_back(it.value("name", ""), it.value("quirk", ""));
    return r;
}

static std::string loadRawStore(const char* key);
static void saveRawStore(const char* key, const std::string& text);

std::vector<LegacyRecord> LoadLegacy(uint64_t seed) {
    std::vector<LegacyRecord> out;
    json j = json::parse(loadRawStore("random_rogue_worlds"), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return out;
    std::string key = std::to_string(seed);
    if (!j.contains(key)) return out;
    for (auto& r : j[key]) out.push_back(legacyFromJson(r));
    return out;
}

void AppendLegacy(uint64_t seed, const LegacyRecord& rec) {
    json j = json::parse(loadRawStore("random_rogue_worlds"), nullptr, false);
    if (j.is_discarded() || !j.is_object()) j = json::object();
    std::string key = std::to_string(seed);
    if (!j.contains(key)) j[key] = json::array();
    j[key].push_back(legacyToJson(rec));
    // Ghosts per world capped (the sim only injects so many); worlds capped too.
    while (j[key].size() > 12) j[key].erase(0);
    while (j.size() > 6) j.erase(j.begin());
    saveRawStore("random_rogue_worlds", j.dump());
}

#if defined(__EMSCRIPTEN__)

EM_JS(char*, rr_load_profile, (), {
    var s = null;
    try { s = window.localStorage.getItem('random_rogue_profile'); } catch (e) {}
    if (!s) s = "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});

EM_JS(void, rr_save_profile, (const char* text), {
    try { window.localStorage.setItem('random_rogue_profile', UTF8ToString(text)); } catch (e) {}
});

EM_JS(char*, rr_load_store, (const char* key), {
    var s = null;
    try { s = window.localStorage.getItem(UTF8ToString(key)); } catch (e) {}
    if (!s) s = "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});

EM_JS(void, rr_save_store, (const char* key, const char* text), {
    try { window.localStorage.setItem(UTF8ToString(key), UTF8ToString(text)); } catch (e) {}
});

static std::string loadRawStore(const char* key) {
    char* raw = rr_load_store(key);
    std::string text(raw ? raw : "");
    free(raw);
    return text;
}

static void saveRawStore(const char* key, const std::string& text) {
    rr_save_store(key, text.c_str());
}

Profile LoadProfile() {
    char* raw = rr_load_profile();
    std::string text(raw ? raw : "");
    free(raw);
    return Profile::fromJson(text);
}

void SaveProfile(const Profile& p) {
    rr_save_profile(p.toJson().c_str());
}

#else

static std::string profilePath() {
    return std::string(GetApplicationDirectory()) + "profile.json";
}

static std::string loadRawStore(const char* key) {
    std::string path = std::string(GetApplicationDirectory()) + key + ".json";
    char* raw = LoadFileText(path.c_str());
    if (!raw) return "";
    std::string text(raw);
    UnloadFileText(raw);
    return text;
}

static void saveRawStore(const char* key, const std::string& text) {
    std::string path = std::string(GetApplicationDirectory()) + key + ".json";
    SaveFileText(path.c_str(), (char*)text.c_str());
}

Profile LoadProfile() {
    char* raw = LoadFileText(profilePath().c_str());
    if (!raw) return Profile{};
    Profile p = Profile::fromJson(raw);
    UnloadFileText(raw);
    return p;
}

void SaveProfile(const Profile& p) {
    std::string text = p.toJson();
    SaveFileText(profilePath().c_str(), (char*)text.c_str());
}

#endif
