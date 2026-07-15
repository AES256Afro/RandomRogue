#include "game.h"
#include <cctype>
#include <cmath>
#include <ctime>
#include <sstream>
#include <nlohmann/json.hpp>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
// Leaderboard + share: relative fetches so they work on random-rogue.com
// and fail silently anywhere else.
EM_JS(void, rr_submit_score, (const char* json), {
    try { fetch('/__score', { method: 'POST', headers: { 'content-type': 'application/json' }, body: UTF8ToString(json) }).catch(function(){}); } catch (e) {}
});
EM_JS(void, rr_fetch_scores, (int day), {
    try { fetch('/__scores?day=' + day).then(function(r){ return r.text(); }).then(function(t){ window.__rrScores = t; }).catch(function(){ window.__rrScores = '[]'; }); } catch (e) { window.__rrScores = '[]'; }
});
EM_JS(char*, rr_get_scores, (), {
    var s = window.__rrScores || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(void, rr_copy_text, (const char* text), {
    try { navigator.clipboard.writeText(UTF8ToString(text)); } catch (e) {}
});
EM_JS(void, rr_fetch_ghosts, (int day, const char* notName), {
    try { fetch('/__ghosts?day=' + day + '&n=3&not=' + encodeURIComponent(UTF8ToString(notName))).then(function(r){ return r.text(); }).then(function(t){ window.__rrGhosts = t; }).catch(function(){ window.__rrGhosts = "[]"; }); } catch (e) { window.__rrGhosts = "[]"; }
});
EM_JS(char*, rr_get_ghosts, (), {
    var s = window.__rrGhosts || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(void, rr_fetch_replay, (int id), {
    window.__rrReplay = "";
    try { fetch('/__replay?id=' + id).then(function(r){ return r.text(); }).then(function(t){ window.__rrReplay = t; }).catch(function(){ window.__rrReplay = "[]"; }); } catch (e) { window.__rrReplay = "[]"; }
});
EM_JS(char*, rr_get_replay, (), {
    var s = window.__rrReplay || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(void, rr_submit_deed, (const char* json), {
    try { fetch('/__deed', { method: 'POST', headers: { 'content-type': 'application/json' }, body: UTF8ToString(json) }).catch(function(){}); } catch (e) {}
});
EM_JS(void, rr_fetch_deeds, (int day), {
    try { fetch('/__deeds?day=' + day).then(function(r){ return r.text(); }).then(function(t){ window.__rrDeeds = t; }).catch(function(){ window.__rrDeeds = "[]"; }); } catch (e) { window.__rrDeeds = "[]"; }
});
EM_JS(char*, rr_get_deeds, (), {
    var s = window.__rrDeeds || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
#endif

namespace {

constexpr int kW = 320, kH = 180;

const Color PAL_BG    = {24, 20, 37, 255};
const Color PAL_INK   = {222, 222, 213, 255};
const Color PAL_GOLD  = {255, 205, 117, 255};
const Color PAL_DIM   = {139, 155, 180, 255};
const Color PAL_DARK  = {90, 105, 136, 255};
const Color PAL_RED   = {231, 82, 86, 255};
const Color PAL_GREEN = {126, 196, 93, 255};
const Color PAL_ROW   = {38, 33, 55, 255};

// Wrap by words; honors '\n'. Never draws past maxY: the final visible line
// gets a ".." marker instead of spilling into buttons or off the canvas.
int DrawTextWrapped(const std::string& text, int x, int y, int width, Color color,
                    int maxY = kH) {
    const int fs = 10, lineH = 11;
    std::string line;
    bool clipped = false;
    auto flush = [&]() {
        if (clipped) return;
        if (y + lineH * 2 > maxY) {
            // Last line that fits: mark the cut.
            if (!line.empty()) DrawText((line + " ..").c_str(), x, y, fs, color);
            y += lineH;
            clipped = true;
            line.clear();
            return;
        }
        if (!line.empty()) DrawText(line.c_str(), x, y, fs, color);
        y += lineH;
        line.clear();
    };
    std::string word;
    for (size_t i = 0; i <= text.size() && !clipped; i++) {
        char c = (i < text.size()) ? text[i] : ' ';
        if (c == ' ' || c == '\n') {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (MeasureText(candidate.c_str(), fs) <= width) {
                line = candidate;
            } else {
                flush();
                line = word;
            }
            word.clear();
            if (c == '\n') flush();
        } else {
            word += c;
        }
    }
    if (!line.empty() && !clipped) {
        if (y + lineH <= maxY) DrawText(line.c_str(), x, y, fs, color);
        y += lineH;
    }
    return y;
}

} // namespace

bool Game::init() {
    auto loadText = [&](const std::string& path, auto&& fn) {
        char* text = LoadFileText(path.c_str());
        if (text) { fn(text); UnloadFileText(text); }
    };
    loadText("assets/data/recipes/prose.json", [&](const char* t) { grammar_.loadJsonText(t); });
    loadText("assets/data/recipes/language.json", [&](const char* t) { forge_.loadJsonText(t); });
    loadText("assets/data/items.json", [&](const char* t) { items_.loadItemsJsonText(t); });
    loadText("assets/data/quirks.json", [&](const char* t) { items_.loadQuirksJsonText(t); });
    items_.generateFamilies();
    // Events live in per-deck files listed by a manifest.
    loadText("assets/data/events/manifest.json", [&](const char* t) {
        nlohmann::json m = nlohmann::json::parse(t, nullptr, false);
        if (m.is_discarded() || !m.contains("files")) return;
        for (auto& f : m["files"])
            if (f.is_string())
                loadText("assets/data/events/" + f.get<std::string>(),
                         [&](const char* et) { deck_.loadJsonText(et); });
    });
    loadText("assets/data/companions.json", [&](const char* t) {
        nlohmann::json j = nlohmann::json::parse(t, nullptr, false);
        if (j.is_discarded() || !j.is_array()) return;
        for (auto& c : j) {
            Companion k;
            k.id = c.value("id", "");
            k.kind = c.value("kind", "");
            k.trait = c.value("trait", "");
            k.passive = c.value("passive", "");
            k.packBonus = c.value("packBonus", 0);
            if (!k.id.empty()) compKinds_.push_back(k);
        }
    });
    loadText("assets/data/traits.json", [&](const char* t) {
        nlohmann::json j = nlohmann::json::parse(t, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return;
        for (auto& [id, name] : j.items())
            if (name.is_string()) traitNames_[id] = name.get<std::string>();
    });
    if (deck_.size() == 0) dataError_ = "content missing: assets/data/events/";
    if (items_.size() == 0) dataError_ = "content missing: assets/data/items.json";
    nextSeed_ = (uint64_t)time(nullptr) % 1000000000ULL;
    audio_.init();
    profile_ = LoadProfile();
    return dataError_.empty();
}

void Game::shutdown() {
    audio_.shutdown();
}

std::vector<Game::StartClass> Game::startClasses() const {
    std::vector<StartClass> classes = {
        {"Drifter", "rations, a weapon, no expectations", "", true},
        {"Scavenger", "satchel, rope, lockpick, an eye for trash", "die 5 times",
         profile_.deaths >= 5},
        {"Duelist", "a rapier, an attitude, +1 DEX", "survive 7 days in one run",
         profile_.bestDays >= 7},
        {"Scholar", "a book, a monocle, +1 INT", "read 3 books",
         profile_.booksRead >= 3},
        {"Tourist", "fancy hat, spyglass, spending money", "live 25 days total",
         profile_.daysTotal >= 25},
    };
    if (!pendingLegacy_.empty()) {
        const LegacyRecord& last = pendingLegacy_.back();
        std::string blurb = "heir of " + last.name;
        blurb += last.relics.empty() ? "; a name to live down"
                                     : "; their " + last.relics[0].first;
        classes.push_back({"Heir", blurb, "", true});
    }
    return classes;
}

// ---- company & purpose (P5) -------------------------------------------------

struct AmbDef { const char* name; const char* desc; };
static const AmbDef kAmbitions[6] = {
    {"Strike It Rich", "hold 120 gold at once"},
    {"Delver", "reach 3 dungeon finales"},
    {"Bookworm", "read 3 things in one life"},
    {"Survivor", "see day 15"},
    {"Relic Hunter", "carry a true artifact"},
    {"Beloved", "be loved somewhere (+20 rep)"},
};

void Game::setCompanion(const std::string& id) {
    for (auto& k : compKinds_) {
        if (k.id != id) continue;
        comp_ = k;
        comp_.name = forge_.person(runRng_).conlang;
        comp_.active = true;
        ch_.companionPassive = comp_.passive;
        ch_.packMax = 9 + comp_.packBonus;
        return;
    }
}

void Game::dismissCompanion(bool died) {
    if (!comp_.active) return;
    if (died) {
        // Companions enter the Chronicle like anyone else who mattered.
        ChronEntry e;
        e.id = (int)history_.chron.size();
        e.year = history_.presentYear;
        e.type = "companion_died";
        e.extra = comp_.name + ", " + comp_.kind;
        history_.chron.push_back(e);
        newsLine_ = comp_.name + " will not be forgotten. Or avenged, probably.";
    }
    comp_ = Companion{};
    ch_.companionPassive.clear();
    ch_.packMax = 9;
}

void Game::offerContract() {
    if (contract_.active || history_.factions.empty()) return;
    contract_ = Contract{};
    contract_.faction = runRng_.range(0, (int)history_.factions.size() - 1);
    const std::string& patron = history_.factions[contract_.faction].name;
    std::vector<int> resting;
    for (int i = 0; i < (int)history_.artifacts.size(); i++)
        if (!history_.artifacts[i].claimed && history_.artifacts[i].restingSite >= 0)
            resting.push_back(i);
    if (!resting.empty() && runRng_.chance(60)) {
        int ai = resting[runRng_.range(0, (int)resting.size() - 1)];
        contract_.artifactId = ai;
        contract_.reward = 22;
        contract_.desc = "Recover " + history_.artifacts[ai].display() + " (rests at " +
                         world_.sites[history_.artifacts[ai].restingSite].name +
                         ") for " + patron;
    } else {
        int s = runRng_.range(0, (int)world_.sites.size() - 1);
        contract_.siteId = s;
        contract_.reward = 12;
        contract_.desc = "Survey " + world_.sites[s].name + " for " + patron;
    }
    contract_.active = true;
}

void Game::checkPurposes() {
    if (ch_.dead) return;
    // Contracts pay out the moment their condition holds.
    if (contract_.active) {
        bool done = false;
        if (contract_.artifactId >= 0)
            for (auto& item : ch_.pack)
                if (item.artifactId == contract_.artifactId) done = true;
        if (contract_.siteId >= 0 && currentSite_ == contract_.siteId) done = true;
        if (done) {
            ch_.money += contract_.reward;
            contractsDone_++;
            if (contract_.faction >= 0 && contract_.faction < (int)rep_.size()) {
                rep_[contract_.faction] += 6;
                if (rep_[contract_.faction] > 50) rep_[contract_.faction] = 50;
            }
            audio_.coin();
            Screen back = screen_;
            showInfo("CONTRACT COMPLETE\n\n" + contract_.desc +
                     ".\n\nA courier finds you within the hour. " +
                     std::to_string(contract_.reward) +
                     " gold, and your name said warmly in rooms you've never entered.");
            infoBack_ = back;
            contract_ = Contract{};
        }
    }
    if (ambition_.id >= 0 && !ambition_.done) {
        bool done = false;
        switch (ambition_.id) {
            case 0: done = ch_.money >= 120; break;
            case 1: done = finalesSeen_ >= 3; break;
            case 2: done = booksThisRun_ >= 3; break;
            case 3: done = ch_.day >= 15; break;
            case 4:
                for (auto& item : ch_.pack)
                    if (item.artifactId >= 0) done = true;
                break;
            case 5:
                for (int r : rep_)
                    if (r >= 20) done = true;
                break;
        }
        if (done) {
            ambition_.done = true;
            ch_.maxHp += 3;
            ch_.hp += 3;
            ch_.money += 10;
            profile_.ambitionsDone++;
            SaveProfile(profile_);
            audio_.chime();
            Screen back = screen_;
            showInfo("AMBITION FULFILLED: " + ambition_.name +
                     "\n\nYou set out to " + ambition_.desc +
                     ", and you did. Whatever else this life becomes, it had a point. "
                     "(+3 max HP, +10 gold, one long satisfied exhale)");
            infoBack_ = back;
        }
    }
}

ItemInstance Game::makeItem(const std::string& id) {
    ItemInstance item = items_.make(id, runRng_);
    bindQuirk(item);
    return item;
}

// ---- mid-run save/load (R3) -------------------------------------------------
// The world re-derives from seed + legacy; only the life itself is stored.

void Game::saveRun() {
    nlohmann::json pack = nlohmann::json::array();
    for (auto& i : ch_.pack)
        pack.push_back({{"tid", i.templateId}, {"name", i.name}, {"type", i.type},
                        {"value", i.value}, {"passive", i.passive}, {"quirk", i.quirk},
                        {"qp", i.quirkPassive}, {"prov", i.provenance},
                        {"aid", i.artifactId}, {"use", i.useEffects}});
    nlohmann::json wars = nlohmann::json::array();
    for (auto& w : history_.liveWars)
        wars.push_back({{"a", w.a}, {"b", w.b}, {"d", w.daysLeft}});
    nlohmann::json j = {
        {"seed", masterSeed_}, {"gen", pendingLegacy_.size()},
        {"name", ch_.name.conlang}, {"meaning", ch_.name.meaning},
        {"stats", std::vector<int>(ch_.stats, ch_.stats + STAT_COUNT)},
        {"hp", ch_.hp}, {"maxhp", ch_.maxHp}, {"money", ch_.money},
        {"credits", ch_.credits}, {"day", ch_.day}, {"ev", ch_.eventsSurvived},
        {"packMax", ch_.packMax}, {"compPassive", ch_.companionPassive},
        {"traits", std::vector<std::string>(ch_.traits.begin(), ch_.traits.end())},
        {"pack", pack}, {"region", currentRegion_}, {"rep", rep_},
        {"visited", std::vector<int>(visitedRegions_.begin(), visitedRegions_.end())},
        {"runRng", {runRng_.state, runRng_.inc}},
        {"liveRng", {liveRng_.state, liveRng_.inc}},
        {"ambId", ambition_.id}, {"ambDone", ambition_.done},
        {"contract", {{"active", contract_.active}, {"desc", contract_.desc},
                      {"aid", contract_.artifactId}, {"sid", contract_.siteId},
                      {"fac", contract_.faction}, {"rw", contract_.reward}}},
        {"comp", {{"id", comp_.id}, {"name", comp_.name}, {"kind", comp_.kind},
                  {"trait", comp_.trait}, {"passive", comp_.passive},
                  {"pb", comp_.packBonus}, {"active", comp_.active}}},
        {"finales", finalesSeen_}, {"books", booksThisRun_},
        {"cdone", contractsDone_}, {"wars", wars},
        {"plague", history_.plaguedRegions},
        {"rival", {{"name", rival_.name}, {"meaning", rival_.meaning},
                   {"alive", rival_.alive}, {"deeds", rival_.deeds}}},
        {"ghosts", ghostsRaw_}};
    nlohmann::json fav = nlohmann::json::object();
    for (auto& [gi, f] : favor_) fav[std::to_string(gi)] = f;
    j["favor"] = fav;
    j["miracle"] = miracleUsed_;
    nlohmann::json steps = nlohmann::json::array();
    for (auto& st : journey_)
        steps.push_back({{"d", st.day}, {"s", st.site}, {"c", st.choice}, {"o", st.outcome}});
    j["journey"] = steps;
    SaveRawRun(j.dump());
}

bool Game::loadRun() {
    nlohmann::json j = nlohmann::json::parse(LoadRawRun(), nullptr, false);
    if (j.is_discarded() || !j.contains("seed")) return false;
    nextSeed_ = j["seed"].get<uint64_t>();
    // Rebuild the world exactly as the save knew it.
    pendingLegacy_ = LoadLegacy(nextSeed_);
    size_t gen = j.value("gen", (size_t)0);
    if (pendingLegacy_.size() > gen) pendingLegacy_.resize(gen);
    cachedLegacySeed_ = nextSeed_;
    ambition_ = Ambition{};
    ambition_.id = j.value("ambId", -1);
    ambition_.done = j.value("ambDone", false);
    if (ambition_.id >= 0) {
        ambition_.name = kAmbitions[ambition_.id].name;
        ambition_.desc = kAmbitions[ambition_.id].desc;
    }
    suppressStrangers_ = true; // the save knows its own ghosts
    newRun(0);
    suppressStrangers_ = false;
    std::string gj = j.value("ghosts", std::string());
    if (!gj.empty()) {
        injectStrangers(gj);
        ghostsRaw_ = gj;
    }
    if (j.contains("rival")) {
        auto& r = j["rival"];
        rival_.name = r.value("name", rival_.name);
        rival_.meaning = r.value("meaning", rival_.meaning);
        rival_.alive = r.value("alive", true);
        rival_.deeds = r.value("deeds", 0);
    }
    favor_.clear();
    if (j.contains("favor") && j["favor"].is_object())
        for (auto& [gi, f] : j["favor"].items())
            favor_[atoi(gi.c_str())] = f.get<int>();
    miracleUsed_ = j.value("miracle", false);
    journey_.clear();
    if (j.contains("journey"))
        for (auto& s : j["journey"]) {
            JourneyStep st;
            st.day = s.value("d", 0);
            st.site = s.value("s", "");
            st.choice = s.value("c", "");
            st.outcome = s.value("o", "");
            journey_.push_back(st);
        }
    // Now overlay the saved life onto the fresh world.
    ch_.name.conlang = j.value("name", ch_.name.conlang);
    ch_.name.meaning = j.value("meaning", ch_.name.meaning);
    auto stats = j.value("stats", std::vector<int>{});
    for (int i = 0; i < STAT_COUNT && i < (int)stats.size(); i++) ch_.stats[i] = stats[i];
    ch_.hp = j.value("hp", 10);
    ch_.maxHp = j.value("maxhp", 10);
    ch_.money = j.value("money", 0);
    ch_.credits = j.value("credits", 0);
    ch_.day = j.value("day", 1);
    ch_.eventsSurvived = j.value("ev", 0);
    ch_.packMax = j.value("packMax", 9);
    ch_.companionPassive = j.value("compPassive", "");
    ch_.traits.clear();
    for (auto& t : j.value("traits", std::vector<std::string>{})) ch_.traits.insert(t);
    ch_.pack.clear();
    if (j.contains("pack"))
        for (auto& p : j["pack"]) {
            ItemInstance i;
            i.templateId = p.value("tid", "");
            i.name = p.value("name", "");
            i.type = p.value("type", "misc");
            i.value = p.value("value", 0);
            i.passive = p.value("passive", "");
            i.quirk = p.value("quirk", "");
            i.quirkPassive = p.value("qp", "");
            i.provenance = p.value("prov", "");
            i.artifactId = p.value("aid", -1);
            i.useEffects = p.value("use", std::vector<std::string>{});
            if (i.artifactId >= 0 && i.artifactId < (int)history_.artifacts.size())
                history_.artifacts[i.artifactId].claimed = true;
            ch_.pack.push_back(i);
        }
    currentRegion_ = j.value("region", currentRegion_);
    rep_ = j.value("rep", rep_);
    visitedRegions_.clear();
    for (int r : j.value("visited", std::vector<int>{})) visitedRegions_.insert(r);
    if (j.contains("runRng")) { runRng_.state = j["runRng"][0]; runRng_.inc = j["runRng"][1]; }
    if (j.contains("liveRng")) { liveRng_.state = j["liveRng"][0]; liveRng_.inc = j["liveRng"][1]; }
    if (j.contains("contract")) {
        auto& c = j["contract"];
        contract_.active = c.value("active", false);
        contract_.desc = c.value("desc", "");
        contract_.artifactId = c.value("aid", -1);
        contract_.siteId = c.value("sid", -1);
        contract_.faction = c.value("fac", -1);
        contract_.reward = c.value("rw", 0);
    }
    if (j.contains("comp")) {
        auto& c = j["comp"];
        comp_.id = c.value("id", "");
        comp_.name = c.value("name", "");
        comp_.kind = c.value("kind", "");
        comp_.trait = c.value("trait", "");
        comp_.passive = c.value("passive", "");
        comp_.packBonus = c.value("pb", 0);
        comp_.active = c.value("active", false);
    }
    finalesSeen_ = j.value("finales", 0);
    booksThisRun_ = j.value("books", 0);
    contractsDone_ = j.value("cdone", 0);
    history_.liveWars.clear();
    if (j.contains("wars"))
        for (auto& w : j["wars"])
            history_.liveWars.push_back({w.value("a", 0), w.value("b", 0),
                                         w.value("d", 5), -1});
    if (j.contains("plague"))
        history_.plaguedRegions = j["plague"].get<std::map<int, int>>();
    enterTravel();
    return true;
}

void Game::clearRun() { SaveRawRun(""); }

// ---- procedural pixel portrait (R3) -----------------------------------------
// A face from a name: same name, same face, forever. No assets were harmed.
void Game::drawPortrait(int x, int y, int scale, const std::string& name) {
    uint32_t h = 2166136261u;
    for (char c : name) h = (h ^ (uint8_t)c) * 16777619u;
    auto bit = [&](int n) { return (h >> (n % 31)) & 1; };
    static const Color kSkin[4] = {{224, 172, 128, 255}, {186, 128, 88, 255},
                                   {142, 96, 66, 255}, {158, 175, 136, 255}};
    static const Color kHair[6] = {{60, 42, 32, 255},  {188, 152, 76, 255},
                                   {40, 40, 48, 255},  {150, 60, 54, 255},
                                   {200, 200, 208, 255}, {88, 110, 78, 255}};
    Color skin = kSkin[h % 4];
    Color hair = kHair[(h >> 4) % 6];
    auto px = [&](int cx, int cy, Color col) {
        DrawRectangle(x + cx * scale, y + cy * scale, scale, scale, col);
    };
    for (int cy = 2; cy < 8; cy++) // head
        for (int cx = 1; cx < 7; cx++) px(cx, cy, skin);
    int hairLen = 2 + (int)bit(3) + (int)bit(11); // hairline
    for (int cy = 1; cy < hairLen; cy++)
        for (int cx = 1; cx < 7; cx++) px(cx, cy, hair);
    if (bit(7)) { px(0, 3, hair); px(7, 3, hair); } // side hair
    Color eye = bit(9) ? Color{40, 48, 80, 255} : Color{40, 64, 40, 255};
    px(2, 4 + (int)bit(5), eye);
    px(5, 4 + (int)bit(5), eye);
    if (bit(13)) { px(2, 6, hair); px(3, 6, hair); px(4, 6, hair); px(5, 6, hair); } // beard
    else px(3 + (int)bit(2), 6, Color{120, 70, 60, 255}); // mouth
}

// Every quirk is tied to something: either a hidden mechanical modifier, or a
// real entity from this world's Chronicle. The player is never told which.
void Game::bindQuirk(ItemInstance& item) {
    if (item.artifactId >= 0 || !item.quirk.empty()) return;
    if (!runRng_.chance(50)) return; // plenty of items are just honest items

    bool mechanical = runRng_.chance(45);
    if (mechanical && !items_.quirkTexts().empty()) {
        item.quirk = runRng_.pick(items_.quirkTexts());
        item.quirkPassive = runRng_.chance(60) && !items_.goodPassives().empty()
                                ? runRng_.pick(items_.goodPassives())
                                : (!items_.badPassives().empty()
                                       ? runRng_.pick(items_.badPassives())
                                       : "");
        return;
    }

    // Historical quirk: bind to real people, places, and disasters.
    Grammar::Ctx ctx = {{"qfig", "someone forgotten"},
                        {"qfaction", "a guild long dissolved"},
                        {"qbeast", "something large"},
                        {"qplague", "a bad season"},
                        {"qsite", "somewhere lost"},
                        {"qcity", "somewhere lost"},
                        {"qbook", "an unreadable book"},
                        {"qyear", std::to_string(runRng_.range(1, history_.years))}};
    std::vector<int> dead;
    for (int i = 0; i < (int)history_.figures.size(); i++)
        if (history_.figures[i].died >= 0) dead.push_back(i);
    if (!dead.empty())
        ctx["qfig"] = history_.figures[runRng_.pick(dead)].name;
    if (!history_.factions.empty())
        ctx["qfaction"] = history_.factions[runRng_.range(0, (int)history_.factions.size() - 1)].name;
    if (!history_.beasts.empty())
        ctx["qbeast"] = history_.beasts[runRng_.range(0, (int)history_.beasts.size() - 1)].name;
    if (!world_.sites.empty())
        ctx["qsite"] = world_.sites[runRng_.range(0, (int)world_.sites.size() - 1)].name;
    std::vector<const ChronEntry*> plagues, books;
    for (auto& e : history_.chron) {
        if (e.type == "plague") plagues.push_back(&e);
        if (e.type == "book_written") books.push_back(&e);
    }
    if (!plagues.empty()) ctx["qplague"] = runRng_.pick(plagues)->extra;
    if (!books.empty()) ctx["qbook"] = runRng_.pick(books)->extra;

    item.quirk = grammar_.expand("{quirk_hist}", runRng_, ctx);
    // A minority of relics of history also carry a little of its weight.
    if (runRng_.chance(30) && !items_.goodPassives().empty())
        item.quirkPassive = runRng_.pick(items_.goodPassives());
}

// The `when` condition language. One condition per string; all must hold.
bool Game::evalCond(const std::string& cond) const {
    std::istringstream ss(cond);
    std::string a, b, c;
    ss >> a >> b >> c;
    if (a == "trait") return ch_.hasTrait(b);
    if (a == "!trait") return !ch_.hasTrait(b);
    if (a == "has") return ch_.hasItem(b);
    if (a == "!has") return !ch_.hasItem(b);
    if (a == "carrying_artifact") {
        for (auto& i : ch_.pack)
            if (i.artifactId >= 0) return true;
        return false;
    }
    if (a == "npc") {
        auto it = npcMarks_.find(slotFigure_);
        return it != npcMarks_.end() && it->second.count(b) > 0;
    }
    auto cmp = [&](int lhs) {
        int rhs = atoi(c.c_str());
        if (b == ">") return lhs > rhs;
        if (b == "<") return lhs < rhs;
        if (b == ">=") return lhs >= rhs;
        if (b == "<=") return lhs <= rhs;
        return lhs == rhs;
    };
    if (a == "companion") return comp_.active;
    if (a == "!companion") return !comp_.active;
    if (a == "vehicle" || a == "!vehicle") {
        bool has = false;
        for (auto& item : ch_.pack)
            if (item.type == "vehicle") has = true;
        return (a[0] == '!') ? !has : has;
    }
    if (a == "ship" || a == "!ship") {
        bool has = false;
        for (auto& item : ch_.pack)
            if (item.type == "ship") has = true;
        return (a[0] == '!') ? !has : has;
    }
    if (a == "war_here") {
        int owner = regionOwner(currentSite_ >= 0 ? world_.sites[currentSite_].region : -1);
        for (auto& w : history_.liveWars)
            if (w.a == owner || w.b == owner) return owner >= 0;
        return false;
    }
    if (a == "plague_here") {
        int region = currentSite_ >= 0 ? world_.sites[currentSite_].region : -1;
        return region >= 0 && history_.plaguedRegions.count(region) > 0;
    }
    if (a == "raining") return weather_ == "raining";
    if (a == "snowing") return weather_ == "snowing";
    if (a == "season") return season_ == b;
    if (a == "contracts") return cmp(contractsDone_);
    if (a == "rep") return cmp(localRep());
    if (a == "money") return cmp(ch_.money);
    if (a == "credits") return cmp(ch_.credits);
    if (a == "hp") return cmp(ch_.hp);
    if (a == "day") return cmp(ch_.day);
    if (a == "stat") {
        std::string d;
        ss >> d; // "stat str >= 14": b=stat name, c=op, d=value
        int lhs = ch_.stats[statFromName(b)];
        int rhs = atoi(d.c_str());
        if (c == ">") return lhs > rhs;
        if (c == "<") return lhs < rhs;
        if (c == ">=") return lhs >= rhs;
        if (c == "<=") return lhs <= rhs;
        return lhs == rhs;
    }
    return false; // unknown condition: never true, validator catches these
}

// Dead characters of this seed become historical figures; the world simulates
// the years between attempts. Deterministic: same seed + same legacy list =
// same world, every time, on every platform.
void Game::injectLegacy(int classIdx) {
    int yearCursor = history_.years;
    for (size_t i = 0; i < pendingLegacy_.size(); i++) {
        const LegacyRecord& rec = pendingLegacy_[i];
        Rng inj(masterSeed_ + (i + 1) * 31337ULL, 88);
        int deathYear = yearCursor + 1;

        Figure f;
        f.name = rec.name;
        f.faction = history_.factions.empty()
                        ? 0 : (int)(i % history_.factions.size());
        f.trait = "legendary";
        f.profession = "adventurer";
        f.born = deathYear - inj.range(19, 40);
        f.died = deathYear;
        history_.figures.push_back(f);
        int fi = (int)history_.figures.size() - 1;

        ChronEntry death;
        death.id = (int)history_.chron.size();
        death.year = deathYear;
        death.type = "pc_died";
        death.actor = fi;
        death.extra = rec.epitaph;
        for (int s = 0; s < (int)world_.sites.size(); s++)
            if (world_.sites[s].name == rec.deathSite) death.site = s;
        history_.chron.push_back(death);

        // Their things scatter into the world, findable, name attached.
        for (size_t r = 0; r < rec.relics.size(); r++) {
            // The heir carries the newest ghost's first relic instead.
            bool heirTakesIt = (classIdx == 5) && (i + 1 == pendingLegacy_.size()) && r == 0;
            if (heirTakesIt) continue;
            HArtifact a;
            a.conlang = rec.relics[r].first;
            a.meaning = "";
            a.material = "memory and " + std::string(r == 0 ? "stubbornness" : "rust");
            a.forgedBy = fi;
            a.forgedYear = deathYear;
            a.restingSite = inj.range(0, (int)world_.sites.size() - 1);
            history_.artifacts.push_back(a);
            ChronEntry relic;
            relic.id = (int)history_.chron.size();
            relic.year = deathYear;
            relic.type = "pc_relic";
            relic.actor = fi;
            relic.artifact = (int)history_.artifacts.size() - 1;
            relic.site = a.restingSite;
            history_.chron.push_back(relic);
            history_.artifacts.back().deeds.push_back(relic.id);
        }

        int gap = 2 + (int)(i % 4);
        SimulateYears(world_, history_, masterSeed_ + (i + 1) * 7919ULL,
                      yearCursor + 1, yearCursor + gap, grammar_, forge_);
        yearCursor += gap;
    }
    history_.presentYear = yearCursor;
    history_.liveStartId = (int)history_.chron.size();
}

// Other players' fallen, walked into this daily world as fresh graves (R4).
// No gap years are simulated: everyone's daily world must stay the same age
// no matter how many ghosts their browser happened to fetch.
void Game::injectStrangers(const std::string& json) {
    strangers_.clear();
    nlohmann::json list = nlohmann::json::parse(json, nullptr, false);
    if (list.is_discarded() || !list.is_array()) return;
    int n = 0;
    for (auto& g : list) {
        if (n >= 3) break;
        Stranger s;
        s.name = g.value("name", "");
        s.epitaph = g.value("epitaph", "");
        s.days = g.value("days", 0);
        if (s.name.empty()) continue;
        Rng inj(masterSeed_ + 977ULL * (n + 1), 99);
        Figure f;
        f.name = s.name;
        f.faction = history_.factions.empty() ? 0 : inj.range(0, (int)history_.factions.size() - 1);
        f.trait = "ill-fated";
        f.profession = "wanderer";
        f.born = history_.presentYear - inj.range(19, 40);
        f.died = history_.presentYear; // they fell in this very world, today
        history_.figures.push_back(f);
        s.figure = (int)history_.figures.size() - 1;

        std::string deathSite = g.value("site", "");
        s.site = inj.range(0, (int)world_.sites.size() - 1);
        for (int i = 0; i < (int)world_.sites.size(); i++)
            if (world_.sites[i].name == deathSite) s.site = i;

        ChronEntry death;
        death.id = (int)history_.chron.size();
        death.year = history_.presentYear;
        death.type = "pc_died";
        death.actor = s.figure;
        death.site = s.site;
        death.extra = s.epitaph;
        history_.chron.push_back(death);

        // Their relics rest where they fell, name attached, quirk intact.
        nlohmann::json relics = nlohmann::json::parse(
            g.value("relics", std::string("[]")), nullptr, false);
        if (!relics.is_discarded() && relics.is_array()) {
            int rn = 0;
            for (auto& r : relics) {
                if (rn++ >= 2) break;
                HArtifact a;
                a.conlang = r.value("name", "");
                if (a.conlang.empty()) continue;
                a.meaning = "";
                a.material = "someone else's story";
                a.forgedBy = s.figure;
                a.forgedYear = history_.presentYear;
                a.restingSite = s.site;
                history_.artifacts.push_back(a);
            }
        }
        strangers_.push_back(s);
        n++;
    }
}

void Game::newRun(int classIdx) {
    masterSeed_ = nextSeed_;
    runCounter_++;
    world_ = GenerateWorld(masterSeed_, grammar_, forge_);
    history_ = SimulateHistory(world_, masterSeed_, grammar_, forge_);
    if (cachedLegacySeed_ != masterSeed_) {
        pendingLegacy_ = LoadLegacy(masterSeed_);
        cachedLegacySeed_ = masterSeed_;
    }
    // Each generation is its own person living its own days — keyed by how
    // many came before, so a shared seed still means the same first life for
    // everyone, but your second life never replays your first.
    Rng langRng(masterSeed_ + pendingLegacy_.size() * 131071ULL, STREAM_LANG);
    runRng_ = Rng(masterSeed_ + pendingLegacy_.size() * 524287ULL, STREAM_RUNTIME);
    liveRng_ = Rng(masterSeed_ ^ ((pendingLegacy_.size() + 1) * 1013904223ULL), 77);
    injectLegacy(classIdx);
    rep_.assign(history_.factions.size(), 0);
    season_ = "spring";
    weather_ = "clear";
    newsLine_.clear();
    legacySaved_ = false;
    ch_ = Character::roll(langRng, forge_);
    switch (classIdx) {
        case 1: // Scavenger
            ch_.pack.push_back(makeItem("rations"));
            ch_.pack.push_back(makeItem("satchel"));
            ch_.pack.push_back(makeItem("rope"));
            ch_.pack.push_back(makeItem("lockpick"));
            break;
        case 2: // Duelist
            ch_.pack.push_back(makeItem("rapier"));
            ch_.pack.push_back(makeItem("ale"));
            ch_.stats[STAT_DEX] += 1;
            break;
        case 3: // Scholar
            ch_.pack.push_back(makeItem("book"));
            ch_.pack.push_back(makeItem("monocle"));
            ch_.stats[STAT_INT] += 1;
            break;
        case 4: // Tourist
            ch_.pack.push_back(makeItem("fancy_hat"));
            ch_.pack.push_back(makeItem("spyglass"));
            ch_.money += 15;
            ch_.credits += 5;
            break;
        case 5: { // Heir: the newest ghost's heirloom, quirk intact
            ch_.pack.push_back(makeItem("bread"));
            if (!pendingLegacy_.empty() && !pendingLegacy_.back().relics.empty()) {
                const auto& [name, quirk] = pendingLegacy_.back().relics[0];
                ItemInstance heirloom;
                heirloom.templateId = "heirloom";
                heirloom.name = name;
                heirloom.type = "weapon";
                heirloom.value = 18;
                heirloom.passive = "check str +1";
                heirloom.quirk = quirk.empty()
                                     ? "still smells faintly of " +
                                           pendingLegacy_.back().name : quirk;
                heirloom.provenance = "Carried by " + pendingLegacy_.back().name +
                                      " until the end. " + pendingLegacy_.back().epitaph;
                ch_.pack.push_back(heirloom);
            }
            ch_.stats[STAT_STR] += 1;
            ch_.stats[STAT_CHA] += 1;
            break;
        }
        default: // Drifter
            ch_.pack.push_back(makeItem("rations"));
            ch_.pack.push_back(makeItem(runRng_.chance(50) ? "rusty_sword" : "club"));
            break;
    }
    deck_.resetUsed();
    pendingArtifact_ = -1;
    pendingShop_ = false;
    forcedNextId_.clear();
    npcMarks_.clear();
    slotFigure_ = -1;
    blessingSpent_ = false;
    comp_ = Companion{};
    contract_ = Contract{};
    finalesSeen_ = 0;
    booksThisRun_ = 0;
    compLine_.clear();
    finishedWell_ = false;
    afterlifeShown_ = false;
    heirBlessing_ = false;
    contractsDone_ = 0;
    visitedRegions_.clear();
    scoreSubmitted_ = false;
    slotBeast_ = -1;
    slotGod_ = -1;
    favor_.clear();
    miracleUsed_ = false;
    journey_.clear();
    deeds_.clear();
    deedNext_ = 0;
    // NPC memory outlives you in this world (R3).
    npcMarks_.clear();
    {
        nlohmann::json m = nlohmann::json::parse(LoadMarks(masterSeed_), nullptr, false);
        if (!m.is_discarded() && m.is_object())
            for (auto& [fig, tags] : m.items())
                for (auto& t : tags)
                    npcMarks_[atoi(fig.c_str())].insert(t.get<std::string>());
    }
    if (classIdx == 5 && !pendingLegacy_.empty() && pendingLegacy_.back().blessing)
        ch_.traits.insert("blessed"); // the afterlife bargain, honored
    // The other wanderer: every world generates one more adventurer than you.
    GenName rn = forge_.person(runRng_);
    rival_ = Rival{};
    rival_.name = rn.conlang;
    rival_.meaning = rn.meaning;
    // The shared graveyard: daily worlds carry other players' fresh dead.
    strangers_.clear();
    ghostsRaw_.clear();
#if defined(__EMSCRIPTEN__)
    if (!suppressStrangers_) {
        uint64_t dailySeed = (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
        if (masterSeed_ == dailySeed) {
            char* raw = rr_get_ghosts();
            std::string gj(raw ? raw : "");
            free(raw);
            if (!gj.empty()) {
                injectStrangers(gj);
                if (!strangers_.empty()) ghostsRaw_ = gj;
            }
            // And the day's deeds so far: other players, live-ish (R5).
            char* draw = rr_get_deeds();
            std::string dj(draw ? draw : "");
            free(draw);
            nlohmann::json dl = nlohmann::json::parse(dj, nullptr, false);
            if (!dl.is_discarded() && dl.is_array())
                for (auto& d : dl) {
                    std::string t = d.value("text", "");
                    if (!t.empty()) deeds_.push_back(t.substr(0, 140));
                }
        }
    }
#endif
    currentRegion_ = runRng_.range(0, (int)world_.regions.size() - 1);
    visitedRegions_.insert(currentRegion_);
    enterTravel();
}

std::vector<int> Game::regionDistances() const {
    std::vector<int> dist(world_.regions.size(), -1);
    if (currentRegion_ < 0 || currentRegion_ >= (int)world_.regions.size()) return dist;
    std::vector<int> queue = {currentRegion_};
    dist[currentRegion_] = 0;
    for (size_t qi = 0; qi < queue.size(); qi++) {
        int r = queue[qi];
        for (int n : world_.regions[r].neighbors) {
            if (n >= 0 && n < (int)dist.size() && dist[n] < 0) {
                dist[n] = dist[r] + 1;
                queue.push_back(n);
            }
        }
    }
    return dist;
}

// The world is wide now: nearby sites are a day away, the horizon costs
// days and supplies. A vehicle halves the road.
void Game::enterTravel() {
    screen_ = TRAVEL;
    if (!ch_.dead) saveRun(); // autosave: a closed tab shouldn't kill a life
    travelOptions_.clear();
    std::vector<int> dist = regionDistances();
    bool vehicle = false;
    for (auto& item : ch_.pack)
        if (item.type == "vehicle") vehicle = true;

    auto makeOption = [&](int si) {
        const Site& s = world_.sites[si];
        TravelOption o;
        o.deck = s.deck;
        o.siteName = s.name;
        o.site = si;
        int d = (s.region >= 0 && s.region < (int)dist.size() && dist[s.region] >= 0)
                    ? dist[s.region] : 3;
        o.days = d < 1 ? 1 : d;
        if (vehicle) o.days = (o.days + 1) / 2;
        o.label = s.name + " (" + s.type + ", " + std::to_string(o.days) +
                  (o.days == 1 ? " day)" : " days)");
        return o;
    };

    std::vector<int> near, far;
    for (int i = 0; i < (int)world_.sites.size(); i++) {
        int r = world_.sites[i].region;
        int d = (r >= 0 && r < (int)dist.size()) ? dist[r] : -1;
        if (d >= 0 && d <= 1) near.push_back(i);
        else far.push_back(i);
    }
    for (int n = 0; n < 2 && !near.empty(); n++) {
        int pi = runRng_.range(0, (int)near.size() - 1);
        travelOptions_.push_back(makeOption(near[pi]));
        near.erase(near.begin() + pi);
    }
    if (!far.empty())
        travelOptions_.push_back(makeOption(far[runRng_.range(0, (int)far.size() - 1)]));
    while (travelOptions_.size() < 3 && !near.empty()) {
        int pi = runRng_.range(0, (int)near.size() - 1);
        travelOptions_.push_back(makeOption(near[pi]));
        near.erase(near.begin() + pi);
    }
    TravelOption wander;
    std::string biome = world_.regions[currentRegion_].biome;
    // Wandering draws from the land itself: biomes are decks now (R5).
    if (biome == "forest") wander.deck = "forest";
    else if (biome == "swamp") wander.deck = "swamp";
    else if (biome == "mountains") wander.deck = "mountains";
    else if (biome == "coast") wander.deck = "coast";
    else wander.deck = "road"; // plains, desert: the long flat middle
    wander.siteName = "the wilds";
    wander.label = "Wander " + world_.regions[currentRegion_].name;
    travelOptions_.push_back(wander);

    // The naval arc: on a coast, with a ship, the map opens sideways (R6).
    if (biome == "coast") {
        bool hasShip = false;
        for (auto& item : ch_.pack)
            if (item.type == "ship") hasShip = true;
        if (hasShip) {
            TravelOption sail;
            sail.deck = "sea";
            sail.siteName = "the open water";
            sail.label = "Set sail (2 days, another shore)";
            sail.days = 2;
            sail.sail = true;
            travelOptions_.push_back(sail);
        }
    }
}

int Game::regionOwner(int region) const {
    if (region < 0) return -1;
    for (int i = 0; i < (int)history_.factions.size(); i++)
        if (history_.factions[i].home == region) return i;
    return -1;
}

int Game::localRep() const {
    int region = (currentSite_ >= 0) ? world_.sites[currentSite_].region : -1;
    int owner = regionOwner(region);
    if (owner < 0) return 0;
    return rep_[owner];
}

int Game::buyPrice(const ItemInstance& item) const {
    int rep = localRep();
    int price = item.value * (135 - rep * 2) / 100;
    return price < 1 ? 1 : price;
}

int Game::sellPrice(const ItemInstance& item) const {
    int rep = localRep();
    int price = item.value * (40 + rep) / 100;
    return price < 1 ? 1 : price;
}

std::string Game::randomRumor() {
    if (history_.chron.empty()) return "Nobody is talking. Suspicious, honestly.";
    // Fresh news travels faster than old history.
    int lo = 0;
    int liveCount = (int)history_.chron.size() - history_.liveStartId;
    if (liveCount > 0 && runRng_.chance(40)) lo = history_.liveStartId;
    const ChronEntry& e = history_.chron[runRng_.range(lo, (int)history_.chron.size() - 1)];
    std::string rumor = RenderChronEntry(e, history_, world_, grammar_, runRng_);
    if (runRng_.chance(15))
        rumor += " (At least, that's the version going around.)";
    return rumor;
}

// One day passes: the world does not wait for you.
void Game::dailyTick() {
    ch_.day++;
    size_t before = history_.chron.size();
    SimulateLiveDay(world_, history_, liveRng_, grammar_, forge_);
    if (history_.chron.size() > before) {
        newsLine_ = RenderChronEntry(history_.chron.back(), history_, world_,
                                     grammar_, liveRng_);
        audio_.chime();
    }
    // The rival is out there too, having a run of their own (R4).
    if (rival_.alive && liveRng_.chance(16)) {
        rival_.deeds++;
        int act = liveRng_.range(1, 100);
        if (act <= 20) {
            // They get to the beast first, sometimes. That bounty is gone.
            int bi = -1;
            for (int i = 0; i < (int)history_.beasts.size(); i++)
                if (history_.beasts[i].died < 0) bi = i;
            if (bi >= 0) {
                Beast& b = history_.beasts[bi];
                b.died = history_.presentYear;
                ChronEntry e;
                e.id = (int)history_.chron.size();
                e.year = history_.presentYear;
                e.type = "pc_beast_slain";
                e.beast = bi;
                e.extra = rival_.name;
                history_.chron.push_back(e);
                newsLine_ = rival_.name + " slew " + b.name + ". Every tavern is buying their drinks tonight.";
            } else {
                newsLine_ = rival_.name + " went hunting and found nothing left to hunt.";
            }
        } else if (act <= 45) {
            int si = liveRng_.range(0, (int)world_.sites.size() - 1);
            newsLine_ = rival_.name + " was seen at " + world_.sites[si].name +
                        ", asking the kind of questions you were saving.";
        } else if (act <= 70 && !history_.factions.empty()) {
            int fi = liveRng_.range(0, (int)history_.factions.size() - 1);
            newsLine_ = rival_.name + " closed a contract for " +
                        history_.factions[fi].name + ". The pay was insulting. They took it.";
        } else if (act <= 96 || ch_.day <= 10) {
            newsLine_ = rival_.name + " \"" + rival_.meaning + "\" won a duel over a matter of pronunciation.";
        } else {
            // The world is dangerous for everyone. Even the competition.
            rival_.alive = false;
            Figure f;
            f.name = rival_.name;
            f.faction = 0;
            f.trait = "rash";
            f.profession = "adventurer";
            f.born = history_.presentYear - 25;
            f.died = history_.presentYear;
            history_.figures.push_back(f);
            ChronEntry e;
            e.id = (int)history_.chron.size();
            e.year = history_.presentYear;
            e.type = "pc_died";
            e.actor = (int)history_.figures.size() - 1;
            e.extra = "The road took them mid-boast.";
            history_.chron.push_back(e);
            newsLine_ = rival_.name + " is dead. You feel the strangest grief: who chases you now?";
        }
    }
    // Word arrives from other players in this same daily world (R5).
    if (deedNext_ < deeds_.size() && liveRng_.chance(14)) {
        newsLine_ = "WORD ARRIVES: " + deeds_[deedNext_++];
        audio_.chime();
    }
    static const char* kSeasons[4] = {"spring", "summer", "autumn", "winter"};
    season_ = kSeasons[(ch_.day / 28) % 4];
    int roll = liveRng_.range(1, 100);
    weather_ = "clear";
    if (season_ == "winter") { if (roll <= 35) weather_ = "snowing"; }
    else if (roll <= 25) weather_ = "raining";
}

std::string Game::chronicleExcerpt(int entries) {
    if (history_.chron.empty()) return "The pages are blank. All of them. That can't be good.";
    int start = runRng_.range(0, (int)history_.chron.size() - 1);
    std::string out;
    for (int i = 0; i < entries && start + i < (int)history_.chron.size(); i++) {
        if (i) out += "\n";
        out += RenderChronEntry(history_.chron[start + i], history_, world_, grammar_, runRng_);
    }
    return out;
}

bool Game::resolveSlots(const Event& e, Grammar::Ctx& ctx) {
    for (auto& [name, query] : e.slots) {
        if (query == "chronicle_random") {
            ctx[name] = randomRumor();
        } else if (query == "chronicle_news") {
            // Only satisfiable when something has actually HAPPENED lately.
            int liveCount = (int)history_.chron.size() - history_.liveStartId;
            if (liveCount <= 0) return false;
            const ChronEntry& e = history_.chron[runRng_.range(
                history_.liveStartId, (int)history_.chron.size() - 1)];
            ctx[name] = RenderChronEntry(e, history_, world_, grammar_, runRng_);
        } else if (query == "artifact_here") {
            int found = -1;
            for (int i = 0; i < (int)history_.artifacts.size(); i++) {
                const HArtifact& a = history_.artifacts[i];
                if (!a.claimed && a.restingSite == currentSite_) { found = i; break; }
            }
            if (found < 0) return false;
            pendingArtifact_ = found;
            const HArtifact& a = history_.artifacts[found];
            ctx[name] = a.display();
            ctx[name + "_material"] = a.material;
        } else if (query == "figure_alive") {
            std::vector<int> pool;
            for (int i = 0; i < (int)history_.figures.size(); i++)
                if (history_.figures[i].died < 0) pool.push_back(i);
            if (pool.empty()) return false;
            int fi = pool[runRng_.range(0, (int)pool.size() - 1)];
            slotFigure_ = fi;
            const Figure& f = history_.figures[fi];
            ctx[name] = f.name;
            ctx[name + "_trait"] = f.trait;
            ctx[name + "_prof"] = f.profession;
            if (f.faction >= 0) ctx[name + "_faction"] = history_.factions[f.faction].name;
        } else if (query == "beast_here") {
            // A living named beast lairing in THIS region: huntable.
            int found = -1;
            for (int i = 0; i < (int)history_.beasts.size(); i++)
                if (history_.beasts[i].died < 0 &&
                    history_.beasts[i].region == currentRegion_) found = i;
            if (found < 0) return false;
            slotBeast_ = found;
            const Beast& b = history_.beasts[found];
            ctx[name] = b.name;
            ctx[name + "_kills"] = std::to_string(b.kills);
        } else if (query == "god") {
            if (history_.gods.empty()) return false;
            int gi = runRng_.range(0, (int)history_.gods.size() - 1);
            slotGod_ = gi; // favor effects in this event land on this god
            const God& god = history_.gods[gi];
            ctx[name] = god.name;
            ctx[name + "_domain"] = god.domain;
            ctx[name + "_mood"] = god.mood > 0 ? "generous" : (god.mood < 0 ? "wrathful" : "indifferent");
            ctx[name + "_favor"] = std::to_string(favor_.count(gi) ? favor_[gi] : 0);
        } else if (query == "figure_dead") {
            std::vector<int> pool;
            for (int i = 0; i < (int)history_.figures.size(); i++)
                if (history_.figures[i].died >= 0) pool.push_back(i);
            if (pool.empty()) return false;
            const Figure& f = history_.figures[pool[runRng_.range(0, (int)pool.size() - 1)]];
            ctx[name] = f.name;
            ctx[name + "_trait"] = f.trait;
            ctx[name + "_year"] = std::to_string(f.died);
        } else if (query == "stranger_here") {
            // Another PLAYER died at this site today (daily worlds, R4).
            int found = -1;
            for (int i = 0; i < (int)strangers_.size(); i++)
                if (strangers_[i].site == currentSite_) found = i;
            if (found < 0) return false;
            const Stranger& s = strangers_[found];
            ctx[name] = s.name;
            ctx[name + "_days"] = std::to_string(s.days);
            ctx[name + "_epitaph"] = s.epitaph.empty()
                ? "The stone is blank. Somehow that's worse." : s.epitaph;
        } else if (query == "ghost_here") {
            // One of YOUR previous lives died at this very site.
            int found = -1;
            for (int i = 0; i < (int)pendingLegacy_.size(); i++)
                if (pendingLegacy_[i].deathSite == siteName_) found = i;
            if (found < 0) return false;
            const LegacyRecord& g = pendingLegacy_[found];
            ctx[name] = g.name;
            ctx[name + "_meaning"] = g.meaning;
            ctx[name + "_days"] = std::to_string(g.days);
            ctx[name + "_epitaph"] = g.epitaph;
            ctx[name + "_relic"] = g.relics.empty() ? "nothing" : g.relics[0].first;
        } else if (query == "rival") {
            if (!rival_.alive) return false;
            ctx[name] = rival_.name;
            ctx[name + "_meaning"] = rival_.meaning;
            ctx[name + "_deeds"] = std::to_string(rival_.deeds);
        } else if (query == "wronged_figure") {
            // Someone you (or a past you) robbed in THIS world. Kin remember.
            int found = -1;
            for (auto& [fi, marks] : npcMarks_)
                if (marks.count("robbed") && !marks.count("settled") &&
                    fi >= 0 && fi < (int)history_.figures.size()) found = fi;
            if (found < 0) return false;
            slotFigure_ = found;
            ctx[name] = history_.figures[found].name;
            ctx[name + "_prof"] = history_.figures[found].profession;
        } else {
            return false; // unknown query: skip the event, loudly absent
        }
    }
    return true;
}

void Game::dealEvent() {
    audio_.playMusicFor(deckTag_, masterSeed_);
    // Dungeons escalate: the last event of a visit draws from the finale deck.
    std::string tag = deckTag_;
    if ((deckTag_ == "dungeon" || deckTag_ == "crash") && eventsLeftHere_ == 1)
        tag = "dungeon_finale";
    for (int attempt = 0; attempt < 6; attempt++) {
        current_ = deck_.draw(runRng_, tag);
        if (!current_ && tag == "dungeon_finale") { tag = "dungeon"; continue; }
        if (!current_ && tag == "crash") { tag = "dungeon"; continue; }
        // Biome decks are smaller; when one runs dry the land defaults.
        if (!current_ && tag == "swamp") { tag = "forest"; continue; }
        if (!current_ && (tag == "mountains" || tag == "coast")) { tag = "road"; continue; }
        if (!current_ && tag == "sea") { tag = "coast"; continue; }
        if (!current_) { enterTravel(); return; }
        // Event-level gate: "trait wanted" events only find the wanted.
        bool gated = false;
        for (auto& w : current_->when)
            if (!evalCond(w)) { gated = true; break; }
        if (gated) continue;
        Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
        pendingArtifact_ = -1;
        slotFigure_ = -1;
        if (!resolveSlots(*current_, ctx)) continue;
        currentCtx_ = ctx;
        currentText_ = grammar_.expand(current_->text, runRng_, ctx);
        choiceTexts_.clear();
        for (auto& c : current_->choices)
            choiceTexts_.push_back(c.requires_.label() +
                                   grammar_.expand(c.text, runRng_, ctx));
        reveal_ = 0.0f;
        if (tag == "dungeon_finale") finalesSeen_++;
        // The company keeps its own commentary.
        compLine_.clear();
        if (comp_.active && runRng_.chance(35)) {
            std::string line = grammar_.expand("{comp_" + comp_.trait + "}", runRng_,
                                               {{"comp", comp_.name}});
            compLine_ = comp_.name + ": \"" + line + "\"";
        }
        screen_ = EVENT;
        return;
    }
    enterTravel();
}

void Game::applyEffects(const std::vector<std::string>& effects) {
    for (auto& fx : effects) {
        std::istringstream ss(fx);
        std::string verb;
        ss >> verb;
        if (verb == "hp") {
            int v = 0; ss >> v;
            if (v < 0) {
                int reduced = -v - ch_.armor();
                v = -(reduced < 1 ? 1 : reduced);
                audio_.thud();
                if (v <= -3) shake_ = (float)(-v > 8 ? 4 : -v / 2);
            }
            ch_.hp += v;
            if (ch_.hp > ch_.maxHp) ch_.hp = ch_.maxHp;
        } else if (verb == "maxhp") {
            int v = 0; ss >> v;
            ch_.maxHp += v;
            if (ch_.hp > ch_.maxHp) ch_.hp = ch_.maxHp;
        } else if (verb == "money") {
            int v = 0; ss >> v;
            if (v > 0) audio_.coin();
            ch_.money += v;
            if (ch_.money < 0) ch_.money = 0;
        } else if (verb == "credits") {
            int v = 0; ss >> v;
            ch_.credits += v;
            if (ch_.credits < 0) ch_.credits = 0;
        } else if (verb == "stat") {
            std::string which; int v = 0;
            ss >> which >> v;
            int i = statFromName(which);
            ch_.stats[i] += v;
            if (ch_.stats[i] < 1) ch_.stats[i] = 1;
        } else if (verb == "item") {
            std::string id; ss >> id;
            if ((int)ch_.pack.size() < ch_.packMax) {
                ch_.pack.push_back(makeItem(id));
                audio_.chime();
            }
        } else if (verb == "loot") {
            std::string tier; ss >> tier;
            if ((int)ch_.pack.size() < ch_.packMax) {
                ItemInstance item = items_.loot(runRng_, tier);
                bindQuirk(item);
                ch_.pack.push_back(item);
                audio_.chime();
            }
        } else if (verb == "removeitem") {
            std::string id; ss >> id;
            ch_.removeItem(id);
        } else if (verb == "companion") {
            std::string id; ss >> id;
            setCompanion(id);
        } else if (verb == "companion_leave") {
            dismissCompanion(false);
        } else if (verb == "companion_dies") {
            dismissCompanion(true);
        } else if (verb == "contract") {
            offerContract();
        } else if (verb == "slay_beast") {
            // You did what three armies couldn't. The Chronicle takes note.
            if (slotBeast_ >= 0 && slotBeast_ < (int)history_.beasts.size()) {
                Beast& b = history_.beasts[slotBeast_];
                b.died = history_.presentYear;
                ChronEntry e;
                e.id = (int)history_.chron.size();
                e.year = history_.presentYear;
                e.type = "pc_beast_slain";
                e.beast = slotBeast_;
                e.extra = ch_.name.conlang;
                history_.chron.push_back(e);
                newsLine_ = b.name + " is dead. You were there. You were the reason.";
                audio_.fanfare();
#if defined(__EMSCRIPTEN__)
                // Daily world? The other players hear about this.
                uint64_t dailySeed = (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
                if (masterSeed_ == dailySeed) {
                    nlohmann::json d = {{"day", (int)(time(nullptr) / 86400)},
                                        {"text", ch_.name.conlang + " slew " + b.name +
                                                 " on day " + std::to_string(ch_.day) + "."}};
                    rr_submit_deed(d.dump().c_str());
                }
#endif
            }
        } else if (verb == "legacy_bless") {
            heirBlessing_ = true;
        } else if (verb == "rep") {
            int v = 0; ss >> v;
            int region = (currentSite_ >= 0) ? world_.sites[currentSite_].region : -1;
            int owner = regionOwner(region);
            if (owner >= 0) {
                rep_[owner] += v;
                if (rep_[owner] > 50) rep_[owner] = 50;
                if (rep_[owner] < -50) rep_[owner] = -50;
            }
        } else if (verb == "shop") {
            pendingShop_ = true;
        } else if (verb == "goto") {
            ss >> forcedNextId_;
        } else if (verb == "take_artifact") {
            if (pendingArtifact_ >= 0 && (int)ch_.pack.size() < ch_.packMax) {
                HArtifact& a = history_.artifacts[pendingArtifact_];
                a.claimed = true;
                a.restingSite = -1;
                ItemInstance item;
                item.templateId = "artifact";
                item.name = a.display();
                item.type = "weapon";
                item.value = 40;
                item.passive = "check str +2";
                item.artifactId = pendingArtifact_;
                std::string prov = "Forged of " + a.material + ".";
                for (int d : a.deeds)
                    prov += " " + RenderChronEntry(history_.chron[d], history_, world_,
                                                   grammar_, runRng_);
                item.provenance = prov;
                ch_.pack.push_back(item);
            }
        } else if (verb == "trait") {
            std::string t; ss >> t;
            if (!t.empty() && t[0] == '+') ch_.traits.insert(t.substr(1));
            else if (!t.empty() && t[0] == '-') ch_.traits.erase(t.substr(1));
        } else if (verb == "npc_mark") {
            std::string mark; ss >> mark;
            if (slotFigure_ >= 0) npcMarks_[slotFigure_].insert(mark);
        } else if (verb == "npc_unmark") {
            // A grudge, formally retired. The family will find a new hobby.
            std::string mark; ss >> mark;
            if (slotFigure_ >= 0) {
                npcMarks_[slotFigure_].erase(mark);
                npcMarks_[slotFigure_].insert("settled");
            }
        } else if (verb == "learn") {
            // Words of the old tongue, kept across every run and every world.
            int n = 0; ss >> n;
            profile_.wordsLearned += n;
            SaveProfile(profile_);
            audio_.chime();
        } else if (verb == "rival_dies") {
            if (rival_.alive) {
                rival_.alive = false;
                newsLine_ = rival_.name + " will not be racing anyone anywhere again.";
            }
        } else if (verb == "favor") {
            // Devotion (or blasphemy) with the god this event summoned.
            int v = 0; ss >> v;
            if (slotGod_ >= 0) {
                favor_[slotGod_] += v;
                if (favor_[slotGod_] < -3 && !ch_.hasTrait("cursed")) {
                    ch_.traits.insert("cursed"); // wrath has a paper trail
                    favor_[slotGod_] = 0;
                }
            }
        } else if (verb == "die") {
            std::string rest;
            std::getline(ss, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            ch_.dead = true;
            if (!rest.empty()) ch_.epitaph = rest;
        } else if (verb == "finish") {
            // A life completed on its own terms — ascension, retirement.
            // The run ends; the Chronicle takes you in ALIVE.
            std::string rest;
            std::getline(ss, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            ch_.dead = true;
            finishedWell_ = true;
            if (!rest.empty()) ch_.epitaph = rest;
#if defined(__EMSCRIPTEN__)
            // A completed life is front-page news in a shared world.
            uint64_t dailySeed = (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
            if (masterSeed_ == dailySeed) {
                nlohmann::json d = {{"day", (int)(time(nullptr) / 86400)},
                                    {"text", ch_.name.conlang + " finished their story on day " +
                                             std::to_string(ch_.day) + ", alive."}};
                rr_submit_deed(d.dump().c_str());
            }
#endif
        }
    }
    if (ch_.hp <= 0) ch_.dead = true;
    // A blessing is a one-time refusal to die. The gods take notes.
    if (ch_.dead && ch_.hasTrait("blessed")) {
        ch_.traits.erase("blessed");
        ch_.dead = false;
        ch_.hp = 1;
        ch_.epitaph.clear();
        blessingSpent_ = true;
    }
    // A miracle: if a god favors you enough, they spend it all to keep you
    // standing — once per life, never for the willingly finished (R5).
    if (ch_.dead && !finishedWell_ && !miracleUsed_) {
        int best = -1, bestFavor = 4; // threshold: 5+ favor
        for (auto& [gi, f] : favor_)
            if (f > bestFavor) { best = gi; bestFavor = f; }
        if (best >= 0 && best < (int)history_.gods.size()) {
            miracleUsed_ = true;
            favor_[best] = 0; // the account is emptied
            ch_.dead = false;
            ch_.hp = 3;
            ch_.epitaph.clear();
            newsLine_ = history_.gods[best].name + " reaches down and puts you back on your feet. The debt is unspoken and enormous.";
            audio_.fanfare();
            shake_ = 3.0f;
        }
    }
}

void Game::chooseOption(int idx) {
    if (!current_ || idx < 0 || idx >= (int)current_->choices.size()) return;
    const Choice& choice = current_->choices[idx];
    if (!choice.requires_.met(ch_)) return;
    if (!choice.check.stat.empty()) audio_.dice();
    blessingSpent_ = false;
    outcome_ = EventDeck::resolve(choice, ch_, runRng_,
                                  [this](const std::string& c) { return evalCond(c); });
    outcome_.text = grammar_.expand(outcome_.text, runRng_, currentCtx_);
    applyEffects(outcome_.effects);
    ch_.eventsSurvived++;
    if (ch_.dead && ch_.epitaph.empty())
        ch_.epitaph = grammar_.expand("{epitaph}", runRng_, currentCtx_);
    // The journey log: what happened, where, in your words (R5 replays).
    if (journey_.size() < 240) {
        JourneyStep step;
        step.day = ch_.day;
        step.site = siteName_.substr(0, 40);
        step.choice = (idx < (int)choiceTexts_.size() ? choiceTexts_[idx] : choice.text)
                          .substr(0, 70);
        step.outcome = outcome_.text.substr(0, 110);
        journey_.push_back(step);
    }
    screen_ = OUTCOME;
}

void Game::continueAfterOutcome() {
    if (ch_.dead) {
        // The gate between lives: one card, one god, one bargain (R3).
        if (!finishedWell_ && !afterlifeShown_) {
            afterlifeShown_ = true;
            const Event* gate = deck_.find("afterlife_gate");
            if (gate) {
                Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
                pendingArtifact_ = -1;
                if (resolveSlots(*gate, ctx)) {
                    current_ = gate;
                    currentCtx_ = ctx;
                    currentText_ = grammar_.expand(gate->text, runRng_, ctx);
                    choiceTexts_.clear();
                    for (auto& c : gate->choices)
                        choiceTexts_.push_back(c.requires_.label() +
                                               grammar_.expand(c.text, runRng_, ctx));
                    reveal_ = 0.0f;
                    compLine_.clear();
                    audio_.dirge();
                    screen_ = EVENT;
                    return;
                }
            }
        }
        screen_ = DEATH;
        return;
    }
    checkPurposes();
    if (screen_ == INFO) return; // ambition/contract fanfare; resumes after
    if (pendingShop_) {
        pendingShop_ = false;
        openVendor();
        return;
    }
    if (!forcedNextId_.empty()) {
        const Event* next = deck_.find(forcedNextId_);
        forcedNextId_.clear();
        if (next) {
            current_ = next;
            Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
            pendingArtifact_ = -1;
            if (resolveSlots(*next, ctx)) {
                currentCtx_ = ctx;
                currentText_ = grammar_.expand(next->text, runRng_, ctx);
                choiceTexts_.clear();
                for (auto& c : next->choices)
                    choiceTexts_.push_back(c.requires_.label() +
                                           grammar_.expand(c.text, runRng_, ctx));
                reveal_ = 0.0f;
                screen_ = EVENT;
                return;
            }
        }
    }
    eventsLeftHere_--;
    if (eventsLeftHere_ > 0) dealEvent();
    else enterTravel();
}

void Game::openInventory() {
    returnScreen_ = screen_;
    invSelected_ = -1;
    screen_ = INVENTORY;
}

void Game::openVendor() {
    vendorStock_.clear();
    int n = runRng_.range(4, 6);
    for (int i = 0; i < n; i++) {
        ItemInstance item = items_.loot(runRng_, runRng_.chance(65) ? "common" : "fine");
        bindQuirk(item);
        vendorStock_.push_back(item);
    }
    vendorSellMode_ = false;
    vendorLine_ = grammar_.expand("{vendor_line}", runRng_);
    screen_ = VENDOR;
}

void Game::showInfo(const std::string& text) {
    infoText_ = text;
    infoBack_ = INVENTORY;
    screen_ = INFO;
}

void Game::frame(Vector2 mouse, bool pressed) {
    pressed_ = pressed;
    audio_.update();
    if (shake_ > 0.0f) {
        shake_ -= GetFrameTime() * 10.0f;
        if (shake_ < 0.0f) shake_ = 0.0f;
    }
    if (!enteringSeed_ && screen_ != TITLE && IsKeyPressed(KEY_M)) audio_.toggleMute();
    // Dev keys for content authoring: trait-gated events are untestable by
    // dice alone. Undocumented, harmless (roguelike full of curses anyway).
    if (IsKeyPressed(KEY_F9)) ch_.traits.insert("cursed");
    if (IsKeyPressed(KEY_F10)) ch_.traits.insert("wanted");
    if (IsKeyPressed(KEY_F11)) cardRequested_ = true; // screenshot any screen

    ClearBackground(PAL_BG);
    switch (screen_) {
        case TITLE:     drawTitle(mouse); break;
        case CLASSPICK: drawClassPick(mouse); break;
        case AMBITION:  drawAmbitionPick(mouse); break;
        case TRAVEL:    drawTravel(mouse); break;
        case EVENT:     drawEvent(mouse); break;
        case OUTCOME:   drawOutcome(); break;
        case DEATH:     drawDeath(mouse); break;
        case INVENTORY: drawInventory(mouse); break;
        case INFO:      drawInfo(); break;
        case VENDOR:    drawVendor(mouse); break;
        case WORLDMAP:  drawWorldMap(mouse); break;
        case CHRONICLE: drawChronicle(mouse); break;
        case SAGA:      drawSaga(mouse); break;
        case REPLAY:    drawReplay(mouse); break;
    }
}

bool Game::uiButton(Rectangle r, const char* label, Vector2 mouse) {
    bool hover = CheckCollisionPointRec(mouse, r);
    DrawRectangleRec(r, hover ? PAL_ROW : Color{31, 27, 46, 255});
    DrawRectangleLinesEx(r, 1, PAL_DARK);
    int w = MeasureText(label, 10);
    DrawText(label, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - 10) / 2), 10,
             hover ? PAL_GOLD : PAL_DIM);
    if (hover && pressed_) {
        audio_.blip();
        return true;
    }
    return false;
}

int Game::optionRows(const std::vector<std::string>& rows,
                     const std::vector<bool>& enabled, Vector2 mouse) {
    const int rowH = 13;
    int n = (int)rows.size();
    int y0 = kH - n * rowH - 4;
    int clicked = -1;
    for (int i = 0; i < n; i++) {
        Rectangle r = {4, (float)(y0 + i * rowH - 1), kW - 8, rowH - 1};
        bool hover = enabled[i] && CheckCollisionPointRec(mouse, r);
        if (hover) DrawRectangleRec(r, PAL_ROW);
        std::string label = std::to_string(i + 1) + ") " + rows[i];
        while (!label.empty() && MeasureText((label + "..").c_str(), 10) > kW - 16)
            label.pop_back();
        if (label.size() < rows[i].size() + 3) label += "..";
        DrawText(label.c_str(), 8, y0 + i * rowH, 10, enabled[i] ? (hover ? PAL_GOLD : PAL_INK) : PAL_DARK);
        if (hover && pressed_) clicked = i;
    }
    for (int i = 0; i < n && i < 9; i++)
        if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i))
            if (enabled[i]) clicked = i;
    return clicked;
}

void Game::drawTopBar() {
    std::string hp = "HP " + std::to_string(ch_.hp) + "/" + std::to_string(ch_.maxHp);
    DrawText(hp.c_str(), 4, 3, 10, ch_.hp <= ch_.maxHp / 3 ? PAL_RED : PAL_GREEN);
    std::string cash = std::to_string(ch_.money) + "g";
    if (ch_.credits > 0) cash += " " + std::to_string(ch_.credits) + "c";
    DrawText(cash.c_str(), 70, 3, 10, PAL_GOLD);
    std::string day = "Day " + std::to_string(ch_.day);
    DrawText(day.c_str(), 130, 3, 10, PAL_DIM);
    const int siteMaxW = kW - 4 - (130 + MeasureText(day.c_str(), 10) + 8);
    std::string site = siteName_;
    while (!site.empty() && MeasureText((site + "..").c_str(), 10) > siteMaxW)
        site.pop_back();
    if (site.size() < siteName_.size()) site += "..";
    int w = MeasureText(site.c_str(), 10);
    DrawText(site.c_str(), kW - w - 4, 3, 10, PAL_DIM);
    DrawLine(0, 15, kW, 15, PAL_DARK);
}

void Game::drawTitle(Vector2 mouse) {
    audio_.playMusicFor("title", 1);
    const char* title = "RANDOM ROGUE";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 38, 20, PAL_GOLD);
    if (!dataError_.empty()) {
        DrawTextWrapped(dataError_, 20, 90, kW - 40, PAL_RED);
        return;
    }
    const char* sub = "a game of poor decisions";
    DrawText(sub, (kW - MeasureText(sub, 10)) / 2, 62, 10, PAL_DARK);

    if (enteringSeed_) {
        std::string line = "enter seed: " + seedInput_ + "_";
        DrawText(line.c_str(), (kW - MeasureText(line.c_str(), 10)) / 2, 92, 10, PAL_GOLD);
        const char* hint = "digits, Enter to accept, Esc to cancel";
        DrawText(hint, (kW - MeasureText(hint, 10)) / 2, 108, 10, PAL_DARK);
        int c;
        while ((c = GetCharPressed()) != 0)
            if (c >= '0' && c <= '9' && seedInput_.size() < 12) seedInput_ += (char)c;
        if (IsKeyPressed(KEY_BACKSPACE) && !seedInput_.empty()) seedInput_.pop_back();
        if (IsKeyPressed(KEY_ENTER)) {
            if (!seedInput_.empty()) nextSeed_ = strtoull(seedInput_.c_str(), nullptr, 10);
            enteringSeed_ = false;
        }
        if (IsKeyPressed(KEY_ESCAPE)) enteringSeed_ = false;
        while (GetKeyPressed() != 0) {} // drain so keys don't leak into "any key"
        return;
    }

    if (cachedLegacySeed_ != nextSeed_) {
        pendingLegacy_ = LoadLegacy(nextSeed_);
        cachedLegacySeed_ = nextSeed_;
    }
    std::string seedLine = "world seed: " + std::to_string(nextSeed_);
    DrawText(seedLine.c_str(), (kW - MeasureText(seedLine.c_str(), 10)) / 2, 88, 10, PAL_DIM);
    std::string hint = "S: set seed   M: mute";
    if (!pendingLegacy_.empty())
        hint = "this world remembers " + std::to_string(pendingLegacy_.size()) +
               " of your dead   S: set seed";
    DrawText(hint.c_str(), (kW - MeasureText(hint.c_str(), 10)) / 2, 102, 10,
             pendingLegacy_.empty() ? PAL_DARK : PAL_GOLD);

    // Touch-friendly: buttons carry the whole flow on iPad.
    bool play = uiButton({kW / 2 - 130, 118, 84, 18}, "BEGIN", mouse);
    bool reroll = uiButton({kW / 2 - 42, 118, 84, 18}, "REROLL SEED", mouse);
    uint64_t dailySeed = (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
    // Everyone who presses DAILY today gets the same world.
    if (uiButton({kW / 2 + 46, 118, 84, 18}, "DAILY WORLD", mouse)) {
        nextSeed_ = dailySeed;
        return;
    }
    bool hasSave = !LoadRawRun().empty();
    if (hasSave && uiButton({kW / 2 - 130, 139, 84, 18}, "CONTINUE", mouse)) {
        audio_.blip();
        if (loadRun()) return;
    }
    if (uiButton({kW / 2 + 46, 139, 84, 18}, "YOUR SAGA", mouse)) {
        audio_.blip();
        sagaLives_ = LoadAllLegacy();
        sagaPage_ = 0;
        screen_ = SAGA;
        return;
    }
    if (uiButton({kW / 2 - 42, 139, 84, 18}, "CHRONICLE", mouse)) {
        audio_.blip();
        // Peek at the world before living in it.
        world_ = GenerateWorld(nextSeed_, grammar_, forge_);
        masterSeed_ = nextSeed_;
        runRng_ = Rng(nextSeed_, STREAM_RUNTIME);
        history_ = SimulateHistory(world_, nextSeed_, grammar_, forge_);
        pendingLegacy_ = LoadLegacy(nextSeed_);
        cachedLegacySeed_ = nextSeed_;
        injectLegacy(-1);
        chronPage_ = 0;
        chronCachedPage_ = -1;
        screen_ = CHRONICLE;
        return;
    }

#if defined(__EMSCRIPTEN__)
    // Today's fallen (daily leaderboard).
    if (nextSeed_ == dailySeed) {
        if (!scoresRequested_) {
            scoresRequested_ = true;
            rr_fetch_scores((int)(time(nullptr) / 86400));
        }
        if (!ghostsRequested_) {
            // Strangers' graves arrive while the player reads the title.
            ghostsRequested_ = true;
            rr_fetch_ghosts((int)(time(nullptr) / 86400), "");
        }
        if (!deedsRequested_) {
            deedsRequested_ = true;
            rr_fetch_deeds((int)(time(nullptr) / 86400));
        }
        if (scoresJson_.empty()) {
            char* raw = rr_get_scores();
            if (raw) { scoresJson_ = raw; free(raw); }
        }
        if (!scoresJson_.empty()) {
            nlohmann::json list = nlohmann::json::parse(scoresJson_, nullptr, false);
            if (!list.is_discarded() && list.is_array() && !list.empty()) {
                scoreIds_.clear();
                scoreNames_.clear();
                std::string line = "today's fallen: ";
                for (int i = 0; i < (int)list.size() && i < 3; i++) {
                    if (i) line += "  |  ";
                    std::string nm = list[i].value("name", "?");
                    line += nm + " (" + std::to_string(list[i].value("days", 0)) + "d)";
                    scoreIds_.push_back(list[i].value("id", -1));
                    scoreNames_.push_back(nm);
                }
                while (!line.empty() && MeasureText((line + "..").c_str(), 10) > kW - 8)
                    line.pop_back();
                int lw = MeasureText(line.c_str(), 10);
                Rectangle lr{(float)((kW - lw) / 2), (float)(kH - 22), (float)lw, 11};
                bool hover = CheckCollisionPointRec(mouse, lr) && !scoreIds_.empty() &&
                             scoreIds_[0] >= 0;
                DrawText(line.c_str(), (int)lr.x, kH - 21, 10, hover ? PAL_INK : PAL_GOLD);
                if (hover && pressed_) {
                    audio_.blip();
                    replayCursor_ = 0;
                    replayWho_ = scoreNames_[0];
                    rr_fetch_replay(scoreIds_[0]);
                    replay_.clear();
                    replayPage_ = 0;
                    replayRequested_ = true;
                    screen_ = REPLAY;
                    return;
                }
            }
        }
    }
#endif
    std::string runs = "deaths: " + std::to_string(profile_.deaths) +
                       "   completed: " + std::to_string(profile_.livesCompleted) +
                       "   best: " + std::to_string(profile_.bestDays) + "d" +
                       "   lexicon: " + std::to_string(profile_.wordsLearned) + " words";
    DrawText(runs.c_str(), (kW - MeasureText(runs.c_str(), 10)) / 2, kH - 10, 10, PAL_DARK);

    if (reroll) {
        nextSeed_ = (nextSeed_ * 6364136223846793005ULL + 1442695040888963407ULL) % 1000000000ULL;
        return;
    }
    int key = GetKeyPressed();
    if (key == KEY_S) { enteringSeed_ = true; seedInput_.clear(); return; }
    if (key == KEY_M) { audio_.toggleMute(); return; }
    if (key == KEY_R) {
        nextSeed_ = (nextSeed_ * 6364136223846793005ULL + 1442695040888963407ULL) % 1000000000ULL;
        return;
    }
    if (play || key != 0) { audio_.blip(); screen_ = CLASSPICK; }
}

void Game::drawClassPick(Vector2 mouse) {
    const char* head = "WHO ARE YOU THIS TIME?";
    DrawText(head, (kW - MeasureText(head, 10)) / 2, 24, 10, PAL_GOLD);

    auto classes = startClasses();
    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& c : classes) {
        if (c.unlocked) rows.push_back(std::string(c.name) + " - " + c.blurb);
        else rows.push_back(std::string(c.name) + " (locked: " + c.lockHint + ")");
        ok.push_back(c.unlocked);
    }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        audio_.blip();
        pendingClass_ = pick;
        // Roll three ambitions to choose from — deterministic per world+generation.
        Rng ambRng(nextSeed_ ^ (pendingLegacy_.size() * 8191ULL), 99);
        ambitionChoices_.clear();
        while ((int)ambitionChoices_.size() < 3) {
            int a = ambRng.range(0, 5);
            bool dup = false;
            for (int c : ambitionChoices_)
                if (c == a) dup = true;
            if (!dup) ambitionChoices_.push_back(a);
        }
        screen_ = AMBITION;
    }
}

void Game::drawAmbitionPick(Vector2 mouse) {
    const char* head = "WHY ARE YOU DOING THIS?";
    DrawText(head, (kW - MeasureText(head, 10)) / 2, 24, 10, PAL_GOLD);
    const char* sub = "an ambition gives a life shape. fulfilling one leaves a mark.";
    DrawText(sub, (kW - MeasureText(sub, 10)) / 2, 40, 10, PAL_DARK);

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (int a : ambitionChoices_) {
        rows.push_back(std::string(kAmbitions[a].name) + " - " + kAmbitions[a].desc);
        ok.push_back(true);
    }
    rows.push_back("No ambition - pure wandering");
    ok.push_back(true);
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        audio_.blip();
        if (pick < (int)ambitionChoices_.size()) {
            ambition_.id = ambitionChoices_[pick];
            ambition_.name = kAmbitions[ambition_.id].name;
            ambition_.desc = kAmbitions[ambition_.id].desc;
            ambition_.done = false;
        } else {
            ambition_ = Ambition{};
        }
        newRun(pendingClass_);
    }
}

void Game::drawTravel(Vector2 mouse) {
    siteName_ = "the map";
    drawTopBar();
    drawPortrait(kW - 25, 23, 2, ch_.name.conlang);
    std::string head = "You are " + ch_.name.conlang + " \"" + ch_.name.meaning +
                       "\", adrift in the world of " + world_.name.conlang + ", " +
                       world_.name.meaning + ".";
    int y = DrawTextWrapped(head, 8, 22, kW - 46, PAL_INK);
    y += 2;
    std::string stats;
    for (int i = 0; i < STAT_COUNT; i++) {
        stats += statName(i);
        stats += " " + std::to_string(ch_.stats[i]) + "  ";
    }
    y = DrawTextWrapped(stats, 8, y, kW - 16, PAL_DARK);
    std::string sky = "It is " + season_ +
                      (weather_ == "clear" ? ", skies clear." : ", " + weather_ + ".");
    if (!newsLine_.empty()) sky += "  NEWS: " + newsLine_;
    y = DrawTextWrapped(sky, 8, y + 1, kW - 16, PAL_DIM, y + 24);
    if (comp_.active) {
        std::string cline = "With you: " + comp_.name + ", " + comp_.kind;
        y = DrawTextWrapped(cline, 8, y + 1, kW - 16, PAL_DIM, y + 13);
    }
    std::string purpose;
    if (ambition_.id >= 0 && !ambition_.done)
        purpose = "Ambition: " + std::string(kAmbitions[ambition_.id].desc);
    if (contract_.active)
        purpose += (purpose.empty() ? "" : "  ") + std::string("Job: ") + contract_.desc;
    if (!purpose.empty()) {
        while (!purpose.empty() && MeasureText((purpose + "..").c_str(), 10) > kW - 16)
            purpose.pop_back();
        DrawText(purpose.c_str(), 8, y + 1, 10, PAL_GOLD);
        y += 12;
    }
    if (!ch_.traits.empty()) {
        std::string tline = "You are: ";
        bool first = true;
        for (auto& t : ch_.traits) {
            if (!first) tline += ", ";
            auto it = traitNames_.find(t);
            tline += (it != traitNames_.end()) ? it->second : t;
            first = false;
        }
        y = DrawTextWrapped(tline, 8, y + 1, kW - 16, PAL_DIM);
    }
    DrawText("Where to?", 8, y + 4, 10, PAL_GOLD);
    if (uiButton({(float)(kW - 52), (float)(y + 1), 48, 16}, "PACK", mouse)) {
        openInventory();
        return;
    }
    if (uiButton({(float)(kW - 104), (float)(y + 1), 48, 16}, "MAP", mouse)) {
        returnScreen_ = TRAVEL;
        screen_ = WORLDMAP;
        return;
    }

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& o : travelOptions_) { rows.push_back(o.label); ok.push_back(true); }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        siteName_ = travelOptions_[pick].siteName;
        deckTag_ = travelOptions_[pick].deck;
        currentSite_ = travelOptions_[pick].site;
        if (currentSite_ >= 0) currentRegion_ = world_.sites[currentSite_].region;
        if (travelOptions_[pick].sail) {
            // A voyage: the sea events happen en route, and you make
            // landfall on some OTHER coast entirely.
            std::vector<int> coasts;
            for (int r = 0; r < (int)world_.regions.size(); r++)
                if (world_.regions[r].biome == "coast" && r != currentRegion_)
                    coasts.push_back(r);
            if (!coasts.empty())
                currentRegion_ = coasts[runRng_.range(0, (int)coasts.size() - 1)];
        }
        visitedRegions_.insert(currentRegion_);
        eventsLeftHere_ = (deckTag_ == "dungeon") ? runRng_.range(3, 4) : runRng_.range(2, 3);
        // The road takes what the road takes: each day past the first eats a
        // food item, or failing that, a piece of you.
        int days = travelOptions_[pick].days;
        for (int d = 0; d < days; d++) {
            dailyTick();
            if (d == 0) continue;
            bool ate = false;
            for (size_t i = 0; i < ch_.pack.size(); i++) {
                if (ch_.pack[i].type == "food") {
                    ch_.pack.erase(ch_.pack.begin() + i);
                    ate = true;
                    break;
                }
            }
            if (!ate) {
                ch_.hp -= 2;
                if (ch_.hp < 1) ch_.hp = 1; // hunger humbles; it doesn't kill
            }
        }
        dealEvent();
        checkPurposes(); // survey contracts complete on arrival
        return;
    }
    if (IsKeyPressed(KEY_TAB)) openInventory();
}

void Game::drawEvent(Vector2 mouse) {
    drawTopBar();
    // Typewriter: text arrives like someone is telling it to you.
    bool done = reveal_ >= (float)currentText_.size();
    if (!done) {
        reveal_ += GetFrameTime() * 110.0f;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            pressed_)
            reveal_ = (float)currentText_.size();
    }
    int choicesTop = kH - (int)current_->choices.size() * 13 - 6;
    int textBottom = compLine_.empty() ? choicesTop : choicesTop - 13;
    int ty = DrawTextWrapped(currentText_.substr(0, (size_t)reveal_), 8, 22, kW - 16,
                             PAL_INK, textBottom);
    if (!compLine_.empty() && reveal_ >= (float)currentText_.size()) {
        std::string cl = compLine_;
        while (!cl.empty() && MeasureText((cl + "..").c_str(), 10) > kW - 16)
            cl.pop_back();
        if (cl.size() < compLine_.size()) cl += "..";
        DrawText(cl.c_str(), 8, (ty < textBottom ? ty + 2 : textBottom), 10, PAL_DARK);
    }

    std::vector<bool> ok;
    for (auto& c : current_->choices) ok.push_back(c.requires_.met(ch_));
    int pick = optionRows(choiceTexts_, ok, mouse);
    if (pick >= 0) { chooseOption(pick); return; }
    if (IsKeyPressed(KEY_TAB)) openInventory();
}

void Game::drawOutcome() {
    drawTopBar();
    int y = 22;
    if (!outcome_.rollText.empty()) {
        bool success = outcome_.rollText.find("success") != std::string::npos;
        y = DrawTextWrapped(outcome_.rollText, 8, y, kW - 16,
                            success ? PAL_GREEN : PAL_RED, kH - 18);
        y += 4;
    }
    y = DrawTextWrapped(outcome_.text, 8, y, kW - 16, PAL_INK, kH - 18);
    std::string fx;
    for (auto& e : outcome_.effects)
        if (e.rfind("die", 0) != 0 && e != "shop" && e.rfind("goto", 0) != 0)
            fx += "[" + e + "] ";
    if (!fx.empty()) y = DrawTextWrapped(fx, 8, y + 4, kW - 16, PAL_DARK, kH - 18);
    if (blessingSpent_)
        DrawTextWrapped("The blessing spends itself. You live. Somewhere, a ledger updates.",
                        8, y + 4, kW - 16, PAL_GOLD, kH - 18);

    const char* prompt = "[tap or Enter to continue]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        pressed_) {
        continueAfterOutcome();
    }
}

void Game::drawVendor(Vector2 mouse) {
    drawTopBar();
    int y = DrawTextWrapped("\"" + vendorLine_ + "\"", 8, 22, kW - 16, PAL_DIM);
    int rep = localRep();
    std::string mode = vendorSellMode_ ? "Selling from your pack" : "For sale";
    if (rep != 0)
        mode += rep > 0 ? " (they like you here)" : " (they remember what you did)";
    DrawText(mode.c_str(), 8, y + 2, 10, PAL_GOLD);
    if (uiButton({(float)(kW - 120), (float)(y - 1), 62, 15},
                 vendorSellMode_ ? "BUY MODE" : "SELL MODE", mouse)) {
        vendorSellMode_ = !vendorSellMode_;
        return;
    }
    if (uiButton({(float)(kW - 54), (float)(y - 1), 50, 15}, "LEAVE", mouse)) {
        continueAfterOutcome();
        return;
    }

    std::vector<std::string> rows;
    std::vector<bool> ok;
    if (!vendorSellMode_) {
        for (auto& item : vendorStock_) {
            int price = buyPrice(item);
            rows.push_back(item.displayName() + " - " + std::to_string(price) + "g");
            ok.push_back(ch_.money >= price && (int)ch_.pack.size() < ch_.packMax);
        }
        if (vendorStock_.empty()) {
            DrawTextWrapped("Sold out. The vendor gestures proudly at nothing.", 8, y + 18,
                            kW - 16, PAL_DIM);
        }
    } else {
        for (auto& item : ch_.pack) {
            rows.push_back(item.displayName() + " - " + std::to_string(sellPrice(item)) + "g");
            ok.push_back(true);
        }
        if (ch_.pack.empty())
            DrawTextWrapped("You have nothing to sell but stories, and those are free.", 8,
                            y + 18, kW - 16, PAL_DIM);
    }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        if (!vendorSellMode_) {
            ItemInstance item = vendorStock_[pick];
            ch_.money -= buyPrice(item);
            ch_.pack.push_back(item);
            vendorStock_.erase(vendorStock_.begin() + pick);
        } else {
            ch_.money += sellPrice(ch_.pack[pick]);
            ch_.pack.erase(ch_.pack.begin() + pick);
        }
        return;
    }
    if (IsKeyPressed(KEY_TAB)) vendorSellMode_ = !vendorSellMode_;
    if (IsKeyPressed(KEY_ESCAPE)) continueAfterOutcome();
}

void Game::drawInventory(Vector2 mouse) {
    siteName_ = "your pack";
    drawTopBar();
    if (invSelected_ < 0) {
        std::string head = "Your pack (" + std::to_string(ch_.pack.size()) + "/" +
                           std::to_string(ch_.packMax) + ")";
        DrawText(head.c_str(), 8, 22, 10, PAL_GOLD);
        if (ch_.pack.empty()) {
            DrawTextWrapped("Empty. Your worldly possessions are: hope.", 8, 38, kW - 16, PAL_DIM);
        } else {
            std::vector<std::string> rows;
            std::vector<bool> ok;
            for (auto& item : ch_.pack) {
                rows.push_back(item.displayName());
                ok.push_back(true);
            }
            int pick = optionRows(rows, ok, mouse);
            if (pick >= 0) { invSelected_ = pick; return; }
        }
        if (uiButton({(float)(kW - 54), 20, 50, 15}, "BACK", mouse)) {
            screen_ = returnScreen_;
            return;
        }
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) screen_ = returnScreen_;
        return;
    }

    // Item detail
    if (invSelected_ >= (int)ch_.pack.size()) { invSelected_ = -1; return; }
    ItemInstance& item = ch_.pack[invSelected_];
    int y = DrawTextWrapped(item.name, 8, 22, kW - 16, PAL_GOLD);
    if (!item.quirk.empty())
        y = DrawTextWrapped("It " + item.quirk + ".", 8, y + 2, kW - 16, PAL_DIM);
    if (!item.provenance.empty())
        y = DrawTextWrapped(item.provenance, 8, y + 2, kW - 16, PAL_INK);
    // The old tongue on old things: readable once you've learned enough (R4).
    if (item.artifactId >= 0 && item.artifactId < (int)history_.artifacts.size()) {
        const HArtifact& a = history_.artifacts[item.artifactId];
        if (!a.meaning.empty()) {
            uint32_t h = 2166136261u;
            for (char c : a.conlang) h = (h ^ (uint8_t)c) * 16777619u;
            int need = 10 + (int)(h % 30);
            if (profile_.wordsLearned >= need)
                y = DrawTextWrapped("The script yields to you now. It reads: \"" +
                                        a.meaning + "\".", 8, y + 2, kW - 16, PAL_GREEN);
            else
                y = DrawTextWrapped("There is script on it in the old tongue. You know " +
                                        std::to_string(profile_.wordsLearned) + " of the " +
                                        std::to_string(need) + " words you'd need.",
                                    8, y + 2, kW - 16, PAL_DIM);
        }
    }
    if (!item.passive.empty())
        y = DrawTextWrapped("[" + item.passive + "]", 8, y + 2, kW - 16, PAL_DARK);
    std::string val = "Worth ~" + std::to_string(item.value) + "g";
    y = DrawTextWrapped(val, 8, y + 2, kW - 16, PAL_DARK);

    bool usable = !item.useEffects.empty();
    std::vector<std::string> rows = {usable ? "Use it" : "Contemplate it", "Drop it", "Back"};
    std::vector<bool> ok = {true, true, true};
    int pick = optionRows(rows, ok, mouse);
    if (pick == 0) {
        if (usable) {
            bool isBook = false, isGossip = false, isMap = false;
            for (auto& u : item.useEffects) {
                if (u == "read") isBook = true;
                if (u == "gossip") isGossip = true;
                if (u == "map") isMap = true;
            }
            if (isBook || isGossip || isMap) {
                profile_.booksRead++;
                booksThisRun_++;
                if (isBook) profile_.wordsLearned += 2; // reading teaches the tongue
                SaveProfile(profile_);
                std::string name = item.name;
                std::string text;
                if (isBook) text = "You read from " + name + ":\n\n" + chronicleExcerpt(3);
                if (isGossip) text = name + " reports, breathlessly:\n\n" + randomRumor();
                if (isMap) {
                    std::string mark;
                    std::vector<int> resting;
                    for (int i = 0; i < (int)history_.artifacts.size(); i++)
                        if (!history_.artifacts[i].claimed && history_.artifacts[i].restingSite >= 0)
                            resting.push_back(i);
                    if (resting.empty()) {
                        mark = "The map marks a spot someone else clearly found first. There's a doodle of them gloating.";
                    } else {
                        const HArtifact& a = history_.artifacts[resting[runRng_.range(0, (int)resting.size() - 1)]];
                        mark = "Once decoded, the map marks " + a.display() + ", resting at " +
                               world_.sites[a.restingSite].name + ". X, as they say.";
                    }
                    text = "You unfold " + name + ". " + mark;
                }
                ch_.pack.erase(ch_.pack.begin() + invSelected_);
                invSelected_ = -1;
                showInfo(text);
                return;
            }
            std::string fxText;
            for (auto& u : item.useEffects) fxText += "[" + u + "] ";
            applyEffects(item.useEffects);
            std::string used = "You use the " + item.name + ". " + fxText;
            ch_.pack.erase(ch_.pack.begin() + invSelected_);
            invSelected_ = -1;
            showInfo(used);
        } else {
            showInfo("You contemplate the " + item.displayName() +
                     ". It offers no answers. It " +
                     (item.quirk.empty() ? "is exactly what it appears to be, which is rare and beautiful."
                                         : item.quirk + ", which is either important or nothing."));
        }
    } else if (pick == 1) {
        ch_.pack.erase(ch_.pack.begin() + invSelected_);
        invSelected_ = -1;
    } else if (pick == 2 || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        invSelected_ = -1;
    }
}

void Game::drawInfo() {
    drawTopBar();
    DrawTextWrapped(infoText_, 8, 22, kW - 16, PAL_INK, kH - 18);
    const char* prompt = "[Enter]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_TAB) ||
        pressed_) {
        screen_ = ch_.dead ? DEATH : infoBack_;
    }
}

