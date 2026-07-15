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
              {"booksRead", booksRead}};
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
    return p;
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
