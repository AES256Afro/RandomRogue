#include "save.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>

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
              {"livesCompleted", livesCompleted},
              {"wordsLearned", wordsLearned},
              {"beastsSlain", beastsSlain},
              {"miracles", miracles},
              {"horizons", horizons},
              {"guildmaster", guildmaster},
              {"vendettas", vendettas},
              {"textSpeed", textSpeed},
              {"volume", volume},
              {"musicOff", musicOff},
              {"reducedMotion", reducedMotion},
              {"seenIntro", seenIntro}};
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
    p.wordsLearned = j.value("wordsLearned", 0);
    p.beastsSlain = j.value("beastsSlain", 0);
    p.miracles = j.value("miracles", 0);
    p.horizons = j.value("horizons", 0);
    p.guildmaster = j.value("guildmaster", 0);
    p.vendettas = j.value("vendettas", 0);
    p.textSpeed = j.value("textSpeed", 1);
    p.volume = j.value("volume", 2);
    p.musicOff = j.value("musicOff", true);
    p.reducedMotion = j.value("reducedMotion", false);
    p.seenIntro = j.value("seenIntro", false);
    return p;
}

// ---- world legacies ---------------------------------------------------------

static json legacyToJson(const LegacyRecord& r) {
    json items = json::array();
    for (auto& [name, quirk] : r.relics) items.push_back({{"name", name}, {"quirk", quirk}});
    return {{"name", r.name},   {"meaning", r.meaning},   {"epitaph", r.epitaph},
            {"deathSite", r.deathSite}, {"days", r.days}, {"relics", items},
            {"blessing", r.blessing}};
}

static LegacyRecord legacyFromJson(const json& j) {
    LegacyRecord r;
    r.name = j.value("name", "");
    r.meaning = j.value("meaning", "");
    r.epitaph = j.value("epitaph", "");
    r.deathSite = j.value("deathSite", "");
    r.days = j.value("days", 0);
    r.blessing = j.value("blessing", false);
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
    // Ghosts per world capped (the sim only injects so many). Keep worlds by
    // actual recency, not JSON object's lexicographic seed order.
    while (j[key].size() > 12) j[key].erase(0);
    json order = j.value("__order", json::array());
    if (!order.is_array()) order = json::array();
    if (order.empty())
        for (auto& [oldKey, lives] : j.items())
            if (oldKey.rfind("__", 0) != 0 && lives.is_array()) order.push_back(oldKey);
    for (auto it = order.begin(); it != order.end();)
        if (it->is_string() && it->get<std::string>() == key) it = order.erase(it);
        else ++it;
    order.push_back(key);
    while (order.size() > 6) {
        std::string oldest = order.front().get<std::string>();
        order.erase(order.begin());
        j.erase(oldest);
    }
    j["__order"] = order;
    saveRawStore("random_rogue_worlds", j.dump());
}

std::vector<std::pair<uint64_t, LegacyRecord>> LoadAllLegacy() {
    std::vector<std::pair<uint64_t, LegacyRecord>> out;
    json j = json::parse(loadRawStore("random_rogue_worlds"), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return out;
    auto appendWorld = [&](const std::string& key) {
        if (key.rfind("__", 0) == 0 || !j.contains(key) || !j[key].is_array()) return;
        uint64_t seed = strtoull(key.c_str(), nullptr, 10);
        for (auto& r : j[key]) out.emplace_back(seed, legacyFromJson(r));
    };
    if (j.contains("__order") && j["__order"].is_array()) {
        for (auto& key : j["__order"])
            if (key.is_string()) appendWorld(key.get<std::string>());
    } else {
        // Backward-compatible import for saves written before recency metadata.
        for (auto& [key, lives] : j.items()) {
            (void)lives;
            appendWorld(key);
        }
    }
    return out;
}

std::string LoadRawRun() { return loadRawStore("random_rogue_run"); }
void SaveRawRun(const std::string& text) { saveRawStore("random_rogue_run", text); }

std::string LoadMarks(uint64_t seed) {
    json j = json::parse(loadRawStore("random_rogue_marks"), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return "{}";
    std::string key = std::to_string(seed);
    return j.contains(key) ? j[key].dump() : "{}";
}

void SaveMarks(uint64_t seed, const std::string& marksJson) {
    json j = json::parse(loadRawStore("random_rogue_marks"), nullptr, false);
    if (j.is_discarded() || !j.is_object()) j = json::object();
    json m = json::parse(marksJson, nullptr, false);
    if (m.is_discarded()) return;
    std::string key = std::to_string(seed);
    j[key] = m;
    json order = j.value("__order", json::array());
    if (!order.is_array()) order = json::array();
    if (order.empty())
        for (auto& [oldKey, oldMarks] : j.items())
            if (oldKey.rfind("__", 0) != 0 && oldMarks.is_object()) order.push_back(oldKey);
    for (auto it = order.begin(); it != order.end();)
        if (it->is_string() && it->get<std::string>() == key) it = order.erase(it);
        else ++it;
    order.push_back(key);
    while (order.size() > 6) {
        std::string oldest = order.front().get<std::string>();
        order.erase(order.begin());
        j.erase(oldest);
    }
    j["__order"] = order;
    saveRawStore("random_rogue_marks", j.dump());
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

static std::string userDataDirectory() {
    namespace fs = std::filesystem;
    fs::path base;
#if defined(_WIN32)
    if (const char* p = std::getenv("LOCALAPPDATA")) base = p;
    else if (const char* p = std::getenv("APPDATA")) base = p;
#elif defined(__APPLE__)
    if (const char* p = std::getenv("HOME"))
        base = fs::path(p) / "Library" / "Application Support";
#else
    if (const char* p = std::getenv("XDG_DATA_HOME")) base = p;
    else if (const char* p = std::getenv("HOME"))
        base = fs::path(p) / ".local" / "share";
#endif
    if (base.empty()) base = GetApplicationDirectory();
    fs::path dir = base / "RandomRogue";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string() + "/";
}

static std::string profilePath() {
    return userDataDirectory() + "profile.json";
}

static std::string readTextFile(const std::string& path) {
    char* raw = LoadFileText(path.c_str());
    if (!raw) return "";
    std::string text(raw);
    UnloadFileText(raw);
    return text;
}

static std::string loadRawStore(const char* key) {
    std::string text = readTextFile(userDataDirectory() + key + ".json");
    if (!text.empty()) return text;
    // One-time compatibility with portable builds that wrote beside the exe.
    return readTextFile(std::string(GetApplicationDirectory()) + key + ".json");
}

static void saveRawStore(const char* key, const std::string& text) {
    std::string path = userDataDirectory() + key + ".json";
    SaveFileText(path.c_str(), (char*)text.c_str());
}

Profile LoadProfile() {
    std::string text = readTextFile(profilePath());
    if (text.empty())
        text = readTextFile(std::string(GetApplicationDirectory()) + "profile.json");
    return text.empty() ? Profile{} : Profile::fromJson(text);
}

void SaveProfile(const Profile& p) {
    std::string text = p.toJson();
    SaveFileText(profilePath().c_str(), (char*)text.c_str());
}

#endif