// The world as a ring of named places — fog over the unvisited (R3).
void Game::drawWorldMap(Vector2 mouse) {
    drawTopBar();
    std::string head = "The world of " + world_.name.conlang;
    DrawText(head.c_str(), (kW - MeasureText(head.c_str(), 10)) / 2, 20, 10, PAL_GOLD);

    auto biomeColor = [](const std::string& b) -> Color {
        if (b == "forest")    return {88, 140, 78, 255};
        if (b == "desert")    return {196, 164, 96, 255};
        if (b == "swamp")     return {96, 116, 84, 255};
        if (b == "mountains") return {150, 150, 165, 255};
        if (b == "coast")     return {92, 128, 168, 255};
        return {140, 150, 96, 255}; // plains
    };
    int n = (int)world_.regions.size();
    float cx = kW / 2.0f, cy = 96.0f, rx = 128.0f, ry = 58.0f;
    std::vector<Vector2> pos(n);
    for (int i = 0; i < n; i++) {
        float a = 2.0f * PI * i / n - PI / 2.0f;
        pos[i] = {cx + rx * cosf(a), cy + ry * sinf(a)};
    }
    // Roads first, under the nodes. Each edge drawn once.
    for (int i = 0; i < n; i++)
        for (int nb : world_.regions[i].neighbors)
            if (nb > i)
                DrawLineV(pos[i], pos[nb], Color{52, 46, 66, 255});
    int hovered = -1;
    for (int i = 0; i < n; i++) {
        bool seen = visitedRegions_.count(i) > 0;
        Color c = seen ? biomeColor(world_.regions[i].biome) : Color{58, 54, 70, 255};
        DrawRectangle((int)pos[i].x - 3, (int)pos[i].y - 3, 6, 6, c);
        if (i == currentRegion_) {
            DrawRectangleLines((int)pos[i].x - 5, (int)pos[i].y - 5, 10, 10, PAL_GOLD);
        }
        if (fabsf(mouse.x - pos[i].x) < 6 && fabsf(mouse.y - pos[i].y) < 6) hovered = i;
    }
    // One label at a time keeps 30 regions readable on a 320px canvas.
    int label = hovered >= 0 ? hovered : currentRegion_;
    if (label >= 0 && label < n) {
        const Region& r = world_.regions[label];
        bool seen = visitedRegions_.count(label) > 0;
        std::string line = seen ? r.name + " - " + r.biome : "somewhere unvisited";
        if (label == currentRegion_) line += "  (you are here)";
        if (seen) {
            int owner = regionOwner(label);
            if (owner >= 0) {
                line += ", held by " + history_.factions[owner].name;
                for (auto& lw : history_.liveWars)
                    if (lw.a == owner || lw.b == owner) { line += "  [war]"; break; }
            }
            if (history_.plaguedRegions.count(label)) line += "  [plague]";
        }
        while (!line.empty() && MeasureText(line.c_str(), 10) > kW - 12) line.pop_back();
        DrawText(line.c_str(), (kW - MeasureText(line.c_str(), 10)) / 2, kH - 30, 10,
                 PAL_INK);
    }
    std::string tally = std::to_string((int)visitedRegions_.size()) + " of " +
                        std::to_string(n) + " regions walked";
    DrawText(tally.c_str(), 8, kH - 16, 10, PAL_DARK);
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        screen_ = returnScreen_;
    }
}

// Page through the whole thousand-year Chronicle, era by era (R3).
void Game::drawChronicle(Vector2 mouse) {
    const int kPerPage = 9;
    int total = (int)history_.chron.size();
    int pages = total > 0 ? (total + kPerPage - 1) / kPerPage : 1;
    if (chronPage_ < 0) chronPage_ = 0;
    if (chronPage_ >= pages) chronPage_ = pages - 1;
    if (chronCachedPage_ != chronPage_) {
        chronLines_.clear();
        // A throwaway RNG: browsing must never disturb run determinism.
        Rng browse(masterSeed_ ^ 0xB00C5ULL, (uint64_t)(chronPage_ + 7));
        for (int i = chronPage_ * kPerPage;
             i < total && i < (chronPage_ + 1) * kPerPage; i++) {
            const ChronEntry& e = history_.chron[i];
            std::string line = "y" + std::to_string(e.year) + "  " +
                RenderChronEntry(e, history_, world_, grammar_, browse);
            chronLines_.push_back(line);
        }
        chronCachedPage_ = chronPage_;
    }
    std::string head = "THE CHRONICLE OF " + world_.name.conlang;
    for (auto& c : head) c = (char)toupper((unsigned char)c);
    DrawText(head.c_str(), (kW - MeasureText(head.c_str(), 10)) / 2, 6, 10, PAL_GOLD);
    std::string era = chronLines_.empty() ? "" :
        std::string(EraName(history_.chron[chronPage_ * kPerPage].year)) +
        "  -  page " + std::to_string(chronPage_ + 1) + "/" + std::to_string(pages);
    DrawText(era.c_str(), (kW - MeasureText(era.c_str(), 10)) / 2, 18, 10, PAL_DIM);
    int y = 32;
    for (auto& line : chronLines_) {
        std::string l = line;
        while (!l.empty() && MeasureText((l + "..").c_str(), 10) > kW - 12) l.pop_back();
        if (l.size() < line.size()) l += "..";
        DrawText(l.c_str(), 6, y, 10, PAL_INK);
        y += 13;
    }
    if (chronLines_.empty())
        DrawText("The pages are blank.", 8, 40, 10, PAL_DIM);
    if (chronPage_ > 0 &&
        (uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse) ||
         IsKeyPressed(KEY_LEFT)))
        chronPage_--;
    if (chronPage_ < pages - 1 &&
        (uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse) ||
         IsKeyPressed(KEY_RIGHT)))
        chronPage_++;
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        screen_ = TITLE;
    }
}

// The Book of You: every life you've lived, in every world still
// remembered, bound as chapters (R5).
void Game::drawSaga(Vector2 mouse) {
    DrawText("THE BOOK OF YOU", (kW - MeasureText("THE BOOK OF YOU", 10)) / 2, 6, 10,
             PAL_GOLD);
    std::string totals = std::to_string(profile_.deaths) + " deaths, " +
                         std::to_string(profile_.livesCompleted) + " lives completed, " +
                         std::to_string(profile_.daysTotal) + " days walked, " +
                         std::to_string(profile_.wordsLearned) + " words learned";
    DrawText(totals.c_str(), (kW - MeasureText(totals.c_str(), 10)) / 2, 18, 10, PAL_DIM);

    const int kPerPage = 3;
    int total = (int)sagaLives_.size();
    int pages = total > 0 ? (total + kPerPage - 1) / kPerPage : 1;
    if (sagaPage_ < 0) sagaPage_ = 0;
    if (sagaPage_ >= pages) sagaPage_ = pages - 1;

    if (total == 0) {
        DrawTextWrapped("The pages are blank. Nobody has died yet, which the book "
                        "finds suspicious.", 30, 60, kW - 60, PAL_DIM);
    }
    int y = 34;
    for (int i = sagaPage_ * kPerPage; i < total && i < (sagaPage_ + 1) * kPerPage; i++) {
        const auto& [seed, life] = sagaLives_[i];
        drawPortrait(8, y, 2, life.name);
        std::string head = "Ch." + std::to_string(i + 1) + "  " + life.name +
                           " \"" + life.meaning + "\"";
        while (!head.empty() && MeasureText(head.c_str(), 10) > kW - 34) head.pop_back();
        DrawText(head.c_str(), 28, y, 10, PAL_GOLD);
        std::string mid = std::to_string(life.days) + " days in world " +
                          std::to_string(seed) + ", ended at " +
                          (life.deathSite.empty() ? "parts unknown" : life.deathSite) +
                          (life.blessing ? "  [blessed the next]" : "");
        bool cut = false;
        while (!mid.empty() && MeasureText((mid + "..").c_str(), 10) > kW - 34) {
            mid.pop_back();
            cut = true;
        }
        if (cut) mid += "..";
        DrawText(mid.c_str(), 28, y + 11, 10, PAL_DIM);
        std::string epi = "\"" + life.epitaph + "\"";
        while (!epi.empty() && MeasureText((epi + "..").c_str(), 10) > kW - 34) epi.pop_back();
        DrawText(epi.c_str(), 28, y + 22, 10, PAL_INK);
        y += 38;
    }
    std::string pg = "page " + std::to_string(sagaPage_ + 1) + "/" + std::to_string(pages);
    DrawText(pg.c_str(), (kW - MeasureText(pg.c_str(), 10)) / 2, kH - 16, 10, PAL_DARK);
    if (sagaPage_ > 0 &&
        (uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse) || IsKeyPressed(KEY_LEFT)))
        sagaPage_--;
    if (sagaPage_ < pages - 1 &&
        (uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse) || IsKeyPressed(KEY_RIGHT)))
        sagaPage_++;
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE))
        screen_ = TITLE;
}

// Watch one of today's fallen make every decision that got them killed (R5).
void Game::drawReplay(Vector2 mouse) {
#if defined(__EMSCRIPTEN__)
    if (replayRequested_ && replay_.empty()) {
        char* raw = rr_get_replay();
        std::string rj(raw ? raw : "");
        free(raw);
        nlohmann::json list = nlohmann::json::parse(rj, nullptr, false);
        if (!list.is_discarded() && list.is_array())
            for (auto& s : list) {
                JourneyStep st;
                st.day = s.value("d", 0);
                st.site = s.value("s", "");
                st.choice = s.value("c", "");
                st.outcome = s.value("o", "");
                replay_.push_back(st);
            }
        if (!list.is_discarded()) replayRequested_ = false;
    }
#endif
    std::string head = "THE FALL OF " + replayWho_;
    for (auto& c : head) c = (char)toupper((unsigned char)c);
    while (!head.empty() && MeasureText(head.c_str(), 10) > kW - 8) head.pop_back();
    DrawText(head.c_str(), (kW - MeasureText(head.c_str(), 10)) / 2, 6, 10, PAL_GOLD);

    if (replay_.empty()) {
        DrawTextWrapped(replayRequested_ ? "The chronicle is being fetched from wherever "
                                           "chronicles live..."
                                         : "Their story arrived blank. Some falls keep "
                                           "their secrets.",
                        30, 60, kW - 60, PAL_DIM);
    } else {
        int total = (int)replay_.size();
        if (replayPage_ < 0) replayPage_ = 0;
        if (replayPage_ >= total) replayPage_ = total - 1;
        const JourneyStep& st = replay_[replayPage_];
        std::string where = "Day " + std::to_string(st.day) + ", " + st.site +
                            "   (" + std::to_string(replayPage_ + 1) + "/" +
                            std::to_string(total) + ")";
        DrawText(where.c_str(), (kW - MeasureText(where.c_str(), 10)) / 2, 20, 10, PAL_DIM);
        int y = DrawTextWrapped("They chose: " + st.choice, 8, 38, kW - 16, PAL_INK);
        DrawTextWrapped(st.outcome, 8, y + 6, kW - 16, PAL_DIM, kH - 24);
        if (replayPage_ > 0 &&
            (uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse) || IsKeyPressed(KEY_LEFT)))
            replayPage_--;
        if (replayPage_ < total - 1 &&
            (uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse) || IsKeyPressed(KEY_RIGHT)))
            replayPage_++;
    }
#if defined(__EMSCRIPTEN__)
    // Cycle through the day's other fallen without going back out.
    if ((int)scoreIds_.size() > 1 &&
        uiButton({(float)(kW / 2 - 42), (float)(kH - 20), 84, 16}, "NEXT FALLEN", mouse)) {
        replayCursor_ = (replayCursor_ + 1) % (int)scoreIds_.size();
        if (scoreIds_[replayCursor_] >= 0) {
            replayWho_ = scoreNames_[replayCursor_];
            rr_fetch_replay(scoreIds_[replayCursor_]);
            replay_.clear();
            replayPage_ = 0;
            replayRequested_ = true;
        }
    }
#endif
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE))
        screen_ = TITLE;
}

void Game::drawDeath(Vector2 mouse) {
    const char* title = finishedWell_ ? "A LIFE, COMPLETED" : "YOU DIED";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 30, 20,
             finishedWell_ ? PAL_GOLD : PAL_RED);
    drawPortrait(kW / 2 - 8, 6, 2, ch_.name.conlang);
    std::string who = ch_.name.conlang + " \"" + ch_.name.meaning + "\"";
    DrawText(who.c_str(), (kW - MeasureText(who.c_str(), 10)) / 2, 56, 10, PAL_INK);
#if defined(__EMSCRIPTEN__)
    if (uiButton({4, (float)(kH - 22), 96, 18}, "COPY EPITAPH", mouse)) {
        std::string card = ch_.name.conlang + " \"" + ch_.name.meaning + "\" - " +
                           ch_.epitaph + " - " + std::to_string(ch_.day) +
                           " days, seed " + std::to_string(masterSeed_) +
                           " - random-rogue.com";
        rr_copy_text(card.c_str());
        return;
    }
#endif
    // A PNG of this screen, exported by main.cpp after the frame renders.
    if (uiButton({4, (float)(kH - 44), 96, 18}, "SAVE CARD", mouse)) {
        cardRequested_ = true;
        return;
    }
    int y = DrawTextWrapped(ch_.epitaph, 30, 74, kW - 60, PAL_GOLD);
    std::string run = "Survived " + std::to_string(ch_.day) + " days and " +
                      std::to_string(ch_.eventsSurvived) + " questionable decisions. Died holding " +
                      std::to_string(ch_.money) + " gold.";
    y = DrawTextWrapped(run, 30, y + 6, kW - 60, PAL_DIM);
    std::string seed = "world seed: " + std::to_string(masterSeed_);
    DrawText(seed.c_str(), (kW - MeasureText(seed.c_str(), 10)) / 2, y + 8, 10, PAL_DARK);
    if (((long)(GetTime() * 2)) % 2 == 0) {
        const char* prompt = "press any key";
        DrawText(prompt, (kW - MeasureText(prompt, 10)) / 2, kH - 16, 10, PAL_DARK);
    }
    if (GetKeyPressed() != 0 || pressed_) {
        // Bank the run into the profile exactly once, on the way out.
        if (finishedWell_) profile_.livesCompleted++;
        else profile_.deaths++;
        profile_.daysTotal += ch_.day;
        if (ch_.day > profile_.bestDays) profile_.bestDays = ch_.day;
        SaveProfile(profile_);
        // The Chronicle absorbs you: this death becomes this world's history.
        if (!legacySaved_) {
            LegacyRecord rec;
            rec.name = ch_.name.conlang;
            rec.meaning = ch_.name.meaning;
            rec.epitaph = ch_.epitaph;
            rec.deathSite = siteName_;
            rec.days = ch_.day;
            rec.blessing = heirBlessing_;
            for (auto& item : ch_.pack) {
                if (rec.relics.size() >= 2) break;
                if (item.artifactId >= 0 || item.type == "weapon" || item.type == "armor")
                    rec.relics.emplace_back(item.name, item.quirk);
            }
            AppendLegacy(masterSeed_, rec);
            legacySaved_ = true;
            cachedLegacySeed_ = ~0ULL; // force reload next time
            // NPC grudges survive you; your autosave does not.
            nlohmann::json marks = nlohmann::json::object();
            for (auto& [fig, tags] : npcMarks_)
                marks[std::to_string(fig)] =
                    std::vector<std::string>(tags.begin(), tags.end());
            SaveMarks(masterSeed_, marks.dump());
            clearRun();
#if defined(__EMSCRIPTEN__)
            // Daily world? Your epitaph joins today's fallen.
            uint64_t dailySeed = (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
            if (masterSeed_ == dailySeed && !scoreSubmitted_) {
                scoreSubmitted_ = true;
                nlohmann::json relics = nlohmann::json::array();
                for (auto& [rn, rq] : rec.relics)
                    relics.push_back({{"name", rn}, {"quirk", rq}});
                nlohmann::json steps = nlohmann::json::array();
                for (auto& st : journey_)
                    steps.push_back({{"d", st.day}, {"s", st.site},
                                     {"c", st.choice}, {"o", st.outcome}});
                nlohmann::json s = {{"day", (int)(time(nullptr) / 86400)},
                                    {"name", ch_.name.conlang + " \"" + ch_.name.meaning + "\""},
                                    {"meaning", ch_.name.meaning},
                                    {"days", ch_.day},
                                    {"epitaph", ch_.epitaph},
                                    {"site", siteName_},
                                    {"relics", relics},
                                    {"finished", finishedWell_},
                                    {"journey", steps}};
                rr_submit_score(s.dump().c_str());
            }
#endif
        }
        // Same world by default: go back in, years later, among your ghosts.
        nextSeed_ = masterSeed_;
        screen_ = TITLE;
    }
}
