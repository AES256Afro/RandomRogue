#include "game.h"
#include <algorithm>
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
    try { fetch('https://random-rogue.com/__score', { method: 'POST', headers: { 'content-type': 'application/json' }, body: UTF8ToString(json) }).catch(function(){}); } catch (e) {}
});
EM_JS(void, rr_fetch_scores, (int day), {
    try { fetch('https://random-rogue.com/__scores?day=' + day).then(function(r){ return r.text(); }).then(function(t){ window.__rrScores = t; }).catch(function(){ window.__rrScores = '[]'; }); } catch (e) { window.__rrScores = '[]'; }
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
    try { fetch('https://random-rogue.com/__ghosts?day=' + day + '&n=3&not=' + encodeURIComponent(UTF8ToString(notName))).then(function(r){ return r.text(); }).then(function(t){ window.__rrGhosts = t; }).catch(function(){ window.__rrGhosts = "[]"; }); } catch (e) { window.__rrGhosts = "[]"; }
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
    try { fetch('https://random-rogue.com/__replay?id=' + id).then(function(r){ return r.text(); }).then(function(t){ window.__rrReplay = t; }).catch(function(){ window.__rrReplay = "[]"; }); } catch (e) { window.__rrReplay = "[]"; }
});
EM_JS(char*, rr_get_replay, (), {
    var s = window.__rrReplay || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(void, rr_submit_deed, (const char* json), {
    try { fetch('https://random-rogue.com/__deed', { method: 'POST', headers: { 'content-type': 'application/json' }, body: UTF8ToString(json) }).catch(function(){}); } catch (e) {}
});
EM_JS(void, rr_fetch_deeds, (int day), {
    try { fetch('https://random-rogue.com/__deeds?day=' + day).then(function(r){ return r.text(); }).then(function(t){ window.__rrDeeds = t; }).catch(function(){ window.__rrDeeds = "[]"; }); } catch (e) { window.__rrDeeds = "[]"; }
});
EM_JS(char*, rr_get_deeds, (), {
    var s = window.__rrDeeds || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(char*, rr_get_mod, (), {
    var s = window.__rrMod || "";
    var len = lengthBytesUTF8(s) + 1;
    var buf = _malloc(len);
    stringToUTF8(s, buf, len);
    return buf;
});
EM_JS(void, rr_queue_telemetry, (const char* json, int flush), {
    try {
        window.__rrTelemetry = window.__rrTelemetry || [];
        window.__rrTelemetry.push(JSON.parse(UTF8ToString(json)));
        if (flush || window.__rrTelemetry.length >= 12) {
            var batch = window.__rrTelemetry.splice(0, 32);
            fetch('https://random-rogue.com/__telemetry', {
                method: 'POST', headers: {'content-type': 'application/json'},
                body: JSON.stringify({events: batch})
            }).catch(function(){});
        }
    } catch (e) {}
});
EM_JS(void, rr_clear_telemetry, (), { window.__rrTelemetry = []; });
EM_JS(void, rr_fetch_balance, (), {
    window.__rrBalance = "";
    try { fetch('https://random-rogue.com/__telemetry_stats').then(function(r){ return r.text(); }).then(function(t){ window.__rrBalance = t; }).catch(function(){ window.__rrBalance = "[]"; }); } catch (e) { window.__rrBalance = "[]"; }
});
EM_JS(char*, rr_get_balance, (), {
    var s = window.__rrBalance || "";
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

// Word-wrap into lines without drawing; honors '\n'. The measuring half of
// DrawTextWrapped, shared by the scrollable readers (R9b).
std::vector<std::string> WrapLines(const std::string& text, int width, int fs = 10) {
    std::vector<std::string> lines;
    std::string line, word;
    for (size_t i = 0; i <= text.size(); i++) {
        char c = (i < text.size()) ? text[i] : ' ';
        if (c == ' ' || c == '\n') {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (MeasureText(candidate.c_str(), fs) <= width) {
                line = candidate;
            } else {
                lines.push_back(line);
                line = word;
            }
            word.clear();
            if (c == '\n') {
                lines.push_back(line);
                line.clear();
            }
        } else {
            word += c;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

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
    loadText("assets/data/recipes/items.json",
             [&](const char* t) { items_.loadRecipesJsonText(t); });
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
    // Scramble the clock so fresh visitors don't all see near-identical
    // 78xxxxxxx seeds — every digit should feel rolled (R9).
    nextSeed_ = ((uint64_t)time(nullptr) * 6364136223846793005ULL +
                 1442695040888963407ULL) % 1000000000ULL;
    audio_.init();
    profile_ = LoadProfile();
    // Settings persist across sessions (R7).
    SetMasterVolume(0.35f + 0.3f * (profile_.volume % 3));
    if (profile_.musicOff != audio_.musicMuted()) audio_.toggleMusic();
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
    } else {
        // Index 5 is ALWAYS the Heir; newRun's special-casing depends on it.
        classes.push_back({"Heir", "", "a previous life in this world", false});
    }
    // The deeds of past lives open new doors (R7).
    classes.push_back({"Captain", "a sloop of your own, sea legs",
                       "sail beyond the chart", profile_.horizons >= 1});
    classes.push_back({"Anointed", "incense, and a god already listening",
                       "be saved by a miracle", profile_.miracles >= 1});
    classes.push_back({"Guildchild", "a badge, a stipend, a family name",
                       "take the guildmaster's chair", profile_.guildmaster >= 1});
    return classes;
}

// ---- company & purpose (P5) -------------------------------------------------

struct AmbDef { const char* name; const char* desc; };
static const AmbDef kAmbitions[12] = {
    {"Strike It Rich", "hold 120 gold at once"},
    {"Delver", "reach 3 dungeon finales"},
    {"Bookworm", "read 3 things in one life"},
    {"Survivor", "see day 15"},
    {"Relic Hunter", "carry a true artifact"},
    {"Beloved", "be loved somewhere (+20 rep)"},
    {"Beastslayer", "slay a named beast"},
    {"Polyglot", "learn 8 words in one life"},
    {"Mariner", "make landfall on 3 shores"},
    {"Peacemaker", "settle an old grudge"},
    {"Devout", "reach 5 favor with one god"},
    {"Common Cause", "win 3 victories no one could win alone"},
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

std::string Game::RegionState::description() const {
    std::string out;
    if (prosperity >= 3) out = "thriving";
    else if (prosperity <= -3) out = "impoverished";
    else out = "holding together";
    if (danger >= 3) out += ", perilous";
    else if (danger <= -2) out += ", unusually safe";
    if (unrest >= 3) out += ", and close to revolt";
    else if (unrest <= -2) out += ", and politically sleepy";
    if (flags.count("plagued")) out += " under quarantine";
    if (flags.count("at_war")) out += " behind wartime checkpoints";
    if (flags.count("haunted")) out += " with a documented haunting";
    if (flags.count("refugees")) out += " while sheltering displaced neighbors";
    if (flags.count("trade_boom")) out += " amid a contagious trade boom";
    if (flags.count("spillover")) out += " under pressure from nearby trouble";
    if (flags.count("refugee_quarter")) out += " around a permanent refugee quarter";
    if (flags.count("faith_divided")) out += " amid a public crisis of faith";
    if (flags.count("guild_reformed")) out += " under a newly reformed guild";
    if (flags.count("walking_citizens")) out += " while hosting citizens of a moving town";
    if (flags.count("disputed_estate")) out += " beside an estate with legal opinions";
    if (flags.count("story_moved_on")) out += " after a crisis resolved without you";
    if (flags.count("tenants_union")) out += " with an organized tenants union";
    if (flags.count("worker_coop")) out += " around a worker-owned district";
    if (flags.count("public_clinic")) out += " with a public clinic refusing invoices";
    if (flags.count("clean_power")) out += " under a stubbornly clean power grid";
    if (flags.count("commons")) out += " beside land held in common";
    if (flags.count("mutual_aid")) out += " around a busy mutual-aid table";
    if (flags.count("company_town")) out += " under company-town contracts";
    if (flags.count("evictions")) out += " amid organized evictions";
    if (flags.count("toxic_zone")) out += " beneath a profitable brown cloud";
    if (flags.count("militarized")) out += " under armed administrative concern";
    return out;
}

Game::NpcRelation& Game::relation(int figure) {
    NpcRelation& value = npcRelations_[figure];
    value.lastSeen = ch_.day;
    return value;
}

const Game::SocialTie* Game::socialTieFor(int figure) const {
    for (const SocialTie& tie : socialTies_)
        if (tie.a == figure || tie.b == figure) return &tie;
    return nullptr;
}

void Game::recordTelemetry(int choice, int score, bool runEnd) {
#if defined(__EMSCRIPTEN__)
    if (profile_.analyticsOff || !current_) return;
    int gap = 999;
    auto it = lastEventSerial_.find(current_->id);
    if (it != lastEventSerial_.end()) gap = eventSerial_ - it->second;
    nlohmann::json value = {
        {"v", 1}, {"event", current_->id.substr(0, 64)},
        {"choice", choice}, {"day", std::min(999, ch_.day)},
        {"gap", std::min(999, gap)}, {"score", std::max(0, std::min(300, score))},
        {"deck", deckTag_.substr(0, 24)}, {"end", runEnd ? 1 : 0}};
    rr_queue_telemetry(value.dump().c_str(), runEnd ? 1 : 0);
#else
    (void)choice; (void)score; (void)runEnd;
#endif
}

const Game::NpcRelation* Game::relationIfKnown(int figure) const {
    auto found = npcRelations_.find(figure);
    return found == npcRelations_.end() ? nullptr : &found->second;
}

StoryContext Game::storyContext(const std::string& location) const {
    StoryContext ctx;
    ctx.day = ch_.day;
    ctx.location = location;
    ctx.weather = weather_;
    ctx.companion = comp_.active;
    ctx.quest = contract_.active;
    ctx.knownNpc = !npcRelations_.empty();
    ctx.mystery = mystery_.active && !mystery_.solved;
    ctx.rumors = !rumors_.empty();
    ctx.activeFamilies = activeStoryFamilies();
    ctx.arcBeatDue = !scheduledNextId_.empty();
    for (const ItemInstance& item : ch_.pack)
        if (item.artifactId >= 0) ctx.artifact = true;
    ctx.plague = history_.plaguedRegions.count(currentRegion_) > 0;
    int owner = regionOwner(currentRegion_);
    for (const LiveWar& war : history_.liveWars)
        if (war.a == owner || war.b == owner) ctx.war = true;
    if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
        const RegionState& state = regionStates_[currentRegion_];
        ctx.solidarity = state.solidarity >= 2;
        ctx.pollution = state.pollution >= 2;
        ctx.scarcity = state.supply <= -2 || state.rent >= 3;
    }
    return ctx;
}

std::set<std::string> Game::activeStoryFamilies() const {
    std::set<std::string> out;
    auto add = [&](const std::string& id) {
        const Event* event = deck_.find(id);
        if (event && !event->family.empty()) out.insert(event->family);
    };
    add(scheduledNextId_);
    for (const PendingConsequence& value : consequences_) add(value.eventId);
    return out;
}

void Game::offerContract() {
    if (contract_.active || history_.factions.empty()) return;
    contract_ = Contract{};
    contract_.faction = runRng_.range(0, (int)history_.factions.size() - 1);
    const std::string& patron = history_.factions[contract_.faction].name;
    contract_.acceptedDay = ch_.day;
    std::vector<int> resting;
    for (int i = 0; i < (int)history_.artifacts.size(); i++)
        if (!history_.artifacts[i].claimed && history_.artifacts[i].restingSite >= 0)
            resting.push_back(i);
    std::vector<int> beasts;
    for (int i = 0; i < (int)history_.beasts.size(); i++)
        if (history_.beasts[i].died < 0) beasts.push_back(i);
    std::vector<int> known;
    for (auto& pair : npcRelations_)
        if (pair.first >= 0 && pair.first < (int)history_.figures.size() &&
            history_.figures[pair.first].died < 0)
            known.push_back(pair.first);

    int kind = runRng_.range(0, 5);
    if (kind == 0 && !resting.empty()) {
        int ai = resting[runRng_.range(0, (int)resting.size() - 1)];
        contract_.kind = "artifact";
        contract_.artifactId = ai;
        contract_.reward = 24;
        contract_.desc = "Recover " + history_.artifacts[ai].display() + " (rests at " +
                         world_.sites[history_.artifacts[ai].restingSite].name +
                         ") for " + patron;
    } else if (kind == 1 && !beasts.empty()) {
        int bi = beasts[runRng_.range(0, (int)beasts.size() - 1)];
        contract_.kind = "hunt";
        contract_.beastId = bi;
        contract_.reward = 30;
        contract_.desc = "End the career of " + history_.beasts[bi].name + " for " + patron;
    } else if (kind == 2 && !known.empty()) {
        int fi = known[runRng_.range(0, (int)known.size() - 1)];
        contract_.kind = "reconcile";
        contract_.figureId = fi;
        contract_.reward = 18;
        contract_.desc = "Earn the trust of " + history_.figures[fi].name + " for " + patron;
    } else if (kind == 3) {
        static const char* goods[] = {"bread", "rope", "recording", "incense", "monster_bait"};
        int gi = runRng_.range(0, 4);
        int s = runRng_.range(0, (int)world_.sites.size() - 1);
        contract_.kind = "delivery";
        contract_.requiredItem = goods[gi];
        contract_.siteId = s;
        contract_.reward = 16;
        contract_.desc = "Deliver " + contract_.requiredItem + " to " + world_.sites[s].name +
                         " for " + patron;
    } else if (kind == 4 && mystery_.active && !mystery_.solved) {
        contract_.kind = "mystery";
        contract_.reward = 28;
        contract_.desc = "Resolve " + mystery_.title + " for " + patron;
    } else if (kind == 5) {
        static const char* aid[] = {"bread", "rations", "repair_kit", "sewing_kit"};
        int ai = runRng_.range(0, 3);
        int s = runRng_.range(0, (int)world_.sites.size() - 1);
        contract_.kind = "mutual_aid";
        contract_.requiredItem = aid[ai];
        contract_.siteId = s;
        contract_.reward = 8;
        contract_.desc = "Bring " + contract_.requiredItem + " to the mutual-aid table at " +
                         world_.sites[s].name + ". The people there are the patron";
    } else {
        int s = runRng_.range(0, (int)world_.sites.size() - 1);
        contract_.kind = "survey";
        contract_.siteId = s;
        contract_.reward = 12;
        contract_.desc = "Survey " + world_.sites[s].name + " for " + patron;
    }
    static const char* twists[] = {
        "The patron wants proof, not confidence.",
        "Your rival has heard about the same fee.",
        "Payment includes a favor nobody has priced yet.",
        "The request was filed under a false name."
    };
    int twist = runRng_.range(0, 3);
    contract_.twist = twists[twist];
    contract_.desc += ". " + contract_.twist;
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
        if (contract_.kind == "survey" && contract_.siteId >= 0 &&
            currentSite_ == contract_.siteId) done = true;
        if (contract_.kind == "hunt" && contract_.beastId >= 0 &&
            contract_.beastId < (int)history_.beasts.size() &&
            history_.beasts[contract_.beastId].died >= 0) done = true;
        if (contract_.kind == "reconcile" && contract_.figureId >= 0) {
            const NpcRelation* rel = relationIfKnown(contract_.figureId);
            done = rel && rel->trust >= 3 && rel->grudge <= 1;
        }
        if ((contract_.kind == "delivery" || contract_.kind == "mutual_aid") &&
            currentSite_ == contract_.siteId &&
            ch_.day > contract_.acceptedDay && ch_.hasItem(contract_.requiredItem))
            done = true;
        if (contract_.kind == "mystery" && mystery_.solved) done = true;
        if (done) {
            if (contract_.kind == "delivery" || contract_.kind == "mutual_aid")
                ch_.removeItem(contract_.requiredItem);
            if (contract_.kind == "mutual_aid") {
                collectiveVictories_++;
                if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
                    regionStates_[currentRegion_].solidarity = std::min(
                        5, regionStates_[currentRegion_].solidarity + 2);
                    regionStates_[currentRegion_].supply = std::min(
                        5, regionStates_[currentRegion_].supply + 1);
                    regionStates_[currentRegion_].flags.insert("mutual_aid");
                }
                audio_.solidarity();
            }
            ch_.money += contract_.reward;
            contractsDone_++;
            if (contract_.faction >= 0 && contract_.faction < (int)rep_.size()) {
                rep_[contract_.faction] += 6;
                if (rep_[contract_.faction] > 50) rep_[contract_.faction] = 50;
            }
            audio_.coin();
            ChronEntry deed;
            deed.id = (int)history_.chron.size();
            deed.year = history_.presentYear;
            deed.type = "pc_contract";
            deed.faction = contract_.faction;
            deed.site = currentSite_;
            deed.extra = ch_.name.conlang + " completed a " + contract_.kind + " commission";
            history_.chron.push_back(deed);
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
            case 6: done = beastSlainThisRun_; break;
            case 7: done = wordsThisRun_ >= 8; break;
            case 8: done = landfalls_ >= 3; break;
            case 9: done = settledThisRun_; break;
            case 10:
                for (auto& [gi, f] : favor_)
                    if (f >= 5) done = true;
                break;
            case 11: done = collectiveVictories_ >= 3; break;
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
        wars.push_back({{"a", w.a}, {"b", w.b}, {"d", w.daysLeft},
                        {"entry", w.declaredEntry}});
    nlohmann::json figures = nlohmann::json::array();
    for (auto& f : history_.figures)
        figures.push_back({{"name", f.name}, {"fac", f.faction}, {"trait", f.trait},
                           {"prof", f.profession}, {"born", f.born}, {"died", f.died}});
    nlohmann::json artifacts = nlohmann::json::array();
    for (auto& a : history_.artifacts)
        artifacts.push_back({{"name", a.conlang}, {"meaning", a.meaning},
                             {"mat", a.material}, {"forgedBy", a.forgedBy},
                             {"forgedYear", a.forgedYear}, {"site", a.restingSite},
                             {"claimed", a.claimed}, {"deeds", a.deeds}});
    nlohmann::json beasts = nlohmann::json::array();
    for (auto& b : history_.beasts)
        beasts.push_back({{"name", b.name}, {"region", b.region},
                          {"kills", b.kills}, {"died", b.died}});
    nlohmann::json chron = nlohmann::json::array();
    for (auto& e : history_.chron)
        chron.push_back({{"id", e.id}, {"year", e.year}, {"type", e.type},
                         {"actor", e.actor}, {"fac", e.faction},
                         {"fac2", e.faction2}, {"site", e.site},
                         {"artifact", e.artifact}, {"beast", e.beast},
                         {"cause", e.cause}, {"extra", e.extra}});
    nlohmann::json sites = nlohmann::json::array();
    for (auto& s : world_.sites)
        sites.push_back({{"name", s.name}, {"type", s.type}, {"deck", s.deck},
                         {"region", s.region}});
    nlohmann::json j = {
        {"schema", 6},
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
                      {"kind", contract_.kind}, {"twist", contract_.twist},
                      {"aid", contract_.artifactId}, {"sid", contract_.siteId},
                      {"figure", contract_.figureId}, {"beast", contract_.beastId},
                      {"fac", contract_.faction}, {"rw", contract_.reward},
                      {"accepted", contract_.acceptedDay},
                      {"required", contract_.requiredItem}}},
        {"comp", {{"id", comp_.id}, {"name", comp_.name}, {"kind", comp_.kind},
                  {"trait", comp_.trait}, {"passive", comp_.passive},
                  {"pb", comp_.packBonus}, {"active", comp_.active},
                  {"dt", comp_.daysTogether}}},
        {"finales", finalesSeen_}, {"books", booksThisRun_},
        {"cdone", contractsDone_}, {"wars", wars},
        {"plague", history_.plaguedRegions},
        {"figures", figures}, {"artifacts", artifacts}, {"beasts", beasts},
        {"chron", chron}, {"sites", sites},
        {"presentYear", history_.presentYear}, {"liveStart", history_.liveStartId},
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
    // Seen cards must survive CONTINUE, or the deck deals repeats (R9d).
    j["usedEvents"] = std::vector<std::string>(deck_.used().begin(), deck_.used().end());
    j["season"] = season_;
    j["weather"] = weather_;
    j["news"] = newsLine_;
    j["beastRun"] = beastSlainThisRun_;
    j["wordsRun"] = wordsThisRun_;
    j["landfalls"] = landfalls_;
    j["settledRun"] = settledThisRun_;
    j["afterlife"] = afterlifeShown_;
    j["heirBlessing"] = heirBlessing_;
    j["finishedWell"] = finishedWell_;
    j["blessingSpent"] = blessingSpent_;
    j["pendingShop"] = pendingShop_;
    j["forcedNext"] = forcedNextId_;
    j["siteName"] = siteName_;
    j["deckTag"] = deckTag_;
    j["currentSite"] = currentSite_;
    j["eventsLeft"] = eventsLeftHere_;
    j["resumeOutcome"] = screen_ == OUTCOME;
    j["outcome"] = {{"text", outcome_.text}, {"roll", outcome_.rollText},
                    {"effects", outcome_.effects}};
    j["dead"] = ch_.dead;
    j["epitaph"] = ch_.epitaph;
    nlohmann::json marks = nlohmann::json::object();
    for (auto& [fi, tags] : npcMarks_)
        marks[std::to_string(fi)] = std::vector<std::string>(tags.begin(), tags.end());
    j["npcMarks"] = marks;
    nlohmann::json relations = nlohmann::json::object();
    for (auto& [fi, rel] : npcRelations_)
        relations[std::to_string(fi)] = {
            {"trust", rel.trust}, {"fear", rel.fear}, {"respect", rel.respect},
            {"debt", rel.debt}, {"affection", rel.affection},
            {"grudge", rel.grudge}, {"knowledge", rel.knowledge},
            {"lastSeen", rel.lastSeen}};
    j["npcRelations"] = relations;
    j["director"] = {
        {"ids", std::vector<std::string>(director_.recentIds().begin(), director_.recentIds().end())},
        {"tags", std::vector<std::string>(director_.recentTags().begin(), director_.recentTags().end())},
        {"families", std::vector<std::string>(director_.recentFamilies().begin(), director_.recentFamilies().end())},
        {"approaches", std::vector<std::string>(director_.recentApproaches().begin(), director_.recentApproaches().end())},
        {"counts", director_.tagCounts()}};
    nlohmann::json consequences = nlohmann::json::array();
    for (auto& value : consequences_)
        consequences.push_back({{"event", value.eventId}, {"source", value.source},
                                {"summary", value.summary}, {"day", value.dueDay},
                                {"figure", value.figure}, {"region", value.region}});
    j["consequences"] = consequences;
    j["scheduled"] = {{"event", scheduledNextId_}, {"figure", scheduledFigure_},
                       {"region", scheduledRegion_}};
    nlohmann::json regionState = nlohmann::json::array();
    for (auto& state : regionStates_)
        regionState.push_back({{"prosperity", state.prosperity}, {"danger", state.danger},
                               {"unrest", state.unrest}, {"pressure", state.pressure},
                               {"supply", state.supply}, {"rent", state.rent},
                               {"pollution", state.pollution},
                               {"solidarity", state.solidarity},
                               {"flags", std::vector<std::string>(state.flags.begin(),
                                                                  state.flags.end())}});
    j["regionState"] = regionState;
    j["mystery"] = {{"active", mystery_.active}, {"solved", mystery_.solved},
                     {"tried", mystery_.tried}, {"correct", mystery_.correctVerdict},
                     {"appealed", mystery_.appealed},
                     {"culprit", mystery_.culprit}, {"victim", mystery_.victim},
                     {"site", mystery_.site}, {"artifact", mystery_.artifact},
                     {"decoy", mystery_.decoy}, {"accused", mystery_.accused},
                     {"clues", mystery_.clues}, {"evidence", mystery_.evidence},
                     {"doubt", mystery_.doubt}, {"title", mystery_.title},
                     {"secret", mystery_.secret}, {"public", mystery_.publicStory}};
    nlohmann::json social = nlohmann::json::array();
    for (const SocialTie& tie : socialTies_)
        social.push_back({{"a", tie.a}, {"b", tie.b}, {"affinity", tie.affinity},
                          {"kind", tie.kind}});
    j["social"] = social;
    j["storyEchoes"] = std::vector<std::string>(storyEchoes_.begin(), storyEchoes_.end());
    j["storyEchoRegions"] = storyEchoRegions_;
    j["autonomousArcs"] = autonomousArcResolutions_;
    nlohmann::json rumorState = nlohmann::json::array();
    for (const Rumor& value : rumors_)
        rumorState.push_back({{"id", value.id}, {"text", value.text},
                              {"truth", value.truth}, {"origin", value.origin},
                              {"region", value.region}, {"figure", value.figure},
                              {"age", value.age}, {"reach", value.reach},
                              {"planted", value.planted},
                              {"event", value.foreshadowEvent}, {"due", value.dueDay}});
    j["rumors"] = rumorState;
    j["verifiedRumors"] = std::vector<int>(verifiedRumors_.begin(), verifiedRumors_.end());
    nlohmann::json agendaState = nlohmann::json::array();
    for (const Agenda& value : agendas_)
        agendaState.push_back({{"figure", value.figure}, {"region", value.region},
                               {"progress", value.progress}, {"kind", value.kind},
                               {"active", value.active}});
    j["agendas"] = agendaState;
    j["nextRumor"] = nextRumorId_;
    j["nemesis"] = nemesisFigure_;
    j["collectiveVictories"] = collectiveVictories_;
    j["eventSerial"] = eventSerial_;
    j["lastEvents"] = lastEventSerial_;
    SaveRawRun(j.dump());
}

bool Game::loadRun() {
    std::string raw = LoadRawRun();
    nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded() || !j.contains("seed")) {
        raw = LoadBackupRun();
        j = nlohmann::json::parse(raw, nullptr, false);
        if (!j.is_discarded() && j.contains("seed")) SaveRawRun(raw);
    }
    if (j.is_discarded() || !j.contains("seed")) {
        SaveRawRun("");
        return false;
    }
    int schema = j.value("schema", 1);
    nextSeed_ = j["seed"].get<uint64_t>();
    // Rebuild the world exactly as the save knew it.
    pendingLegacy_ = LoadLegacy(nextSeed_);
    size_t gen = j.value("gen", (size_t)0);
    if (pendingLegacy_.size() > gen) pendingLegacy_.resize(gen);
    cachedLegacySeed_ = nextSeed_;
    ambition_ = Ambition{};
    ambition_.id = j.value("ambId", -1);
    ambition_.done = j.value("ambDone", false);
    if (ambition_.id >= 0 && ambition_.id < 12) {
        ambition_.name = kAmbitions[ambition_.id].name;
        ambition_.desc = kAmbitions[ambition_.id].desc;
    } else if (ambition_.id >= 12) {
        ambition_ = Ambition{};
    }
    suppressStrangers_ = true; // the save knows its own ghosts
    loadingRun_ = true;
    newRun(0);
    loadingRun_ = false;
    suppressStrangers_ = false;
    std::string gj = j.value("ghosts", std::string());
    if (schema < 2 && !gj.empty()) {
        injectStrangers(gj);
    }
    ghostsRaw_ = gj;
    // Schema 2 stores the mutable simulation, including appended strangers,
    // rival deaths, relocated artifacts, ruined cities, and every live entry.
    if (schema >= 2) {
        if (j.contains("figures") && j["figures"].is_array()) {
            history_.figures.clear();
            for (auto& v : j["figures"]) {
                Figure f;
                f.name = v.value("name", ""); f.faction = v.value("fac", -1);
                f.trait = v.value("trait", ""); f.profession = v.value("prof", "");
                f.born = v.value("born", 0); f.died = v.value("died", -1);
                history_.figures.push_back(f);
            }
        }
        if (j.contains("artifacts") && j["artifacts"].is_array()) {
            history_.artifacts.clear();
            for (auto& v : j["artifacts"]) {
                HArtifact a;
                a.conlang = v.value("name", ""); a.meaning = v.value("meaning", "");
                a.material = v.value("mat", ""); a.forgedBy = v.value("forgedBy", -1);
                a.forgedYear = v.value("forgedYear", 0); a.restingSite = v.value("site", -1);
                a.claimed = v.value("claimed", false);
                a.deeds = v.value("deeds", std::vector<int>{});
                history_.artifacts.push_back(a);
            }
        }
        if (j.contains("beasts") && j["beasts"].is_array()) {
            history_.beasts.clear();
            for (auto& v : j["beasts"]) {
                Beast b;
                b.name = v.value("name", ""); b.region = v.value("region", -1);
                b.kills = v.value("kills", 0); b.died = v.value("died", -1);
                history_.beasts.push_back(b);
            }
        }
        if (j.contains("chron") && j["chron"].is_array()) {
            history_.chron.clear();
            for (auto& v : j["chron"]) {
                ChronEntry e;
                e.id = v.value("id", (int)history_.chron.size());
                e.year = v.value("year", 0); e.type = v.value("type", "");
                e.actor = v.value("actor", -1); e.faction = v.value("fac", -1);
                e.faction2 = v.value("fac2", -1); e.site = v.value("site", -1);
                e.artifact = v.value("artifact", -1); e.beast = v.value("beast", -1);
                e.cause = v.value("cause", -1); e.extra = v.value("extra", "");
                history_.chron.push_back(e);
            }
        }
        if (j.contains("sites") && j["sites"].is_array()) {
            int n = std::min((int)world_.sites.size(), (int)j["sites"].size());
            for (int i = 0; i < n; i++) {
                auto& v = j["sites"][i];
                world_.sites[i].name = v.value("name", world_.sites[i].name);
                world_.sites[i].type = v.value("type", world_.sites[i].type);
                world_.sites[i].deck = v.value("deck", world_.sites[i].deck);
                world_.sites[i].region = v.value("region", world_.sites[i].region);
            }
        }
        history_.presentYear = j.value("presentYear", history_.presentYear);
        history_.liveStartId = j.value("liveStart", history_.liveStartId);
    }
    if (j.contains("rival")) {
        auto& r = j["rival"];
        rival_.name = r.value("name", rival_.name);
        rival_.meaning = r.value("meaning", rival_.meaning);
        rival_.alive = r.value("alive", true);
        rival_.deeds = r.value("deeds", 0);
    }
    if (j.contains("usedEvents")) {
        std::set<std::string> used;
        for (auto& id : j["usedEvents"]) used.insert(id.get<std::string>());
        deck_.setUsed(used);
    }
    season_ = j.value("season", season_);
    weather_ = j.value("weather", weather_);
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
    ch_.dead = j.value("dead", false);
    ch_.epitaph = j.value("epitaph", std::string());
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
        contract_.kind = c.value("kind", "");
        contract_.twist = c.value("twist", "");
        contract_.artifactId = c.value("aid", -1);
        contract_.siteId = c.value("sid", -1);
        contract_.figureId = c.value("figure", -1);
        contract_.beastId = c.value("beast", -1);
        contract_.faction = c.value("fac", -1);
        contract_.reward = c.value("rw", 0);
        contract_.acceptedDay = c.value("accepted", 0);
        contract_.requiredItem = c.value("required", "");
        if (contract_.kind.empty() && contract_.active)
            contract_.kind = contract_.artifactId >= 0 ? "artifact" : "survey";
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
        comp_.daysTogether = c.value("dt", 0);
    }
    finalesSeen_ = j.value("finales", 0);
    booksThisRun_ = j.value("books", 0);
    contractsDone_ = j.value("cdone", 0);
    beastSlainThisRun_ = j.value("beastRun", false);
    wordsThisRun_ = j.value("wordsRun", 0);
    landfalls_ = j.value("landfalls", 0);
    settledThisRun_ = j.value("settledRun", false);
    afterlifeShown_ = j.value("afterlife", false);
    heirBlessing_ = j.value("heirBlessing", false);
    finishedWell_ = j.value("finishedWell", false);
    blessingSpent_ = j.value("blessingSpent", false);
    pendingShop_ = j.value("pendingShop", false);
    forcedNextId_ = j.value("forcedNext", std::string());
    newsLine_ = j.value("news", std::string());
    siteName_ = j.value("siteName", std::string("the map"));
    deckTag_ = j.value("deckTag", std::string("road"));
    currentSite_ = j.value("currentSite", -1);
    eventsLeftHere_ = j.value("eventsLeft", 0);
    npcMarks_.clear();
    if (j.contains("npcMarks") && j["npcMarks"].is_object())
        for (auto& [fi, tags] : j["npcMarks"].items())
            for (auto& tag : tags)
                if (tag.is_string()) npcMarks_[atoi(fi.c_str())].insert(tag.get<std::string>());
    npcRelations_.clear();
    if (j.contains("npcRelations") && j["npcRelations"].is_object())
        for (auto& [fi, value] : j["npcRelations"].items()) {
            NpcRelation rel;
            rel.trust = value.value("trust", 0);
            rel.fear = value.value("fear", 0);
            rel.respect = value.value("respect", 0);
            rel.debt = value.value("debt", 0);
            rel.affection = value.value("affection", 0);
            rel.grudge = value.value("grudge", 0);
            rel.knowledge = value.value("knowledge", 0);
            rel.lastSeen = value.value("lastSeen", 0);
            npcRelations_[atoi(fi.c_str())] = rel;
        }
    if (j.contains("director") && j["director"].is_object()) {
        auto& d = j["director"];
        director_.restore(d.value("ids", std::vector<std::string>{}),
                          d.value("tags", std::vector<std::string>{}),
                          d.value("families", std::vector<std::string>{}),
                          d.value("approaches", std::vector<std::string>{}),
                          d.value("counts", std::map<std::string, int>{}));
    }
    consequences_.clear();
    storyEchoes_.clear();
    storyEchoRegions_.clear();
    autonomousArcResolutions_ = 0;
    if (j.contains("consequences") && j["consequences"].is_array())
        for (auto& value : j["consequences"]) {
            PendingConsequence pending;
            pending.eventId = value.value("event", "");
            pending.source = value.value("source", "");
            pending.summary = value.value("summary", "");
            pending.dueDay = value.value("day", 0);
            pending.figure = value.value("figure", -1);
            pending.region = value.value("region", -1);
            if (!pending.eventId.empty()) consequences_.push_back(pending);
        }
    if (j.contains("scheduled") && j["scheduled"].is_object()) {
        scheduledNextId_ = j["scheduled"].value("event", "");
        scheduledFigure_ = j["scheduled"].value("figure", -1);
        scheduledRegion_ = j["scheduled"].value("region", -1);
    }
    if (j.contains("regionState") && j["regionState"].is_array()) {
        regionStates_.clear();
        for (auto& value : j["regionState"]) {
            RegionState state;
            state.prosperity = value.value("prosperity", 0);
            state.danger = value.value("danger", 0);
            state.unrest = value.value("unrest", 0);
            state.pressure = value.value("pressure", 0);
            state.supply = value.value("supply", 0);
            state.rent = value.value("rent", 0);
            state.pollution = value.value("pollution", 0);
            state.solidarity = value.value("solidarity", 0);
            for (auto& flag : value.value("flags", std::vector<std::string>{}))
                state.flags.insert(flag);
            regionStates_.push_back(state);
        }
        if (regionStates_.size() != world_.regions.size())
            regionStates_.resize(world_.regions.size());
    }
    if (j.contains("mystery") && j["mystery"].is_object()) {
        auto& value = j["mystery"];
        mystery_.active = value.value("active", false);
        mystery_.solved = value.value("solved", false);
        mystery_.tried = value.value("tried", false);
        mystery_.correctVerdict = value.value("correct", false);
        mystery_.appealed = value.value("appealed", false);
        mystery_.culprit = value.value("culprit", -1);
        mystery_.victim = value.value("victim", -1);
        mystery_.site = value.value("site", -1);
        mystery_.artifact = value.value("artifact", -1);
        mystery_.decoy = value.value("decoy", mystery_.culprit);
        mystery_.accused = value.value("accused", -1);
        mystery_.clues = value.value("clues", 0);
        mystery_.evidence = value.value("evidence", 0);
        mystery_.doubt = value.value("doubt", 0);
        mystery_.title = value.value("title", "");
        mystery_.secret = value.value("secret", "");
        mystery_.publicStory = value.value("public", "");
    }
    if (j.contains("social") && j["social"].is_array()) {
        socialTies_.clear();
        for (auto& value : j["social"]) {
            SocialTie tie;
            tie.a = value.value("a", -1); tie.b = value.value("b", -1);
            tie.affinity = value.value("affinity", 0);
            tie.kind = value.value("kind", "acquaintance");
            if (tie.a >= 0 && tie.b >= 0) socialTies_.push_back(tie);
        }
    }
    storyEchoes_.clear();
    for (const std::string& family : j.value("storyEchoes", std::vector<std::string>{}))
        storyEchoes_.insert(family);
    storyEchoRegions_ = j.value("storyEchoRegions", std::map<std::string, int>{});
    autonomousArcResolutions_ = j.value("autonomousArcs", 0);
    if (j.contains("rumors") && j["rumors"].is_array()) {
        rumors_.clear();
        for (auto& value : j["rumors"]) {
            Rumor rumor;
            rumor.id = value.value("id", 0);
            rumor.text = value.value("text", "");
            rumor.truth = value.value("truth", 50);
            rumor.origin = value.value("origin", -1);
            rumor.region = value.value("region", -1);
            rumor.figure = value.value("figure", -1);
            rumor.age = value.value("age", 0);
            rumor.reach = value.value("reach", 1);
            rumor.planted = value.value("planted", false);
            rumor.foreshadowEvent = value.value("event", "");
            rumor.dueDay = value.value("due", 0);
            if (!rumor.text.empty()) rumors_.push_back(rumor);
        }
    }
    verifiedRumors_.clear();
    for (int id : j.value("verifiedRumors", std::vector<int>{}))
        verifiedRumors_.insert(id);
    if (j.contains("agendas") && j["agendas"].is_array()) {
        agendas_.clear();
        for (auto& value : j["agendas"]) {
            Agenda agenda;
            agenda.figure = value.value("figure", -1);
            agenda.region = value.value("region", -1);
            agenda.progress = value.value("progress", 0);
            agenda.kind = value.value("kind", "");
            agenda.active = value.value("active", true);
            if (agenda.figure >= 0 && !agenda.kind.empty()) agendas_.push_back(agenda);
        }
    }
    nextRumorId_ = j.value("nextRumor", nextRumorId_);
    nemesisFigure_ = j.value("nemesis", -1);
    collectiveVictories_ = j.value("collectiveVictories", 0);
    eventSerial_ = j.value("eventSerial", 0);
    lastEventSerial_ = j.value("lastEvents", std::map<std::string, int>{});
    history_.liveWars.clear();
    if (j.contains("wars"))
        for (auto& w : j["wars"])
            history_.liveWars.push_back({w.value("a", 0), w.value("b", 0),
                                         w.value("d", 5), w.value("entry", -1)});
    if (j.contains("plague"))
        history_.plaguedRegions = j["plague"].get<std::map<int, int>>();
    bool resumeOutcome = j.value("resumeOutcome", false);
    if (resumeOutcome && j.contains("outcome")) {
        auto& o = j["outcome"];
        outcome_.text = o.value("text", "");
        outcome_.rollText = o.value("roll", "");
        outcome_.effects = o.value("effects", std::vector<std::string>{});
        textScroll_ = 0;
        screen_ = OUTCOME;
    } else {
        enterTravel();
    }
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
    if (mechanical && !items_.quirkPairs().empty()) {
        // Text and mechanics are one record now (R10): the description IS
        // the explanation, even when the player never learns the numbers.
        const auto& pair = runRng_.pick(items_.quirkPairs());
        item.quirk = pair.first;
        item.quirkPassive = pair.second;
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
    if (a == "npc_rel") {
        std::string valueText;
        ss >> valueText;
        const NpcRelation* rel = relationIfKnown(slotFigure_);
        if (!rel) return false;
        int lhs = 0;
        if (b == "trust") lhs = rel->trust;
        else if (b == "fear") lhs = rel->fear;
        else if (b == "respect") lhs = rel->respect;
        else if (b == "debt") lhs = rel->debt;
        else if (b == "affection") lhs = rel->affection;
        else if (b == "grudge") lhs = rel->grudge;
        else if (b == "knowledge") lhs = rel->knowledge;
        else return false;
        int rhs = atoi(valueText.c_str());
        if (c == ">") return lhs > rhs;
        if (c == "<") return lhs < rhs;
        if (c == ">=") return lhs >= rhs;
        if (c == "<=") return lhs <= rhs;
        return lhs == rhs;
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
    if (a == "mystery_active") return mystery_.active && !mystery_.solved;
    if (a == "mystery_solved") return mystery_.solved;
    if (a == "mystery_tried") return mystery_.tried;
    if (a == "mystery_appealed") return mystery_.appealed;
    if (a == "verdict_correct") return mystery_.tried && mystery_.correctVerdict;
    if (a == "accused") return mystery_.accused >= 0;
    if (a == "social_known") return socialTieFor(slotFigure_) != nullptr;
    if (a == "nemesis") return nemesisFigure_ >= 0 &&
        (slotFigure_ < 0 || slotFigure_ == nemesisFigure_);
    if (a == "agenda") {
        const Agenda* agenda = agendaFor(slotFigure_);
        return agenda && (b.empty() || agenda->kind == b);
    }
    if (a == "network") {
        const SocialTie* tie = socialTieFor(slotFigure_);
        return tie && (b.empty() || tie->kind == b);
    }
    if (a == "echo") return storyEchoes_.count(b) > 0;
    if (a == "region") {
        return currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size() &&
               regionStates_[currentRegion_].flags.count(b) > 0;
    }
    if (a == "neighbor") {
        if (currentRegion_ < 0 || currentRegion_ >= (int)world_.regions.size()) return false;
        for (int neighbor : world_.regions[currentRegion_].neighbors)
            if (neighbor >= 0 && neighbor < (int)regionStates_.size() &&
                regionStates_[neighbor].flags.count(b) > 0) return true;
        return false;
    }
    if (a == "contracts") return cmp(contractsDone_);
    if (a == "rumors") return cmp((int)rumors_.size());
    if (a == "collective") return cmp(collectiveVictories_);
    if (a == "solidarity" || a == "pollution" || a == "rent" || a == "supply") {
        if (currentRegion_ < 0 || currentRegion_ >= (int)regionStates_.size()) return false;
        const RegionState& state = regionStates_[currentRegion_];
        if (a == "solidarity") return cmp(state.solidarity);
        if (a == "pollution") return cmp(state.pollution);
        if (a == "rent") return cmp(state.rent);
        return cmp(state.supply);
    }
    if (a == "clues") return cmp(mystery_.clues);
    if (a == "evidence") return cmp(mystery_.evidence);
    if (a == "doubt") return cmp(mystery_.doubt);
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
    // World age: every life this world has taken sharpens it (R7 ratchet).
    worldGen_ = (int)pendingLegacy_.size();
    if (worldGen_ > 9) worldGen_ = 9;
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
        case 6: // Captain: the sea already knows your name
            ch_.pack.push_back(makeItem("sloop"));
            ch_.pack.push_back(makeItem("rations"));
            ch_.stats[STAT_CON] += 1;
            break;
        case 7: // Anointed: a god already listening (favor set post-reset)
            ch_.pack.push_back(makeItem("incense"));
            ch_.pack.push_back(makeItem("bread"));
            ch_.stats[STAT_WIS] += 1;
            break;
        case 8: // Guildchild: born on the ladder (contract credit post-reset)
            ch_.pack.push_back(makeItem("rations"));
            ch_.traits.insert("agent");
            ch_.money += 12;
            break;
        default: // Drifter
            ch_.pack.push_back(makeItem("rations"));
            ch_.pack.push_back(makeItem(runRng_.chance(50) ? "rusty_sword" : "club"));
            break;
    }
    deck_.resetUsed();
    director_.reset();
    pendingArtifact_ = -1;
    pendingShop_ = false;
    forcedNextId_.clear();
    scheduledNextId_.clear();
    scheduledFigure_ = -1;
    scheduledRegion_ = -1;
    consequences_.clear();
    npcMarks_.clear();
    npcRelations_.clear();
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
    beastSlainThisRun_ = false;
    wordsThisRun_ = 0;
    landfalls_ = 0;
    settledThisRun_ = false;
    regionStates_.assign(world_.regions.size(), RegionState{});
    generateMystery();
    generateSocialWeb();
    seedLivingPolitics();
    eventSerial_ = 0;
    lastEventSerial_.clear();
    currentDirectorScore_ = 100;
    // Class kits that touch post-reset state land here (R7).
    if (classIdx == 7 && !history_.gods.empty())
        favor_[runRng_.range(0, (int)history_.gods.size() - 1)] = 2;
    if (classIdx == 8) contractsDone_ = 1;
    // NPC memory outlives you in this world (R3).
    npcMarks_.clear();
    {
        nlohmann::json m = nlohmann::json::parse(LoadMarks(masterSeed_), nullptr, false);
        if (!m.is_discarded() && m.is_object()) {
            nlohmann::json marks = m.contains("marks") ? m["marks"] : m;
            if (marks.is_object())
                for (auto& [fig, tags] : marks.items()) {
                    if (fig == "relations" || fig == "marks") continue;
                    if (!tags.is_array()) continue;
                    for (auto& t : tags)
                        if (t.is_string()) npcMarks_[atoi(fig.c_str())].insert(t.get<std::string>());
                }
            if (m.contains("relations") && m["relations"].is_object())
                for (auto& [fig, value] : m["relations"].items()) {
                    NpcRelation rel;
                    rel.trust = value.value("trust", 0);
                    rel.fear = value.value("fear", 0);
                    rel.respect = value.value("respect", 0);
                    rel.debt = value.value("debt", 0);
                    rel.affection = value.value("affection", 0);
                    rel.grudge = value.value("grudge", 0);
                    rel.knowledge = value.value("knowledge", 0);
                    rel.lastSeen = value.value("lastSeen", 0);
                    npcRelations_[atoi(fig.c_str())] = rel;
                }
        }
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
        if (boardKeyFor(masterSeed_) >= 0) {
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
    if (!ch_.dead && !loadingRun_)
        saveRun(); // autosave: a closed tab shouldn't kill a life
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
        // The world's condition travels with the signpost (R10).
        if (history_.plaguedRegions.count(s.region)) o.label += " [plague]";
        int owner = regionOwner(s.region);
        if (owner >= 0)
            for (auto& w : history_.liveWars)
                if (w.a == owner || w.b == owner) { o.label += " [war]"; break; }
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
    if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
        const RegionState& state = regionStates_[currentRegion_];
        if (state.flags.count("trade_boom")) price = price * 85 / 100;
        price = price * (100 + state.rent * 5 - state.supply * 4) / 100;
        if (state.flags.count("worker_coop")) price = price * 88 / 100;
        if (state.flags.count("company_town")) price = price * 120 / 100;
    }
    return price < 1 ? 1 : price;
}

int Game::sellPrice(const ItemInstance& item) const {
    int rep = localRep();
    int price = item.value * (40 + rep) / 100;
    if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
        const RegionState& state = regionStates_[currentRegion_];
        if (state.flags.count("trade_boom")) price = price * 115 / 100;
        if (state.supply <= -2) price = price * 110 / 100;
        if (state.flags.count("worker_coop")) price = price * 108 / 100;
        if (state.flags.count("company_town")) price = price * 75 / 100;
    }
    return price < 1 ? 1 : price;
}

std::string Game::randomRumor() {
    if (!rumors_.empty() && runRng_.chance(65)) {
        const Rumor& value = rumors_[runRng_.range(0, (int)rumors_.size() - 1)];
        std::string where;
        if (value.region >= 0 && value.region < (int)world_.regions.size())
            where = " Heard near " + world_.regions[value.region].name + ".";
        return value.text + where;
    }
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

void Game::generateMystery() {
    mystery_ = Mystery{};
    if (history_.figures.size() < 2 || world_.sites.empty()) return;
    Rng plot(masterSeed_ ^ ((pendingLegacy_.size() + 1) * 0x9e3779b97f4a7c15ULL), 109);
    std::vector<int> alive, dead;
    for (int i = 0; i < (int)history_.figures.size(); i++) {
        if (history_.figures[i].died < 0) alive.push_back(i);
        else dead.push_back(i);
    }
    if (alive.empty()) return;
    mystery_.culprit = alive[plot.range(0, (int)alive.size() - 1)];
    mystery_.decoy = alive.size() > 1
        ? alive[(plot.range(0, (int)alive.size() - 2) + 1) % alive.size()]
        : mystery_.culprit;
    if (mystery_.decoy == mystery_.culprit && alive.size() > 1)
        mystery_.decoy = alive[(std::find(alive.begin(), alive.end(), mystery_.culprit) -
                                alive.begin() + 1) % alive.size()];
    if (!dead.empty()) mystery_.victim = dead[plot.range(0, (int)dead.size() - 1)];
    else {
        mystery_.victim = plot.range(0, (int)history_.figures.size() - 1);
        if (mystery_.victim == mystery_.culprit)
            mystery_.victim = (mystery_.victim + 1) % (int)history_.figures.size();
    }
    mystery_.site = plot.range(0, (int)world_.sites.size() - 1);
    if (!history_.artifacts.empty() && plot.chance(70))
        mystery_.artifact = plot.range(0, (int)history_.artifacts.size() - 1);
    const Figure& culprit = history_.figures[mystery_.culprit];
    const Figure& victim = history_.figures[mystery_.victim];
    std::string object = mystery_.artifact >= 0
        ? history_.artifacts[mystery_.artifact].display()
        : "a sealed page removed from the Chronicle";
    mystery_.title = "The " + victim.name + " Contradiction";
    mystery_.publicStory = victim.name + " vanished after carrying " + object +
                           " through " + world_.sites[mystery_.site].name + ".";
    mystery_.secret = culprit.name + ", the " + culprit.trait + " " +
                      culprit.profession + ", rewrote the record to conceal it.";
    mystery_.active = true;
}

void Game::generateSocialWeb() {
    socialTies_.clear();
    std::vector<int> alive;
    for (int i = 0; i < (int)history_.figures.size(); i++)
        if (history_.figures[i].died < 0) alive.push_back(i);
    if (alive.size() < 2) return;
    Rng social(masterSeed_ ^ ((pendingLegacy_.size() + 1) * 0xD1B54A32D192ED03ULL), 127);
    static const char* kinds[] = {
        "family", "rival", "employer", "protege", "debtor", "confidant"};
    int target = std::min((int)alive.size() * 2, 80);
    std::set<std::pair<int, int>> used;
    for (int tries = 0; (int)socialTies_.size() < target && tries < target * 8; tries++) {
        int a = alive[social.range(0, (int)alive.size() - 1)];
        int b = alive[social.range(0, (int)alive.size() - 1)];
        if (a == b) continue;
        if (a > b) std::swap(a, b);
        if (!used.insert({a, b}).second) continue;
        SocialTie tie;
        tie.a = a;
        tie.b = b;
        tie.kind = kinds[social.range(0, 5)];
        tie.affinity = social.range(-4, 4);
        if (tie.kind == "family" || tie.kind == "confidant" || tie.kind == "protege")
            tie.affinity = std::max(1, tie.affinity);
        if (tie.kind == "rival") tie.affinity = std::min(-1, tie.affinity);
        socialTies_.push_back(tie);
    }
}

std::string Game::agendaName(const std::string& kind) const {
    if (kind == "organize") return "building worker power";
    if (kind == "mutual_aid") return "expanding mutual aid";
    if (kind == "expose") return "documenting hidden harm";
    if (kind == "liberate") return "opening land and borders";
    if (kind == "profiteer") return "extracting emergency profit";
    if (kind == "pollute") return "externalizing industrial waste";
    if (kind == "militarize") return "turning fear into armed authority";
    if (kind == "displace") return "converting homes into assets";
    return "pursuing an undisclosed objective";
}

const Game::Agenda* Game::agendaFor(int figure) const {
    for (const Agenda& agenda : agendas_)
        if (agenda.active && agenda.figure == figure) return &agenda;
    return nullptr;
}

void Game::addRumor(const std::string& text, int truth, int region, int figure,
                    bool planted, const std::string& foreshadowEvent, int dueDay) {
    if (text.empty()) return;
    Rumor value;
    value.id = nextRumorId_++;
    value.text = text.substr(0, 240);
    value.truth = std::max(0, std::min(100, truth));
    value.origin = region;
    value.region = region;
    value.figure = figure;
    value.planted = planted;
    value.foreshadowEvent = foreshadowEvent;
    value.dueDay = dueDay;
    rumors_.push_back(value);
    while (rumors_.size() > 48) rumors_.erase(rumors_.begin());
}

void Game::seedLivingPolitics() {
    rumors_.clear();
    verifiedRumors_.clear();
    agendas_.clear();
    nextRumorId_ = 1;
    nemesisFigure_ = -1;
    collectiveVictories_ = 0;
    if (world_.regions.empty()) return;

    std::vector<int> alive;
    for (int i = 0; i < (int)history_.figures.size(); i++)
        if (history_.figures[i].died < 0) alive.push_back(i);
    static const char* kinds[] = {
        "organize", "mutual_aid", "expose", "liberate",
        "profiteer", "pollute", "militarize", "displace"
    };
    int count = std::min(16, (int)alive.size());
    for (int i = 0; i < count; i++) {
        int pick = runRng_.range(0, (int)alive.size() - 1);
        Agenda agenda;
        agenda.figure = alive[pick];
        alive.erase(alive.begin() + pick);
        agenda.region = runRng_.range(0, (int)world_.regions.size() - 1);
        agenda.kind = kinds[runRng_.range(0, 7)];
        agenda.progress = runRng_.range(0, 4);
        agendas_.push_back(agenda);
    }

    static const char* initial[] = {
        "Dockworkers have begun comparing pay slips. Management calls this theft of proprietary sadness.",
        "A housing fund is buying whole streets and promising to improve them by removing the residents.",
        "Someone has measured the river downstream from the refinery. The refinery has measured the someone.",
        "A free kitchen keeps appearing one block ahead of the officials assigned to close it."
    };
    for (int i = 0; i < 4; i++) {
        int region = runRng_.range(0, (int)world_.regions.size() - 1);
        addRumor(initial[i], 55 + i * 10, region, -1, i == 1);
    }
}

void Game::applyAgenda(Agenda& agenda) {
    if (!agenda.active || agenda.region < 0 ||
        agenda.region >= (int)regionStates_.size()) return;
    RegionState& state = regionStates_[agenda.region];
    int fi = agenda.figure;
    std::string who = fi >= 0 && fi < (int)history_.figures.size()
        ? history_.figures[fi].name : "Someone with excellent stationery";
    std::string report;

    if (agenda.kind == "organize") {
        state.solidarity++;
        state.unrest++;
        if (state.solidarity >= 3) state.flags.insert("tenants_union");
        report = who + " helped workers form a council before anyone could appoint its chair.";
    } else if (agenda.kind == "mutual_aid") {
        state.supply++;
        state.prosperity++;
        state.solidarity++;
        if (state.solidarity >= 2) state.flags.insert("public_clinic");
        report = who + " expanded a mutual-aid route. Three neighborhoods now share one impossible pantry.";
    } else if (agenda.kind == "expose") {
        state.pollution--;
        state.solidarity++;
        report = who + " published records that were confidential only because they were damning.";
    } else if (agenda.kind == "liberate") {
        state.rent--;
        state.solidarity++;
        if (state.solidarity >= 3) state.flags.insert("commons");
        report = who + " opened fenced land and discovered the fence had no argument without guards.";
    } else if (agenda.kind == "profiteer") {
        state.rent++;
        state.supply--;
        state.flags.insert("company_town");
        report = who + " acquired the local shortage and began charging it rent.";
    } else if (agenda.kind == "pollute") {
        state.pollution++;
        state.prosperity++;
        report = who + " moved industrial waste off the balance sheet and into the drinking water.";
    } else if (agenda.kind == "militarize") {
        state.danger++;
        state.unrest++;
        state.flags.insert("militarized");
        report = who + " answered public fear with uniforms, checkpoints, and a private invoice.";
    } else if (agenda.kind == "displace") {
        state.rent++;
        state.pressure++;
        state.flags.insert("evictions");
        report = who + " converted occupied homes into vacant investment opportunities.";
    }
    state.prosperity = std::max(-5, std::min(5, state.prosperity));
    state.danger = std::max(-5, std::min(5, state.danger));
    state.unrest = std::max(-5, std::min(5, state.unrest));
    state.pressure = std::max(-5, std::min(5, state.pressure));
    state.supply = std::max(-5, std::min(5, state.supply));
    state.rent = std::max(-5, std::min(5, state.rent));
    state.pollution = std::max(-5, std::min(5, state.pollution));
    state.solidarity = std::max(-5, std::min(5, state.solidarity));
    addRumor(report, 78, agenda.region, fi, false);

    ChronEntry entry;
    entry.id = (int)history_.chron.size();
    entry.year = history_.presentYear;
    entry.type = "region_shift";
    entry.actor = fi;
    entry.extra = report;
    history_.chron.push_back(entry);
    if (agenda.region == currentRegion_) newsLine_ = report;
}

void Game::advanceRumorsAndAgendas() {
    if (world_.regions.empty()) return;
    for (Rumor& rumor : rumors_) {
        rumor.age++;
        if (rumor.region >= 0 && rumor.region < (int)world_.regions.size() &&
            !world_.regions[rumor.region].neighbors.empty() && liveRng_.chance(22)) {
            const auto& neighbors = world_.regions[rumor.region].neighbors;
            rumor.region = neighbors[liveRng_.range(0, (int)neighbors.size() - 1)];
            rumor.reach++;
            int drift = liveRng_.range(-7, 5);
            rumor.truth = std::max(0, std::min(100, rumor.truth + drift));
        }
        if (!rumor.foreshadowEvent.empty() && rumor.dueDay == ch_.day) {
            newsLine_ = "THE WARNING COMES DUE: " + rumor.text;
            audio_.warning();
        }
    }

    for (Agenda& agenda : agendas_) {
        if (!agenda.active) continue;
        agenda.progress += liveRng_.chance(35) ? 2 : 1;
        if (agenda.progress >= 7) {
            applyAgenda(agenda);
            agenda.progress = liveRng_.range(0, 2);
        }
    }

    int strongest = -1;
    for (const auto& pair : npcRelations_)
        if (pair.second.grudge >= 6 &&
            (strongest < 0 || pair.second.grudge > npcRelations_.at(strongest).grudge))
            strongest = pair.first;
    if (strongest >= 0) nemesisFigure_ = strongest;
    if (nemesisFigure_ >= 0 && nemesisFigure_ < (int)history_.figures.size() &&
        ch_.day % 8 == 0) {
        RegionState& state = regionStates_[currentRegion_];
        state.unrest = std::min(5, state.unrest + 1);
        state.flags.insert("blacklisted");
        std::string warning = history_.figures[nemesisFigure_].name +
            " has been telling employers, guards, and one unusually political horse about you.";
        addRumor(warning, 92, currentRegion_, nemesisFigure_, true);
        newsLine_ = warning;
    }

    if (ch_.day % 4 == 0) {
        const RegionState& state = regionStates_[currentRegion_];
        std::string text;
        int truth = 70;
        if (state.pollution >= 3)
            text = "Children are drawing the refinery smoke with colors the refinery says do not exist.";
        else if (state.rent >= 3)
            text = "The rent rose again. The building has not, although the landlord's confidence has.";
        else if (state.supply <= -2)
            text = "Merchants deny a shortage while standing in front of several locked warehouses.";
        else if (state.solidarity >= 3)
            text = "A neighborhood assembly solved in an afternoon what officials had studied for eleven years.";
        if (!text.empty()) addRumor(text, truth, currentRegion_);
    }
}

void Game::queueConsequence(int days, const std::string& eventId,
                            const std::string& summary) {
    if (eventId.empty()) return;
    PendingConsequence value;
    value.eventId = eventId;
    value.source = current_ ? current_->id : "world";
    value.summary = summary;
    value.dueDay = ch_.day + std::max(1, days);
    value.figure = slotFigure_;
    value.region = currentRegion_;
    consequences_.push_back(value);
}

void Game::activateDueConsequence() {
    if (!scheduledNextId_.empty()) return;
    int due = -1;
    int reserveWindow = eventSerial_ > 0 && eventSerial_ % 3 == 0 ? 1 : 0;
    for (int i = 0; i < (int)consequences_.size(); i++)
        if (consequences_[i].dueDay <= ch_.day + reserveWindow &&
            (due < 0 || consequences_[i].dueDay < consequences_[due].dueDay))
            due = i;
    if (due < 0) return;
    scheduledNextId_ = consequences_[due].eventId;
    scheduledFigure_ = consequences_[due].figure;
    scheduledRegion_ = consequences_[due].region;
    if (!consequences_[due].summary.empty())
        newsLine_ = "AN OLD CHOICE RETURNS: " + consequences_[due].summary;
    consequences_.erase(consequences_.begin() + due);
}

void Game::resolveStaleConsequences() {
    // A story can continue without the player, but only after a generous
    // window. This prevents four simultaneous arcs from clogging the road.
    for (int i = 0; i < (int)consequences_.size(); i++) {
        PendingConsequence& value = consequences_[i];
        if (value.dueDay + 12 > ch_.day) continue;
        const Event* event = deck_.find(value.eventId);
        std::string family = event ? event->family : std::string();
        int region = value.region >= 0 ? value.region : currentRegion_;
        if (region >= 0 && region < (int)regionStates_.size()) {
            regionStates_[region].unrest = std::min(5, regionStates_[region].unrest + 1);
            regionStates_[region].flags.insert("story_moved_on");
        }
        if (!family.empty()) {
            storyEchoes_.insert(family);
            storyEchoRegions_[family] = region;
        }
        ChronEntry entry;
        entry.id = (int)history_.chron.size();
        entry.year = history_.presentYear;
        entry.type = "region_shift";
        entry.site = currentSite_;
        entry.extra = "While " + ch_.name.conlang + " was elsewhere, " +
                      (value.summary.empty() ? value.source : value.summary) +
                      " resolved without permission.";
        history_.chron.push_back(entry);
        newsLine_ = entry.extra;
        autonomousArcResolutions_++;
        consequences_.erase(consequences_.begin() + i);
        return;
    }
}

void Game::convergeStory(const std::string& family) {
    if (family.empty()) return;
    storyEchoes_.insert(family);
    storyEchoRegions_[family] = currentRegion_;
    RegionState* region = currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()
        ? &regionStates_[currentRegion_] : nullptr;
    if (region) {
        if (family == "refugee_bells") {
            region->flags.insert("refugee_quarter");
            region->prosperity = std::min(5, region->prosperity + 1);
        } else if (family == "contagious_trade") {
            region->flags.insert("trade_boom");
            region->danger = std::min(5, region->danger + 1);
        } else if (family == "false_saint") {
            region->flags.insert("faith_divided");
            region->unrest = std::min(5, region->unrest + 1);
        } else if (family == "guild_schism") {
            region->flags.insert("guild_reformed");
            region->prosperity = std::min(5, region->prosperity + 1);
        } else if (family == "walking_town") {
            region->flags.insert("walking_citizens");
            region->pressure = std::min(5, region->pressure + 2);
        } else if (family == "crooked_inheritance") {
            region->flags.insert("disputed_estate");
        }
    }

    int known = slotFigure_;
    if (known < 0 && !npcRelations_.empty()) known = npcRelations_.begin()->first;
    if (known >= 0 && known < (int)history_.figures.size()) {
        NpcRelation& rel = relation(known);
        rel.knowledge = std::min(10, rel.knowledge + 1);
        npcMarks_[known].insert("story_witness");
    }

    // Artifacts keep moving through the social and regional systems. A
    // completed arc can relocate one unclaimed object to a neighboring site.
    std::vector<int> movable;
    for (int i = 0; i < (int)history_.artifacts.size(); i++)
        if (!history_.artifacts[i].claimed) movable.push_back(i);
    if (!movable.empty() && !world_.sites.empty()) {
        int artifactPick = runRng_.range(0, (int)movable.size() - 1);
        int sitePick = runRng_.range(0, (int)world_.sites.size() - 1);
        history_.artifacts[movable[artifactPick]].restingSite = sitePick;
    }

    ChronEntry entry;
    entry.id = (int)history_.chron.size();
    entry.year = history_.presentYear;
    entry.type = "region_shift";
    entry.actor = known;
    entry.site = currentSite_;
    entry.extra = "The consequences of " + family +
                  " escaped their ending and entered ordinary life.";
    history_.chron.push_back(entry);
    newsLine_ = entry.extra;
}

void Game::updateRegionState() {
    if (regionStates_.size() != world_.regions.size())
        regionStates_.assign(world_.regions.size(), RegionState{});
    std::vector<int> exportedDanger(regionStates_.size(), 0);
    std::vector<int> exportedProsperity(regionStates_.size(), 0);
    if (ch_.day % 7 == 0) {
        for (int r = 0; r < (int)regionStates_.size(); r++) {
            for (int neighbor : world_.regions[r].neighbors) {
                if (neighbor < 0 || neighbor >= (int)regionStates_.size()) continue;
                if (regionStates_[neighbor].danger >= 3 ||
                    regionStates_[neighbor].unrest >= 3 ||
                    history_.plaguedRegions.count(neighbor)) exportedDanger[r]++;
                if (regionStates_[neighbor].prosperity >= 3) exportedProsperity[r]++;
            }
        }
    }
    for (int r = 0; r < (int)regionStates_.size(); r++) {
        RegionState& state = regionStates_[r];
        std::string before = state.description();
        bool plagued = history_.plaguedRegions.count(r) > 0;
        bool war = false;
        int owner = regionOwner(r);
        for (const LiveWar& live : history_.liveWars)
            if (live.a == owner || live.b == owner) war = true;
        bool beast = false;
        for (const Beast& value : history_.beasts)
            if (value.died < 0 && value.region == r) beast = true;

        if (ch_.day % 7 == 0) {
            if (plagued) {
                state.prosperity--; state.danger++; state.unrest++;
                state.supply--;
            }
            if (war) {
                state.prosperity--; state.danger++; state.unrest += 2;
                state.supply--; state.pollution++;
            }
            if (beast) state.danger++;
            if (!plagued && !war && !beast) {
                if ((ch_.day / 7 + r) % 2 == 0) state.prosperity++;
                if (state.danger > 0) state.danger--;
                if (state.unrest > 0) state.unrest--;
            }
            state.pressure += exportedDanger[r] - exportedProsperity[r];
            if (exportedDanger[r] > 0) {
                state.danger++;
                if (state.pressure >= 2) state.unrest++;
                state.flags.insert("spillover");
            } else {
                state.flags.erase("spillover");
            }
            if (exportedProsperity[r] > exportedDanger[r]) state.prosperity++;
            if (state.pressure >= 3) state.flags.insert("refugees");
            else state.flags.erase("refugees");
            if (state.prosperity >= 3 && exportedProsperity[r] > 0)
                state.flags.insert("trade_boom");
            else state.flags.erase("trade_boom");
            if (state.flags.count("trade_boom") && !state.flags.count("tenants_union"))
                state.rent++;
            if (state.solidarity >= 2) {
                if (state.rent > -3) state.rent--;
                if (state.supply < 3) state.supply++;
            }
            if (state.flags.count("clean_power") && state.pollution > -3)
                state.pollution--;
            if (state.pollution >= 3) {
                state.danger++;
                state.unrest++;
            }
        }
        state.prosperity = std::max(-5, std::min(5, state.prosperity));
        state.danger = std::max(-5, std::min(5, state.danger));
        state.unrest = std::max(-5, std::min(5, state.unrest));
        state.pressure = std::max(-5, std::min(5, state.pressure));
        state.supply = std::max(-5, std::min(5, state.supply));
        state.rent = std::max(-5, std::min(5, state.rent));
        state.pollution = std::max(-5, std::min(5, state.pollution));
        state.solidarity = std::max(-5, std::min(5, state.solidarity));
        if (plagued) state.flags.insert("plagued"); else state.flags.erase("plagued");
        if (war) state.flags.insert("at_war"); else state.flags.erase("at_war");
        if (state.danger >= 4 && !beast) state.flags.insert("haunted");
        if (state.prosperity >= 3) state.flags.insert("thriving");
        else state.flags.erase("thriving");
        if (state.prosperity <= -3) state.flags.insert("impoverished");
        else state.flags.erase("impoverished");
        if (state.danger >= 3) state.flags.insert("perilous");
        else state.flags.erase("perilous");
        if (state.unrest >= 3) state.flags.insert("volatile");
        else state.flags.erase("volatile");
        if (state.pollution >= 3) state.flags.insert("toxic_zone");
        else if (state.pollution <= 0) state.flags.erase("toxic_zone");
        if (state.rent >= 3) state.flags.insert("evictions");
        else if (state.rent <= 0) state.flags.erase("evictions");
        if (state.solidarity >= 4) state.flags.insert("worker_coop");
        if (state.solidarity >= 3 && state.supply >= 1) state.flags.insert("public_clinic");
        if (state.solidarity >= 4 && state.rent <= 0) state.flags.insert("commons");

        std::string after = state.description();
        if (r == currentRegion_ && ch_.day % 7 == 0 && before != after) {
            ChronEntry entry;
            entry.id = (int)history_.chron.size();
            entry.year = history_.presentYear;
            entry.type = "region_shift";
            entry.extra = world_.regions[r].name + " became " + after +
                          " while the roads were busy elsewhere.";
            history_.chron.push_back(entry);
            newsLine_ = entry.extra;
        }
    }
}

// One day passes: the world does not wait for you.
void Game::dailyTick() {
    ch_.day++;
    size_t before = history_.chron.size();
    SimulateLiveDay(world_, history_, liveRng_, grammar_, forge_);
    // Old worlds are restless worlds: extra live history per generation.
    if (worldGen_ >= 2 && liveRng_.chance(worldGen_ * 6))
        SimulateLiveDay(world_, history_, liveRng_, grammar_, forge_);
    if (history_.chron.size() > before) {
        newsLine_ = RenderChronEntry(history_.chron.back(), history_, world_,
                                     grammar_, liveRng_);
        audio_.chime();
    }
    // The road builds trust one shared day at a time (R10). A devoted
    // companion leans in: their passive sharpens.
    if (comp_.active) {
        comp_.daysTogether++;
        if (comp_.daysTogether == 10) {
            // "check dex +1" -> "check dex +2", once, at devotion.
            std::istringstream ps(comp_.passive);
            std::string verb, which; int v = 0;
            ps >> verb >> which >> v;
            if (verb == "check" && v > 0) {
                comp_.passive = "check " + which + " +" + std::to_string(v + 1);
                ch_.companionPassive = comp_.passive;
            }
            newsLine_ = comp_.name + " has stopped keeping one eye on the exits. " +
                        "You two are a unit now.";
            audio_.chime();
        }
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
    updateRegionState();
    advanceRumorsAndAgendas();
    if (ch_.day % 7 == 0 && currentRegion_ >= 0 &&
        currentRegion_ < (int)regionStates_.size() &&
        regionStates_[currentRegion_].unrest >= 3) {
        for (const SocialTie& tie : socialTies_) {
            if (tie.affinity > -3) continue;
            if (npcRelations_.count(tie.a)) {
                NpcRelation& rel = relation(tie.a);
                rel.grudge = std::min(10, rel.grudge + 1);
            }
            if (npcRelations_.count(tie.b)) {
                NpcRelation& rel = relation(tie.b);
                rel.grudge = std::min(10, rel.grudge + 1);
            }
        }
    }
    resolveStaleConsequences();
    activateDueConsequence();
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
        } else if (query == "rumor_active") {
            if (rumors_.empty()) return false;
            std::vector<int> local;
            for (int i = 0; i < (int)rumors_.size(); i++)
                if (rumors_[i].region == currentRegion_) local.push_back(i);
            int ri = local.empty()
                ? runRng_.range(0, (int)rumors_.size() - 1)
                : local[runRng_.range(0, (int)local.size() - 1)];
            const Rumor& rumor = rumors_[ri];
            ctx[name] = rumor.text;
            ctx[name + "_age"] = std::to_string(rumor.age);
            ctx[name + "_reach"] = std::to_string(rumor.reach);
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
            std::vector<int> familiar;
            for (int fi : pool)
                if (npcRelations_.count(fi)) familiar.push_back(fi);
            int fi = -1;
            bool recur = !familiar.empty() && runRng_.chance(65);
            if (recur) fi = familiar[runRng_.range(0, (int)familiar.size() - 1)];
            else fi = pool[runRng_.range(0, (int)pool.size() - 1)];
            slotFigure_ = fi;
            const Figure& f = history_.figures[fi];
            NpcRelation& rel = relation(fi);
            ctx[name] = f.name;
            ctx[name + "_trait"] = f.trait;
            ctx[name + "_prof"] = f.profession;
            ctx[name + "_trust"] = std::to_string(rel.trust);
            ctx[name + "_fear"] = std::to_string(rel.fear);
            ctx[name + "_respect"] = std::to_string(rel.respect);
            ctx[name + "_debt"] = std::to_string(rel.debt);
            ctx[name + "_grudge"] = std::to_string(rel.grudge);
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
        } else if (query == "remembered_figure") {
            int found = scheduledFigure_;
            if (found < 0 || found >= (int)history_.figures.size() ||
                history_.figures[found].died >= 0) {
                int best = -1, bestScore = -1000;
                for (auto& pair : npcRelations_) {
                    int fi = pair.first;
                    if (fi < 0 || fi >= (int)history_.figures.size() ||
                        history_.figures[fi].died >= 0) continue;
                    const NpcRelation& rel = pair.second;
                    int score = std::abs(rel.trust) + std::abs(rel.fear) +
                                std::abs(rel.respect) + std::abs(rel.grudge) + rel.knowledge;
                    if (score > bestScore) { best = fi; bestScore = score; }
                }
                found = best;
            }
            if (found < 0) return false;
            slotFigure_ = found;
            const Figure& f = history_.figures[found];
            NpcRelation& rel = relation(found);
            ctx[name] = f.name;
            ctx[name + "_trait"] = f.trait;
            ctx[name + "_prof"] = f.profession;
            ctx[name + "_trust"] = std::to_string(rel.trust);
            ctx[name + "_fear"] = std::to_string(rel.fear);
            ctx[name + "_respect"] = std::to_string(rel.respect);
            ctx[name + "_debt"] = std::to_string(rel.debt);
            ctx[name + "_grudge"] = std::to_string(rel.grudge);
        } else if (query == "social_pair") {
            if (socialTies_.empty()) return false;
            std::vector<int> familiar;
            for (int i = 0; i < (int)socialTies_.size(); i++) {
                bool scheduled = scheduledFigure_ >= 0 &&
                    (socialTies_[i].a == scheduledFigure_ ||
                     socialTies_[i].b == scheduledFigure_);
                bool known = npcRelations_.count(socialTies_[i].a) ||
                             npcRelations_.count(socialTies_[i].b);
                if (scheduled || (scheduledFigure_ < 0 && known)) familiar.push_back(i);
            }
            int ti = -1;
            if (!familiar.empty() &&
                (scheduledFigure_ >= 0 || runRng_.chance(70)))
                ti = familiar[runRng_.range(0, (int)familiar.size() - 1)];
            else
                ti = runRng_.range(0, (int)socialTies_.size() - 1);
            const SocialTie& tie = socialTies_[ti];
            slotFigure_ = scheduledFigure_ == tie.b ? tie.b : tie.a;
            slotSocialOther_ = slotFigure_ == tie.a ? tie.b : tie.a;
            ctx[name] = history_.figures[slotFigure_].name;
            ctx[name + "_other"] = history_.figures[slotSocialOther_].name;
            ctx[name + "_kind"] = tie.kind;
            ctx[name + "_affinity"] = std::to_string(tie.affinity);
        } else if (query == "mystery_suspect") {
            if (!mystery_.active || mystery_.solved || mystery_.culprit < 0) return false;
            if (scheduledFigure_ == mystery_.culprit || scheduledFigure_ == mystery_.decoy)
                slotSuspect_ = scheduledFigure_;
            else
                slotSuspect_ = runRng_.chance(50) ? mystery_.culprit : mystery_.decoy;
            if (slotSuspect_ < 0 || slotSuspect_ >= (int)history_.figures.size()) return false;
            slotFigure_ = slotSuspect_;
            const Figure& suspect = history_.figures[slotSuspect_];
            ctx[name] = suspect.name;
            ctx[name + "_trait"] = suspect.trait;
            ctx[name + "_prof"] = suspect.profession;
            ctx[name + "_case"] = mystery_.title;
        } else if (query == "mystery_clue") {
            if (!mystery_.active || mystery_.solved || mystery_.culprit < 0 ||
                mystery_.culprit >= (int)history_.figures.size()) return false;
            const Figure& culprit = history_.figures[mystery_.culprit];
            if (mystery_.clues <= 0)
                ctx[name] = "The altered ink was mixed by someone trained as a " +
                            culprit.profession + ".";
            else if (mystery_.clues == 1)
                ctx[name] = "A witness remembers the culprit as " + culprit.trait +
                            ", and remembers being paid to forget.";
            else
                ctx[name] = "The hidden signature spells " + culprit.name +
                            ". At last, the lie has a name.";
            ctx[name + "_case"] = mystery_.title;
            ctx[name + "_site"] = world_.sites[mystery_.site].name;
        } else if (query == "mystery_culprit") {
            if (!mystery_.active || mystery_.solved || mystery_.culprit < 0 ||
                mystery_.culprit >= (int)history_.figures.size()) return false;
            slotFigure_ = mystery_.culprit;
            const Figure& culprit = history_.figures[mystery_.culprit];
            ctx[name] = culprit.name;
            ctx[name + "_trait"] = culprit.trait;
            ctx[name + "_prof"] = culprit.profession;
            ctx[name + "_case"] = mystery_.title;
        } else {
            return false; // unknown query: skip the event, loudly absent
        }
    }
    return true;
}

bool Game::presentEvent(const Event* event, bool markUsed) {
    if (!event) return false;
    current_ = event;
    Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
    pendingArtifact_ = -1;
    slotFigure_ = -1;
    slotBeast_ = -1;
    slotGod_ = -1;
    slotSocialOther_ = -1;
    slotSuspect_ = -1;
    if (!resolveSlots(*event, ctx)) return false;
    for (const std::string& condition : event->when)
        if (!evalCond(condition)) return false;

    eventSerial_++;
    currentDirectorScore_ = director_.score(*event, storyContext(deckTag_));

    if (markUsed) deck_.markUsed(event->id);
    currentCtx_ = ctx;
    currentText_ = grammar_.expand(event->text, runRng_, ctx);
    choiceTexts_.clear();
    for (const Choice& choice : event->choices)
        choiceTexts_.push_back(choice.requires_.label() +
                               grammar_.expand(choice.text, runRng_, ctx));
    reveal_ = 0.0f;
    textScroll_ = 0;
    for (const std::string& location : event->locations)
        if (location == "dungeon_finale") finalesSeen_++;

    compLine_.clear();
    if (comp_.active && runRng_.chance(35)) {
        std::string line = grammar_.expand("{comp_" + comp_.trait + "}", runRng_,
                                           {{"comp", comp_.name}});
        compLine_ = comp_.name + ": \"" + line + "\"";
    }
    screen_ = EVENT;
    return true;
}

void Game::dealEvent() {
    audio_.playMusicFor(deckTag_, masterSeed_);
    if (!scheduledNextId_.empty()) {
        const Event* scheduled = deck_.find(scheduledNextId_);
        scheduledNextId_.clear();
        if (presentEvent(scheduled, false)) return;
        scheduledFigure_ = -1;
        scheduledRegion_ = -1;
    }
    // Dungeons escalate: the last event of a visit draws from the finale deck.
    std::string tag = deckTag_;
    if ((deckTag_ == "dungeon" || deckTag_ == "crash") && eventsLeftHere_ == 1)
        tag = "dungeon_finale";
    // Ineligible draws (failed gates/slots) return to the pool unseen; the
    // tried-set stops this loop from redrawing them within one deal (R9).
    std::set<std::string> tried;
    StoryContext context = storyContext(tag);
    for (int attempt = 0; attempt < 12; attempt++) {
        current_ = deck_.draw(runRng_, tag, &tried,
                              [&](const Event& event) {
                                  return director_.score(event, context);
                              });
        if (!current_ && tag == "dungeon_finale") { tag = "dungeon"; continue; }
        if (!current_ && tag == "crash") { tag = "dungeon"; continue; }
        // Biome decks are smaller; when one runs dry the land defaults.
        if (!current_ && tag == "swamp") { tag = "forest"; continue; }
        if (!current_ && (tag == "mountains" || tag == "coast")) { tag = "road"; continue; }
        if (!current_ && tag == "sea") { tag = "coast"; continue; }
        if (!current_) { enterTravel(); return; }
        tried.insert(current_->id);
        if (presentEvent(current_)) return;
    }
    enterTravel();
}

// Drops the least valuable non-artifact item; narrates it on the card.
bool Game::dropCheapest(const std::string& why) {
    int worst = -1, worstVal = 1 << 20;
    for (int i = 0; i < (int)ch_.pack.size(); i++) {
        if (ch_.pack[i].artifactId >= 0) continue; // never auto-drop relics
        if (ch_.pack[i].value < worstVal) { worstVal = ch_.pack[i].value; worst = i; }
    }
    if (worst < 0) return false;
    outcome_.text += "\n(You leave behind " + ch_.pack[worst].name + " " + why + ".)";
    ch_.pack.erase(ch_.pack.begin() + worst);
    return true;
}

void Game::takeItem(const ItemInstance& item) {
    if ((int)ch_.pack.size() >= ch_.capacity()) {
        // Swap only if the find beats the worst thing you carry.
        int worst = -1, worstVal = 1 << 20;
        for (int i = 0; i < (int)ch_.pack.size(); i++) {
            if (ch_.pack[i].artifactId >= 0) continue;
            if (ch_.pack[i].value < worstVal) { worstVal = ch_.pack[i].value; worst = i; }
        }
        if (worst < 0 || item.value <= worstVal) {
            outcome_.text += "\n(Your pack is full; " + item.name + " stays behind.)";
            return;
        }
        outcome_.text += "\n(Pack full: you swap " + ch_.pack[worst].name + " for " +
                         item.name + ".)";
        ch_.pack.erase(ch_.pack.begin() + worst);
    }
    ch_.pack.push_back(item);
    audio_.chime();
}

void Game::applyEffects(const std::vector<std::string>& effects) {
    for (auto& fx : effects) {
        std::istringstream ss(fx);
        std::string verb;
        ss >> verb;
        if (verb == "hp") {
            int v = 0; ss >> v;
            if (v < 0) {
                // The ratchet: a world that has taken lives of yours hits
                // harder every generation (R7).
                v -= worldGen_ / 3;
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
            // ...and pays better, too. Risk and reward age together (R7).
            if (v > 0) {
                v += worldGen_ / 3;
                audio_.coin();
            }
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
            takeItem(makeItem(id));
        } else if (verb == "loot") {
            std::string tier; ss >> tier;
            ItemInstance item = items_.loot(runRng_, tier);
            bindQuirk(item);
            takeItem(item);
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
        } else if (verb == "schedule") {
            int days = 1;
            std::string eventId, summary;
            ss >> days >> eventId;
            std::getline(ss, summary);
            if (!summary.empty() && summary[0] == ' ') summary.erase(0, 1);
            queueConsequence(days, eventId, summary);
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
                beastSlainThisRun_ = true;
                profile_.beastsSlain++;
                SaveProfile(profile_);
#if defined(__EMSCRIPTEN__)
                // Shared world? The other players hear about this.
                int bk = boardKeyFor(masterSeed_);
                if (bk >= 0) {
                    nlohmann::json d = {{"day", bk},
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
            // A full pack makes room for a true artifact: the cheapest thing
            // you carry gets left behind, and the card says so (R10).
            if (pendingArtifact_ >= 0 && (int)ch_.pack.size() >= ch_.capacity())
                dropCheapest("to make room for what you found");
            if (pendingArtifact_ >= 0 && (int)ch_.pack.size() < ch_.capacity()) {
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
        } else if (verb == "npc_rel") {
            std::string field;
            int amount = 0;
            ss >> field >> amount;
            if (slotFigure_ >= 0) {
                NpcRelation& rel = relation(slotFigure_);
                int* value = nullptr;
                if (field == "trust") value = &rel.trust;
                else if (field == "fear") value = &rel.fear;
                else if (field == "respect") value = &rel.respect;
                else if (field == "debt") value = &rel.debt;
                else if (field == "affection") value = &rel.affection;
                else if (field == "grudge") value = &rel.grudge;
                else if (field == "knowledge") value = &rel.knowledge;
                if (value) *value = std::max(-10, std::min(10, *value + amount));
            }
        } else if (verb == "npc_know") {
            int amount = 1;
            ss >> amount;
            if (slotFigure_ >= 0) {
                NpcRelation& rel = relation(slotFigure_);
                rel.knowledge = std::max(0, std::min(10, rel.knowledge + amount));
            }
        } else if (verb == "network") {
            int amount = 0;
            ss >> amount;
            for (SocialTie& tie : socialTies_)
                if ((tie.a == slotFigure_ && tie.b == slotSocialOther_) ||
                    (tie.b == slotFigure_ && tie.a == slotSocialOther_)) {
                    tie.affinity = std::max(-10, std::min(10, tie.affinity + amount));
                    break;
                }
        } else if (verb == "network_rel") {
            std::string field;
            int amount = 0;
            ss >> field >> amount;
            if (slotSocialOther_ >= 0) {
                NpcRelation& rel = relation(slotSocialOther_);
                int* value = nullptr;
                if (field == "trust") value = &rel.trust;
                else if (field == "fear") value = &rel.fear;
                else if (field == "respect") value = &rel.respect;
                else if (field == "debt") value = &rel.debt;
                else if (field == "affection") value = &rel.affection;
                else if (field == "grudge") value = &rel.grudge;
                if (value) *value = std::max(-10, std::min(10, *value + amount));
            }
        } else if (verb == "rumor") {
            int truth = 50;
            ss >> truth;
            std::string text;
            std::getline(ss, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);
            addRumor(text, truth, currentRegion_, slotFigure_, truth < 35);
        } else if (verb == "foreshadow") {
            int days = 1;
            std::string eventId;
            ss >> days >> eventId;
            std::string text;
            std::getline(ss, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);
            queueConsequence(days, eventId, text);
            addRumor(text, 72, currentRegion_, slotFigure_, false,
                     eventId, ch_.day + std::max(1, days));
        } else if (verb == "agenda") {
            std::string kind;
            ss >> kind;
            if (slotFigure_ >= 0 && !kind.empty()) {
                bool found = false;
                for (Agenda& agenda : agendas_)
                    if (agenda.figure == slotFigure_) {
                        agenda.kind = kind;
                        agenda.region = currentRegion_;
                        agenda.progress = 0;
                        agenda.active = true;
                        found = true;
                    }
                if (!found) agendas_.push_back({slotFigure_, currentRegion_, 0, kind, true});
            }
        } else if (verb == "nemesis") {
            if (slotFigure_ >= 0) {
                nemesisFigure_ = slotFigure_;
                relation(slotFigure_).grudge = std::max(6, relation(slotFigure_).grudge);
                npcMarks_[slotFigure_].insert("nemesis");
            }
        } else if (verb == "collective") {
            int amount = 1;
            ss >> amount;
            collectiveVictories_ = std::max(0, collectiveVictories_ + amount);
            if (amount > 0) audio_.solidarity();
        } else if (verb == "converge") {
            std::string family;
            ss >> family;
            convergeStory(family);
        } else if (verb == "npc_unmark") {
            // A grudge, formally retired. The family will find a new hobby.
            std::string mark; ss >> mark;
            if (slotFigure_ >= 0) {
                npcMarks_[slotFigure_].erase(mark);
                npcMarks_[slotFigure_].insert("settled");
                settledThisRun_ = true;
                profile_.vendettas++;
                SaveProfile(profile_);
            }
        } else if (verb == "learn") {
            // Words of the old tongue, kept across every run and every world.
            int n = 0; ss >> n;
            profile_.wordsLearned += n;
            wordsThisRun_ += n;
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
        } else if (verb == "clue" || verb == "mystery_clue") {
            int amount = 1;
            ss >> amount;
            if (mystery_.active && !mystery_.solved) {
                int before = mystery_.clues;
                mystery_.clues = std::max(0, std::min(3, mystery_.clues + amount));
                if (before < 3 && mystery_.clues >= 3)
                    queueConsequence(1, "r12_mystery_reckoning",
                                     "The final signature has been authenticated.");
                else if (before == 0 && mystery_.clues >= 1)
                    queueConsequence(2, "r12_mystery_second",
                                     "A witness has reconsidered their silence.");
                else if (before == 1 && mystery_.clues >= 2)
                    queueConsequence(2, "r12_mystery_second",
                                     "A second copy of the altered record has surfaced.");
            }
        } else if (verb == "evidence") {
            int amount = 0; ss >> amount;
            mystery_.evidence = std::max(0, std::min(9, mystery_.evidence + amount));
        } else if (verb == "doubt") {
            int amount = 0; ss >> amount;
            mystery_.doubt = std::max(0, std::min(9, mystery_.doubt + amount));
        } else if (verb == "mystery_accuse") {
            std::string who; ss >> who;
            if (who == "culprit") mystery_.accused = mystery_.culprit;
            else if (who == "decoy") mystery_.accused = mystery_.decoy;
            else mystery_.accused = slotSuspect_ >= 0 ? slotSuspect_ : slotFigure_;
        } else if (verb == "mystery_trial") {
            if (mystery_.active && !mystery_.tried && mystery_.accused >= 0) {
                mystery_.tried = true;
                mystery_.correctVerdict = mystery_.accused == mystery_.culprit &&
                    mystery_.evidence >= 2 && mystery_.evidence > mystery_.doubt;
                mystery_.solved = mystery_.correctVerdict;
                if (mystery_.accused >= 0) {
                    if (mystery_.correctVerdict) {
                        npcMarks_[mystery_.accused].insert("convicted");
                        relation(mystery_.accused).fear =
                            std::min(10, relation(mystery_.accused).fear + 2);
                    } else {
                        npcMarks_[mystery_.accused].insert("wrongly_accused");
                        NpcRelation& wronged = relation(mystery_.accused);
                        wronged.grudge = std::min(10, wronged.grudge + 5);
                        wronged.trust = std::max(-10, wronged.trust - 4);
                        queueConsequence(5, "r14_wrong_verdict_returns",
                                         "The person you accused has found allies.");
                    }
                }
                ChronEntry entry;
                entry.id = (int)history_.chron.size();
                entry.year = history_.presentYear;
                entry.type = "pc_mystery";
                entry.actor = mystery_.accused;
                entry.site = mystery_.site;
                entry.extra = mystery_.correctVerdict
                    ? ch_.name.conlang + " proved " + mystery_.title
                    : ch_.name.conlang + " reached the wrong verdict in " + mystery_.title;
                history_.chron.push_back(entry);
                newsLine_ = mystery_.correctVerdict
                    ? "The verdict holds. " + mystery_.secret
                    : "The verdict collapses. The real story remains loose.";
                if (mystery_.correctVerdict) audio_.fanfare(); else audio_.thud();
            }
        } else if (verb == "mystery_solve") {
            if (mystery_.active && !mystery_.solved) {
                mystery_.solved = true;
                ChronEntry entry;
                entry.id = (int)history_.chron.size();
                entry.year = history_.presentYear;
                entry.type = "pc_mystery";
                entry.actor = mystery_.culprit;
                entry.site = mystery_.site;
                entry.artifact = mystery_.artifact;
                entry.extra = ch_.name.conlang + " exposed " + mystery_.title;
                history_.chron.push_back(entry);
                newsLine_ = mystery_.title + " is solved. " + mystery_.secret;
                audio_.fanfare();
            }
        } else if (verb == "region") {
            std::string field;
            int amount = 0;
            ss >> field >> amount;
            if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
                RegionState& state = regionStates_[currentRegion_];
                int* value = nullptr;
                if (field == "prosperity") value = &state.prosperity;
                else if (field == "danger") value = &state.danger;
                else if (field == "unrest") value = &state.unrest;
                else if (field == "pressure") value = &state.pressure;
                else if (field == "supply") value = &state.supply;
                else if (field == "rent") value = &state.rent;
                else if (field == "pollution") value = &state.pollution;
                else if (field == "solidarity") value = &state.solidarity;
                if (value) *value = std::max(-5, std::min(5, *value + amount));
            }
        } else if (verb == "region_spread") {
            std::string field;
            int amount = 0;
            ss >> field >> amount;
            if (currentRegion_ >= 0 && currentRegion_ < (int)world_.regions.size())
                for (int neighbor : world_.regions[currentRegion_].neighbors) {
                    if (neighbor < 0 || neighbor >= (int)regionStates_.size()) continue;
                    RegionState& state = regionStates_[neighbor];
                    int* value = nullptr;
                    if (field == "prosperity") value = &state.prosperity;
                    else if (field == "danger") value = &state.danger;
                    else if (field == "unrest") value = &state.unrest;
                    else if (field == "pressure") value = &state.pressure;
                    else if (field == "supply") value = &state.supply;
                    else if (field == "rent") value = &state.rent;
                    else if (field == "pollution") value = &state.pollution;
                    else if (field == "solidarity") value = &state.solidarity;
                    if (value) *value = std::max(-5, std::min(5, *value + amount));
                    state.flags.insert("spillover");
                }
        } else if (verb == "region_flag") {
            std::string flag;
            ss >> flag;
            if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
                if (!flag.empty() && flag[0] == '-')
                    regionStates_[currentRegion_].flags.erase(flag.substr(1));
                else {
                    if (!flag.empty() && flag[0] == '+') flag.erase(0, 1);
                    if (!flag.empty()) regionStates_[currentRegion_].flags.insert(flag);
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
            // Grand endings open new beginnings (R7 unlocks).
            if (current_) {
                if (current_->id == "beyond_the_chart") profile_.horizons++;
                if (current_->id == "guildmaster_offer") profile_.guildmaster++;
                SaveProfile(profile_);
            }
#if defined(__EMSCRIPTEN__)
            // A completed life is front-page news in a shared world.
            int bk = boardKeyFor(masterSeed_);
            if (bk >= 0) {
                nlohmann::json d = {{"day", bk},
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
            profile_.miracles++;
            SaveProfile(profile_);
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
    director_.record(*current_, &choice, ch_.day);
    applyEffects(outcome_.effects);
    recordTelemetry(idx, currentDirectorScore_, ch_.dead);
    lastEventSerial_[current_->id] = eventSerial_;
    scheduledFigure_ = -1;
    scheduledRegion_ = -1;
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
    textScroll_ = 0;
    screen_ = OUTCOME;
    // Commit the resolved choice before the player can close the tab. CONTINUE
    // returns to this outcome, so a bad roll cannot be rewound into a free retry.
    saveRun();
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
        textScroll_ = 0;
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
        textScroll_ = 0;
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
    textScroll_ = 0;
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
    if (!enteringSeed_ && IsKeyPressed(KEY_M)) {
        audio_.toggleMusic();
        profile_.musicOff = audio_.musicMuted();
        SaveProfile(profile_);
    }
    // Dev keys for content authoring: trait-gated events are untestable by
    // dice alone. Undocumented, harmless (roguelike full of curses anyway).
    if (IsKeyPressed(KEY_F9)) ch_.traits.insert("cursed");
    if (IsKeyPressed(KEY_F10)) ch_.traits.insert("wanted");
    if (IsKeyPressed(KEY_F11)) cardRequested_ = true; // screenshot any screen
    if (IsKeyPressed(KEY_F8) && screen_ != TITLE) {
        returnScreen_ = screen_;
        textScroll_ = 0;
        screen_ = DIRECTOR;
    }

    ClearBackground(PAL_BG);
    switch (screen_) {
        case TITLE:     drawTitle(mouse); break;
        case CLASSPICK: drawClassPick(mouse); break;
        case AMBITION:  drawAmbitionPick(mouse); break;
        case TRAVEL:    drawTravel(mouse); break;
        case EVENT:     drawEvent(mouse); break;
        case OUTCOME:   drawOutcome(mouse); break;
        case DEATH:     drawDeath(mouse); break;
        case INVENTORY: drawInventory(mouse); break;
        case INFO:      drawInfo(mouse); break;
        case VENDOR:    drawVendor(mouse); break;
        case WORLDMAP:  drawWorldMap(mouse); break;
        case CHRONICLE: drawChronicle(mouse); break;
        case SAGA:      drawSaga(mouse); break;
        case REPLAY:    drawReplay(mouse); break;
        case OPTIONS:   drawOptions(mouse); break;
        case CRAFT:     drawCraft(mouse); break;
        case JOURNAL:   drawJournal(mouse); break;
        case DIRECTOR:  drawDirector(mouse); break;
        case INVESTIGATION: drawInvestigation(mouse); break;
        case NETWORK:   drawNetwork(mouse); break;
        case BALANCE:   drawBalance(mouse); break;
        case RUMORS:    drawRumors(mouse); break;
    }
}

bool Game::uiButton(Rectangle r, const char* label, Vector2 mouse) {
    bool hover = CheckCollisionPointRec(mouse, r);
    DrawRectangleRec(r, hover ? PAL_ROW : Color{31, 27, 46, 255});
    DrawRectangleLinesEx(r, profile_.highContrast ? 2 : 1,
                         profile_.highContrast ? PAL_INK : PAL_DARK);
    int w = MeasureText(label, 10);
    DrawText(label, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - 10) / 2), 10,
             hover ? PAL_GOLD : PAL_DIM);
    if (hover && pressed_) {
        audio_.blip();
        return true;
    }
    return false;
}

// A row gets a second line when its label needs one (R9b), preventing choice
// text from vanishing behind "..". Two lines is the cap; beyond that we trim.
static std::vector<std::string> rowLines(const std::string& label) {
    std::vector<std::string> lines = WrapLines(label, kW - 16, 10);
    if (lines.size() > 2) {
        lines.resize(2);
        while (!lines[1].empty() &&
               MeasureText((lines[1] + " ..").c_str(), 10) > kW - 16)
            lines[1].pop_back();
        lines[1] += " ..";
    }
    return lines;
}

int Game::optionRowsHeight(const std::vector<std::string>& rows) const {
    int h = 0;
    for (size_t i = 0; i < rows.size(); i++) {
        std::string label = std::to_string(i + 1) + ") " + rows[i];
        h += (int)rowLines(label).size() > 1 ? 24 : 13;
    }
    return h;
}

int Game::optionRows(const std::vector<std::string>& rows,
                     const std::vector<bool>& enabled, Vector2 mouse) {
    int n = (int)rows.size();
    int y0 = kH - optionRowsHeight(rows) - 4;
    int clicked = -1;
    int y = y0;
    for (int i = 0; i < n; i++) {
        std::string label = std::to_string(i + 1) + ") " + rows[i];
        std::vector<std::string> lines = rowLines(label);
        int rowH = (int)lines.size() > 1 ? 24 : 13;
        Rectangle r = {4, (float)(y - 1), kW - 8, (float)(rowH - 1)};
        bool hover = enabled[i] && CheckCollisionPointRec(mouse, r);
        if (hover) DrawRectangleRec(r, PAL_ROW);
        Color c = enabled[i] ? (hover ? PAL_GOLD : PAL_INK) : PAL_DARK;
        DrawText(lines[0].c_str(), 8, y, 10, c);
        if (lines.size() > 1) DrawText(lines[1].c_str(), 18, y + 11, 10, c);
        if (hover && pressed_) clicked = i;
        y += rowH;
    }
    for (int i = 0; i < n && i < 9; i++)
        if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i))
            if (enabled[i]) clicked = i;
    return clicked;
}

// The scrollable reader. `follow` keeps the newest line visible (typewriter);
// once follow is off, the ^ / v buttons page through at the reader's pace.
bool Game::drawScrollText(const std::vector<std::string>& lines, int x, int yTop,
                          int maxY, Color color, Vector2 mouse, bool follow) {
    const int fontSize = profile_.largeText ? 12 : 10;
    const int lineH = fontSize + 1;
    int rowsFit = (maxY - yTop) / lineH;
    if (rowsFit < 1) rowsFit = 1;
    int total = (int)lines.size();
    int maxScroll = total > rowsFit ? total - rowsFit : 0;
    if (follow) textScroll_ = maxScroll;
    if (textScroll_ > maxScroll) textScroll_ = maxScroll;
    if (textScroll_ < 0) textScroll_ = 0;
    int y = yTop;
    for (int i = textScroll_; i < total && i < textScroll_ + rowsFit; i++) {
        DrawText(lines[i].c_str(), x, y, fontSize, color);
        y += lineH;
    }
    bool consumed = false;
    if (maxScroll > 0 && !follow) {
        // Slim page controls on the right edge; keys work too.
        if (textScroll_ > 0 &&
            (uiButton({(float)(kW - 16), (float)yTop, 14, 14}, "^", mouse) ||
             IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_K) || IsKeyPressed(KEY_W) ||
             IsKeyPressed(KEY_PAGE_UP))) {
            textScroll_ -= rowsFit > 1 ? rowsFit - 1 : 1;
            consumed = true;
        }
        if (textScroll_ < maxScroll &&
            (uiButton({(float)(kW - 16), (float)(maxY - 16), 14, 14}, "v", mouse) ||
             IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_J) || IsKeyPressed(KEY_S) ||
             IsKeyPressed(KEY_PAGE_DOWN))) {
            textScroll_ += rowsFit > 1 ? rowsFit - 1 : 1;
            consumed = true;
        }
    }
    return consumed;
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
    if (introPage_ >= 0) {
        drawIntro(mouse);
        return;
    }
    const char* title = "RANDOM ROGUE";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 38, 20, PAL_GOLD);
    DrawText("v15", 4, kH - 10, 10, PAL_DARK); // release stamp for bug reports
    if (!dataError_.empty()) {
        DrawTextWrapped(dataError_, 20, 90, kW - 40, PAL_RED);
        return;
    }
#if defined(__EMSCRIPTEN__)
    // A ?mod= deck arrives whenever its fetch lands; merge it once (R7).
    if (!modLoaded_) {
        char* raw = rr_get_mod();
        std::string modText(raw ? raw : "");
        free(raw);
        if (!modText.empty()) {
            modLoaded_ = true;
            size_t before = deck_.size();
            deck_.loadJsonText(modText.c_str());
            size_t added = deck_.size() - before;
            if (added > 0)
                modLine_ = "mod: +" + std::to_string(added) + " events";
        }
    }
#endif
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
    std::string hint = std::string("S: set seed   M: music ") +
                       (audio_.musicMuted() ? "off" : "on");
    if (!pendingLegacy_.empty())
        hint = "this world remembers " + std::to_string(pendingLegacy_.size()) +
               " of your dead   S: set seed";
    if (!modLine_.empty()) hint += "   " + modLine_;
    DrawText(hint.c_str(), (kW - MeasureText(hint.c_str(), 10)) / 2, 102, 10,
             pendingLegacy_.empty() ? PAL_DARK : PAL_GOLD);

    // Touch-friendly: buttons carry the whole flow on iPad.
    bool play = uiButton({8, 118, 72, 18}, "BEGIN", mouse);
    bool reroll = uiButton({86, 118, 72, 18}, "REROLL", mouse);
    // Everyone who presses DAILY today (or WEEKLY this week) shares a world.
    if (uiButton({164, 118, 72, 18}, "DAILY", mouse)) {
        nextSeed_ = dailySeed();
        return;
    }
    if (uiButton({242, 118, 72, 18}, "WEEKLY", mouse)) {
        nextSeed_ = weeklySeed();
        return;
    }
    bool hasSave = !LoadRawRun().empty() || !LoadBackupRun().empty();
    if (hasSave && uiButton({8, 139, 72, 18}, "CONTINUE", mouse)) {
        audio_.blip();
        if (loadRun()) return;
    }
    if (uiButton({164, 139, 72, 18}, "SAGA", mouse)) {
        audio_.blip();
        sagaLives_ = LoadAllLegacy();
        sagaPage_ = 0;
        screen_ = SAGA;
        return;
    }
    if (uiButton({242, 139, 72, 18}, "OPTIONS", mouse)) {
        audio_.blip();
        screen_ = OPTIONS;
        return;
    }
    if (uiButton({86, 139, 72, 18}, "CHRONICLE", mouse)) {
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
        chronDetail_ = -1;
        chronFilterActor_ = chronFilterFaction_ = -1;
        chronFilterSite_ = chronFilterArtifact_ = chronFilterBeast_ = -1;
        chronFilterList_.clear();
        screen_ = CHRONICLE;
        return;
    }

#if defined(__EMSCRIPTEN__)
    // The fallen of the shared world (daily or weekly leaderboard).
    int boardKey = boardKeyFor(nextSeed_);
    if (boardKey >= 0) {
        if (boardKey != lastBoardKey_) {
            // Switched boards (daily <-> weekly): refetch everything.
            lastBoardKey_ = boardKey;
            scoresRequested_ = ghostsRequested_ = deedsRequested_ = false;
            scoresJson_.clear();
        }
        if (!scoresRequested_) {
            scoresRequested_ = true;
            rr_fetch_scores(boardKey);
        }
        if (!ghostsRequested_) {
            // Strangers' graves arrive while the player reads the title.
            ghostsRequested_ = true;
            rr_fetch_ghosts(boardKey, "");
        }
        if (!deedsRequested_) {
            deedsRequested_ = true;
            rr_fetch_deeds(boardKey);
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
                std::string line = boardKey >= 7000000 ? "this week's fallen: "
                                                       : "today's fallen: ";
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
    if (key == KEY_M) {
        audio_.toggleMusic();
        profile_.musicOff = audio_.musicMuted();
        SaveProfile(profile_);
        return;
    }
    if (key == KEY_R) {
        nextSeed_ = (nextSeed_ * 6364136223846793005ULL + 1442695040888963407ULL) % 1000000000ULL;
        return;
    }
    if (play || key != 0) {
        audio_.blip();
        // First life ever: three cards of how-to before the class pick (R7).
        if (!profile_.seenIntro) {
            introPage_ = 0;
            return;
        }
        screen_ = CLASSPICK;
    }
}

// The how-to cards: everything a first life needs, nothing more (R7).
void Game::drawIntro(Vector2 mouse) {
    static const char* kCards[4] = {
        "HOW TO BE BRIEFLY ALIVE\n\nTravel between places. Each place deals "
        "event cards; every card offers choices, and some choices roll dice "
        "against your stats.\n\nYou will die. That is not failure - your name, "
        "your deeds, and your death all become this world's permanent history. "
        "TAB opens your pack.",
        "THE WORLD REMEMBERS\n\nReplay the same seed and you return YEARS "
        "later: your dead are historical figures, their lost things are "
        "findable relics, and the folk you wronged hold grudges against your "
        "heirs.\n\nDAILY and WEEKLY worlds are shared with every other player "
        "alive right now - their graves, deeds, and falls appear in yours.",
        "POWER HAS A MEMORY\n\nRegions track rent, supply, pollution, and "
        "solidarity. Named people pursue agendas whether you help them or not. "
        "Rumors travel, mutate, and sometimes warn you about what comes next.\n\n"
        "Open the Story Journal to inspect people, rumors, investigations, and "
        "the material condition of the place you are standing in.",
        "THE LONG GAME\n\nTraits change which cards find you. Gods bank favor "
        "and may catch you when you fall - once. Contracts build careers, "
        "words of the old tongue accumulate forever, and ships cross to other "
        "shores.\n\nThere are ways out alive. One is up. One is past the edge "
        "of the chart. Good luck.",
    };
    if (introPage_ < 0) introPage_ = 0;
    DrawTextWrapped(kCards[introPage_], 16, 24, kW - 32, PAL_INK, kH - 26);
    std::string pg = std::to_string(introPage_ + 1) + "/4";
    DrawText(pg.c_str(), 8, kH - 16, 10, PAL_DARK);
    const char* label = introPage_ < 3 ? "NEXT" : "BEGIN";
    if (uiButton({(float)(kW - 56), (float)(kH - 22), 52, 18}, label, mouse) ||
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        introPage_++;
        if (introPage_ >= 4) {
            introPage_ = -1;
            profile_.seenIntro = true;
            SaveProfile(profile_);
            screen_ = CLASSPICK;
        }
    }
}

// Settings: small, persistent, and out of the way (R7).
void Game::drawOptions(Vector2 mouse) {
    const char* head = "OPTIONS";
    DrawText(head, (kW - MeasureText(head, 10)) / 2, 24, 10, PAL_GOLD);
    static const char* kSpeeds[4] = {"slow", "normal", "fast", "instant"};
    static const char* kVols[3] = {"quiet", "medium", "loud"};
    std::string r1 = std::string("Text speed: ") + kSpeeds[profile_.textSpeed % 4];
    std::string r2 = std::string("Volume: ") + kVols[profile_.volume % 3];
    std::string r3 = std::string("Music: ") + (profile_.musicOff ? "off" : "on");
    std::string r4 = std::string("Screen motion: ") +
                     (profile_.reducedMotion ? "reduced" : "full");
    if (uiButton({8, 44, 148, 18}, r1.c_str(), mouse)) {
        profile_.textSpeed = (profile_.textSpeed + 1) % 4;
        SaveProfile(profile_);
    }
    if (uiButton({164, 44, 148, 18}, r2.c_str(), mouse)) {
        profile_.volume = (profile_.volume + 1) % 3;
        SetMasterVolume(0.35f + 0.3f * profile_.volume);
        SaveProfile(profile_);
        audio_.coin(); // hear the new level immediately
    }
    if (uiButton({8, 66, 148, 18}, r3.c_str(), mouse)) {
        profile_.musicOff = !profile_.musicOff;
        if (profile_.musicOff != audio_.musicMuted()) audio_.toggleMusic();
        SaveProfile(profile_);
    }
    if (uiButton({164, 66, 148, 18}, r4.c_str(), mouse)) {
        profile_.reducedMotion = !profile_.reducedMotion;
        shake_ = 0.0f;
        SaveProfile(profile_);
    }
    std::string privacy = std::string("Anonymous tuning: ") +
                          (profile_.analyticsOff ? "off" : "on");
    std::string reading = std::string("Reader text: ") +
                          (profile_.largeText ? "large" : "standard");
    std::string contrast = std::string("Contrast: ") +
                           (profile_.highContrast ? "high" : "standard");
    if (uiButton({8, 88, 148, 18}, reading.c_str(), mouse)) {
        profile_.largeText = !profile_.largeText;
        SaveProfile(profile_);
    }
    if (uiButton({164, 88, 148, 18}, contrast.c_str(), mouse)) {
        profile_.highContrast = !profile_.highContrast;
        SaveProfile(profile_);
    }
    if (uiButton({8, 110, 148, 18}, privacy.c_str(), mouse)) {
        profile_.analyticsOff = !profile_.analyticsOff;
#if defined(__EMSCRIPTEN__)
        if (profile_.analyticsOff) rr_clear_telemetry();
#endif
        SaveProfile(profile_);
    }
    if (uiButton({164, 110, 148, 18}, "DIRECTOR DIAGNOSTICS", mouse)) {
        returnScreen_ = OPTIONS;
        textScroll_ = 0;
        screen_ = DIRECTOR;
        return;
    }
    if (uiButton({8, 132, 148, 18}, "BALANCE DASHBOARD", mouse)) {
        balanceRequested_ = false;
        balanceJson_.clear();
        textScroll_ = 0;
        screen_ = BALANCE;
        return;
    }
    if (uiButton({164, 132, 148, 18}, "REPLAY THE HOW-TO", mouse)) {
        screen_ = TITLE;
        introPage_ = 0;
        return;
    }
    if (uiButton({(float)(kW - 52), (float)(kH - 18), 48, 14}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE))
        screen_ = TITLE;
}

void Game::drawClassPick(Vector2 mouse) {
    const char* head = "WHO ARE YOU THIS TIME?";
    DrawText(head, (kW - MeasureText(head, 10)) / 2, 24, 10, PAL_GOLD);

    auto classes = startClasses();
    const int perPage = 4;
    int pages = std::max(1, ((int)classes.size() + perPage - 1) / perPage);
    classPage_ = std::max(0, std::min(pages - 1, classPage_));
    int first = classPage_ * perPage;
    int pick = -1;
    for (int row = 0; row < perPage && first + row < (int)classes.size(); row++) {
        int ci = first + row;
        const StartClass& value = classes[ci];
        std::string label = std::to_string(row + 1) + ") " + value.name +
            (value.unlocked ? " - " + value.blurb : " (locked: " + value.lockHint + ")");
        std::vector<std::string> lines = rowLines(label);
        int y = 52 + row * 25;
        Rectangle box{4, (float)y, kW - 8.0f, 22};
        bool hover = value.unlocked && CheckCollisionPointRec(mouse, box);
        if (hover) DrawRectangleRec(box, PAL_ROW);
        Color color = value.unlocked ? (hover ? PAL_GOLD : PAL_INK) : PAL_DARK;
        DrawText(lines[0].c_str(), 8, y + 1, 10, color);
        if (lines.size() > 1) DrawText(lines[1].c_str(), 18, y + 11, 10, color);
        if (hover && pressed_) pick = ci;
        if (IsKeyPressed(KEY_ONE + row) && value.unlocked) pick = ci;
    }
    if (pick >= 0) {
        audio_.blip();
        pendingClass_ = pick;
        // Roll three ambitions to choose from — deterministic per world+generation.
        Rng ambRng(nextSeed_ ^ (pendingLegacy_.size() * 8191ULL), 99);
        ambitionChoices_.clear();
        while ((int)ambitionChoices_.size() < 3) {
            int a = ambRng.range(0, 11);
            bool dup = false;
            for (int c : ambitionChoices_)
                if (c == a) dup = true;
            if (!dup) ambitionChoices_.push_back(a);
        }
        screen_ = AMBITION;
    }
    if (classPage_ > 0 &&
        (uiButton({56, (float)(kH - 20), 48, 16}, "< PREV", mouse) ||
         IsKeyPressed(KEY_LEFT))) classPage_--;
    if (classPage_ < pages - 1 &&
        (uiButton({108, (float)(kH - 20), 48, 16}, "NEXT >", mouse) ||
         IsKeyPressed(KEY_RIGHT))) classPage_++;
    if (uiButton({4, (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) {
        audio_.blip();
        screen_ = TITLE;
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
    int pick = -1;
    for (int i = 0; i < (int)rows.size(); i++) {
        std::string label = std::to_string(i + 1) + ") " + rows[i];
        std::vector<std::string> lines = rowLines(label);
        int y = 54 + i * 25;
        Rectangle box{4, (float)y, kW - 8.0f, 22};
        bool hover = CheckCollisionPointRec(mouse, box);
        if (hover) DrawRectangleRec(box, PAL_ROW);
        DrawText(lines[0].c_str(), 8, y + 1, 10, hover ? PAL_GOLD : PAL_INK);
        if (lines.size() > 1) DrawText(lines[1].c_str(), 18, y + 11, 10,
                                      hover ? PAL_GOLD : PAL_INK);
        if (hover && pressed_) pick = i;
        if (IsKeyPressed(KEY_ONE + i)) pick = i;
    }
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
    if (uiButton({4, (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) {
        audio_.blip();
        screen_ = CLASSPICK;
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
    if (worldGen_ >= 2)
        sky += "  This world has taken " + std::to_string(worldGen_) +
               " of your line; it hits harder and pays better.";
    if (!newsLine_.empty()) sky += "  NEWS: " + newsLine_;
    y = DrawTextWrapped(sky, 8, y + 1, kW - 16, PAL_DIM, y + 24);
    if (comp_.active) {
        std::string cline = "With you: " + comp_.name + ", " + comp_.kind +
                            (comp_.devoted() ? " (devoted)" : "");
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
    if (uiButton({(float)(kW - 156), (float)(y + 1), 48, 16}, "JOURNAL", mouse)) {
        textScroll_ = 0;
        screen_ = JOURNAL;
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
            landfalls_++;
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
        static const float kRevealSpeed[4] = {60.0f, 110.0f, 240.0f, 100000.0f};
        reveal_ += GetFrameTime() * kRevealSpeed[profile_.textSpeed % 4];
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            pressed_)
            reveal_ = (float)currentText_.size();
    }
    int choicesTop = kH - optionRowsHeight(choiceTexts_) - 6;
    int textBottom = compLine_.empty() ? choicesTop : choicesTop - 13;
    // The reader: long cards scroll instead of clipping (R9b). While the
    // typewriter runs it follows the newest line; afterwards ^ / v page it.
    int readerSize = profile_.largeText ? 12 : 10;
    std::vector<std::string> lines =
        WrapLines(currentText_.substr(0, (size_t)reveal_), kW - 24, readerSize);
    drawScrollText(lines, 8, 22, textBottom, PAL_INK, mouse, !done);
    if (!compLine_.empty() && done) {
        std::string cl = compLine_;
        while (!cl.empty() && MeasureText((cl + "..").c_str(), 10) > kW - 16)
            cl.pop_back();
        if (cl.size() < compLine_.size()) cl += "..";
        DrawText(cl.c_str(), 8, choicesTop - 12, 10, PAL_DARK);
    }

    std::vector<bool> ok;
    for (auto& c : current_->choices) ok.push_back(c.requires_.met(ch_));
    int pick = optionRows(choiceTexts_, ok, mouse);
    if (pick >= 0) { chooseOption(pick); return; }
    if (IsKeyPressed(KEY_TAB)) openInventory();
}

void Game::drawOutcome(Vector2 mouse) {
    drawTopBar();
    int y = 22;
    if (!outcome_.rollText.empty()) {
        bool success = outcome_.rollText.find("success") != std::string::npos;
        y = DrawTextWrapped(outcome_.rollText, 8, y, kW - 16,
                            success ? PAL_GREEN : PAL_RED, kH - 18);
        y += 4;
    }
    // Everything below the roll scrolls as one readable block (R9b).
    std::string body = outcome_.text;
    std::string fx;
    for (auto& e : outcome_.effects)
        if (e.rfind("die", 0) != 0 && e != "shop" && e.rfind("goto", 0) != 0)
            fx += "[" + e + "] ";
    if (!fx.empty()) body += "\n" + fx;
    if (blessingSpent_)
        body += "\nThe blessing spends itself. You live. Somewhere, a ledger updates.";
    int readerSize = profile_.largeText ? 12 : 10;
    bool scrolled = drawScrollText(WrapLines(body, kW - 24, readerSize), 8, y, kH - 18,
                                   PAL_INK, mouse, false);

    const char* prompt = "[tap or Enter to continue]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (!scrolled && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        pressed_)) {
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
            ok.push_back(ch_.money >= price && (int)ch_.pack.size() < ch_.capacity());
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
        saveRun();
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
                           std::to_string(ch_.capacity()) + ")";
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
        if (uiButton({(float)(kW - 108), 20, 50, 15}, "CRAFT", mouse)) {
            craftScroll_ = 0;
            screen_ = CRAFT;
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
                if (isBook) {
                    profile_.wordsLearned += 2; // reading teaches the tongue
                    wordsThisRun_ += 2;
                }
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
                saveRun();
                showInfo(text);
                return;
            }
            std::string fxText;
            for (auto& u : item.useEffects) fxText += "[" + u + "] ";
            applyEffects(item.useEffects);
            std::string used = "You use the " + item.name + ". " + fxText;
            ch_.pack.erase(ch_.pack.begin() + invSelected_);
            invSelected_ = -1;
            saveRun();
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
        saveRun();
    } else if (pick == 2 || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        invSelected_ = -1;
    }
}

bool Game::craftRecipe(const ItemRecipe& recipe) {
    int first = -1, second = -1;
    for (int i = 0; i < (int)ch_.pack.size(); i++) {
        if (first < 0 && ch_.pack[i].templateId == recipe.first) first = i;
        else if (second < 0 && ch_.pack[i].templateId == recipe.second) second = i;
    }
    if (first < 0 || second < 0) return false;
    if (first < second) std::swap(first, second);
    ch_.pack.erase(ch_.pack.begin() + first);
    ch_.pack.erase(ch_.pack.begin() + second);
    outcome_.text.clear();
    std::string resultName;
    if (!recipe.result.empty()) {
        ItemInstance made = makeItem(recipe.result);
        resultName = made.name;
        takeItem(made);
    }
    applyEffects(recipe.effects);
    saveRun();
    std::string result = recipe.text;
    if (!resultName.empty()) result += "\n\nCreated: " + resultName + ".";
    showInfo(result);
    infoBack_ = CRAFT;
    return true;
}

void Game::drawCraft(Vector2 mouse) {
    siteName_ = "your workbench";
    drawTopBar();
    DrawText("FIELD RECIPES", 8, 22, 10, PAL_GOLD);
    DrawText("Every result keeps both ingredients' story moving.", 8, 34, 10, PAL_DIM);
    const std::vector<ItemRecipe>& recipes = items_.recipes();
    const int perPage = 5;
    int pages = std::max(1, ((int)recipes.size() + perPage - 1) / perPage);
    if (craftScroll_ >= pages) craftScroll_ = pages - 1;
    int start = craftScroll_ * perPage;
    int y = 49;
    for (int i = start; i < (int)recipes.size() && i < start + perPage; i++) {
        const ItemRecipe& recipe = recipes[i];
        bool ready = ch_.hasItem(recipe.first) && ch_.hasItem(recipe.second);
        Rectangle row{4, (float)y, kW - 8.0f, 18};
        bool hover = ready && CheckCollisionPointRec(mouse, row);
        DrawRectangleRec(row, hover ? PAL_ROW : Color{31, 27, 46, 255});
        DrawRectangleLinesEx(row, 1, PAL_DARK);
        std::string label = recipe.name + (ready ? "  [READY]" : "");
        DrawText(label.c_str(), 8, y + 2, 10, ready ? (hover ? PAL_GOLD : PAL_INK) : PAL_DARK);
        if (hover && pressed_) {
            audio_.chime();
            craftRecipe(recipe);
            return;
        }
        y += 20;
    }
    std::string page = std::to_string(craftScroll_ + 1) + "/" + std::to_string(pages);
    DrawText(page.c_str(), (kW - MeasureText(page.c_str(), 10)) / 2, kH - 16, 10, PAL_DIM);
    if (craftScroll_ > 0 && uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse))
        craftScroll_--;
    if (craftScroll_ < pages - 1 &&
        uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse))
        craftScroll_++;
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        invSelected_ = -1;
        screen_ = INVENTORY;
    }
}

void Game::drawJournal(Vector2 mouse) {
    siteName_ = "the story ledger";
    drawTopBar();
    std::string text = "STORY LEDGER\n\n";
    if (contract_.active)
        text += "CURRENT COMMISSION\n" + contract_.desc + "\n\n";
    else
        text += "CURRENT COMMISSION\nNone. Your schedule is insultingly available.\n\n";

    if (mystery_.active) {
        text += "OPEN MYSTERY: " + mystery_.title + "\n" + mystery_.publicStory + "\n";
        if (mystery_.solved)
            text += "SOLVED: " + mystery_.secret + "\n\n";
        else {
            text += "Clues: " + std::to_string(mystery_.clues) + "/3, evidence " +
                    std::to_string(mystery_.evidence) + ", doubt " +
                    std::to_string(mystery_.doubt) + ".\n";
            if (mystery_.accused >= 0 && mystery_.accused < (int)history_.figures.size())
                text += "Accused: " + history_.figures[mystery_.accused].name + ".\n";
            if (mystery_.tried)
                text += mystery_.correctVerdict ? "The verdict held.\n" :
                    "The verdict failed. The case remains open.\n";
            text += "\n";
        }
    }

    if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
        const RegionState& state = regionStates_[currentRegion_];
        text += "CURRENT REGION\n" + world_.regions[currentRegion_].name + " is " +
                state.description() + ".\nSupply " + std::to_string(state.supply) +
                ", rent burden " + std::to_string(state.rent) +
                ", pollution " + std::to_string(state.pollution) +
                ", solidarity " + std::to_string(state.solidarity) + ".\n\n";
    }

    text += "CHOICES WAITING TO RETURN\n";
    if (consequences_.empty() && scheduledNextId_.empty()) text += "None that admit it.\n";
    for (const PendingConsequence& value : consequences_)
        text += "Day " + std::to_string(value.dueDay) + ": " +
                (value.summary.empty() ? value.source : value.summary) + "\n";
    if (!scheduledNextId_.empty()) text += "Now: an old decision has caught up.\n";

    std::set<std::string> activeFamilies = activeStoryFamilies();
    text += "\nACTIVE STORY THREADS\n";
    if (activeFamilies.empty()) text += "No long thread currently has hold of your sleeve.\n";
    for (const std::string& family : activeFamilies) {
        std::string label = family;
        std::replace(label.begin(), label.end(), '_', ' ');
        int beats = 0;
        int nearest = 9999;
        for (const PendingConsequence& value : consequences_) {
            const Event* event = deck_.find(value.eventId);
            if (event && event->family == family) {
                beats++;
                nearest = std::min(nearest, std::max(0, value.dueDay - ch_.day));
            }
        }
        const Event* scheduled = deck_.find(scheduledNextId_);
        int scheduledBeat = scheduled && scheduled->family == family ? 1 : 0;
        int totalBeats = beats + scheduledBeat;
        text += label + ": " + std::to_string(totalBeats) + " unresolved beat" +
                (totalBeats == 1 ? "" : "s") +
                (nearest < 9999 ? ", stirring in about " + std::to_string(nearest) +
                                   " day" + (nearest == 1 ? "" : "s") : "") + ".\n";
    }

    if (!storyEchoes_.empty()) {
        text += "\nSTORIES NOW LIVING IN THE WORLD\n";
        for (const std::string& family : storyEchoes_) {
            std::string label = family;
            std::replace(label.begin(), label.end(), '_', ' ');
            text += label + " now affects ordinary people, prices, rumors, and roads.\n";
        }
    }

    text += "\nPEOPLE WHO REMEMBER YOU\n";
    int shown = 0;
    for (auto& pair : npcRelations_) {
        int fi = pair.first;
        if (fi < 0 || fi >= (int)history_.figures.size() || shown++ >= 10) continue;
        const NpcRelation& rel = pair.second;
        text += history_.figures[fi].name + ": trust " + std::to_string(rel.trust) +
                ", respect " + std::to_string(rel.respect) +
                ", fear " + std::to_string(rel.fear) +
                ", debt " + std::to_string(rel.debt) +
                ", grudge " + std::to_string(rel.grudge) + ".\n";
    }
    if (npcRelations_.empty()) text += "Nobody yet. Give it time.\n";

    text += "\nTHEIR TIES TO EACH OTHER\n";
    int tiesShown = 0;
    for (const SocialTie& tie : socialTies_) {
        if (tiesShown >= 10) break;
        if (!npcRelations_.count(tie.a) && !npcRelations_.count(tie.b)) continue;
        if (tie.a < 0 || tie.b < 0 || tie.a >= (int)history_.figures.size() ||
            tie.b >= (int)history_.figures.size()) continue;
        text += history_.figures[tie.a].name + " and " + history_.figures[tie.b].name +
                ": " + tie.kind + ", affinity " + std::to_string(tie.affinity) + ".\n";
        tiesShown++;
    }
    if (!tiesShown) text += "You have not learned enough to draw the lines yet.\n";

    int readerSize = profile_.largeText ? 12 : 10;
    drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 20, kH - 20,
                   PAL_INK, mouse, false);
    if (mystery_.active && uiButton({4, (float)(kH - 20), 58, 16}, "CASE", mouse)) {
        textScroll_ = 0;
        screen_ = INVESTIGATION;
        return;
    }
    if (uiButton({66, (float)(kH - 20), 64, 16}, "PEOPLE", mouse)) {
        networkPage_ = 0;
        networkSelected_ = -1;
        screen_ = NETWORK;
        return;
    }
    if (uiButton({134, (float)(kH - 20), 64, 16}, "RUMORS", mouse)) {
        rumorPage_ = 0;
        rumorDetail_ = -1;
        screen_ = RUMORS;
        return;
    }
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        textScroll_ = 0;
        screen_ = TRAVEL;
    }
}

void Game::drawInvestigation(Vector2 mouse) {
    drawTopBar();
    DrawText("INVESTIGATION BOARD", 8, 20, 10, PAL_GOLD);
    if (!mystery_.active) {
        DrawTextWrapped("There is no open case. This is probably temporary.", 8, 40,
                        kW - 16, PAL_DIM);
    } else {
        std::string text = mystery_.title + "\n" + mystery_.publicStory + "\n\n";
        text += "Evidence " + std::to_string(mystery_.evidence) +
                "  Doubt " + std::to_string(mystery_.doubt) +
                "  Leads " + std::to_string(mystery_.clues) + "/3\n";
        int strength = mystery_.evidence * 2 + mystery_.clues - mystery_.doubt * 2;
        text += "Case strength: " + std::string(strength >= 6 ? "strong" :
                    strength >= 3 ? "contested" : "fragile") + ".\n";
        if (mystery_.artifact >= 0 && mystery_.artifact < (int)history_.artifacts.size())
            text += "Object: " + history_.artifacts[mystery_.artifact].display() + "\n";
        if (mystery_.site >= 0 && mystery_.site < (int)world_.sites.size())
            text += "Scene: " + world_.sites[mystery_.site].name + "\n";
        // Keep the truthful suspect from occupying a learnable button position.
        // The order is stable for a saved case, but changes between world seeds.
        bool swapSuspects = ((masterSeed_ + (uint64_t)(mystery_.site + 1) * 97ULL) & 1ULL) != 0;
        int suspects[2] = {
            swapSuspects ? mystery_.decoy : mystery_.culprit,
            swapSuspects ? mystery_.culprit : mystery_.decoy
        };
        for (int i = 0; i < 2; i++) {
            int fi = suspects[i];
            if (fi < 0 || fi >= (int)history_.figures.size()) continue;
            const Figure& f = history_.figures[fi];
            int reliability = 35 + (int)((masterSeed_ + (uint64_t)fi * 37ULL) % 61ULL);
            text += "\n" + std::string(i == 0 ? "A: " : "B: ") + f.name +
                    ", " + f.profession + ".\n";
            text += "Testimony reliability " + std::to_string(reliability) +
                    "%. Motive: " + (f.trait.empty() ? "unclear" : f.trait) +
                    ". Alibi: " + (reliability >= 70 ? "corroborated" :
                                    reliability >= 50 ? "partial" : "contradictory") + ".\n";
        }
        if (mystery_.accused >= 0 && mystery_.accused < (int)history_.figures.size())
            text += "\nMarked for accusation: " + history_.figures[mystery_.accused].name + ".";
        if (mystery_.tried)
            text += mystery_.correctVerdict ? "\nThe verdict held." :
                    "\nThe verdict failed. An appeal can reopen the record.";
        int readerSize = profile_.largeText ? 12 : 10;
        drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 34, kH - 42,
                       PAL_INK, mouse, false);

        if (!mystery_.tried) {
            if (uiButton({4, (float)(kH - 39), 70, 16}, "MARK A", mouse))
                mystery_.accused = suspects[0];
            if (uiButton({78, (float)(kH - 39), 70, 16}, "MARK B", mouse))
                mystery_.accused = suspects[1];
        } else if (!mystery_.correctVerdict && !mystery_.appealed && ch_.money >= 8 &&
                   uiButton({4, (float)(kH - 39), 144, 16}, "APPEAL: 8 GOLD", mouse)) {
            ch_.money -= 8;
            mystery_.appealed = true;
            mystery_.tried = false;
            mystery_.accused = -1;
            mystery_.doubt = std::min(9, mystery_.doubt + 1);
            queueConsequence(2, "r12_mystery_second",
                             "An appeal has forced another witness to speak.");
            saveRun();
        }
    }
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) {
        textScroll_ = 0;
        screen_ = JOURNAL;
    }
}

void Game::drawNetwork(Vector2 mouse) {
    drawTopBar();
    DrawText("PEOPLE AND CONNECTIONS", 8, 20, 10, PAL_GOLD);
    std::vector<int> known;
    for (const auto& pair : npcRelations_)
        if (pair.first >= 0 && pair.first < (int)history_.figures.size())
            known.push_back(pair.first);
    if (networkSelected_ >= 0 && networkSelected_ < (int)history_.figures.size()) {
        int fi = networkSelected_;
        const Figure& person = history_.figures[fi];
        const NpcRelation* rel = relationIfKnown(fi);
        std::string text = person.name + "\n" + person.trait + " " + person.profession + "\n";
        if (rel) text += "Trust " + std::to_string(rel->trust) + "  respect " +
            std::to_string(rel->respect) + "  fear " + std::to_string(rel->fear) +
            "\nDebt " + std::to_string(rel->debt) + "  affection " +
            std::to_string(rel->affection) + "  grudge " + std::to_string(rel->grudge) +
            "\nKnowledge " + std::to_string(rel->knowledge) + "/10\n\n";
        const Agenda* agenda = agendaFor(fi);
        if (agenda) {
            if (rel && rel->knowledge >= 3) {
                text += "AGENDA: " + agendaName(agenda->kind) + ".\n";
                if (agenda->region >= 0 && agenda->region < (int)world_.regions.size())
                    text += "Operating near " + world_.regions[agenda->region].name + ".\n";
            } else {
                text += "AGENDA: not yet understood.\n";
            }
        }
        if (fi == nemesisFigure_) text += "STATUS: PERSONAL NEMESIS. This is now a project.\n";
        text += "\n";
        text += "KNOWN CONNECTIONS\n";
        int links = 0;
        for (const SocialTie& tie : socialTies_) {
            if (tie.a != fi && tie.b != fi) continue;
            int other = tie.a == fi ? tie.b : tie.a;
            if (other < 0 || other >= (int)history_.figures.size()) continue;
            bool visible = npcRelations_.count(other) || (rel && rel->knowledge >= 2);
            text += visible ? history_.figures[other].name + ": " + tie.kind +
                                ", affinity " + std::to_string(tie.affinity) + "\n"
                            : "Unknown person: hidden connection\n";
            links++;
        }
        if (!links) text += "No lines have been drawn yet.\n";
        int readerSize = profile_.largeText ? 12 : 10;
        drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 36, kH - 20,
                       PAL_INK, mouse, false);
        if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "LIST", mouse) ||
            IsKeyPressed(KEY_ESCAPE)) {
            networkSelected_ = -1;
            textScroll_ = 0;
        }
        return;
    }

    if (known.empty())
        DrawTextWrapped("Nobody knows you well enough to become a diagram yet.", 8, 42,
                        kW - 16, PAL_DIM);
    const int perPage = 5;
    int pages = std::max(1, ((int)known.size() + perPage - 1) / perPage);
    networkPage_ = std::max(0, std::min(pages - 1, networkPage_));
    for (int row = 0; row < perPage; row++) {
        int pos = networkPage_ * perPage + row;
        if (pos >= (int)known.size()) break;
        int fi = known[pos];
        const NpcRelation& rel = npcRelations_.at(fi);
        std::string label = history_.figures[fi].name + "  K" +
                            std::to_string(rel.knowledge) + " G" +
                            std::to_string(rel.grudge);
        if (uiButton({8, (float)(42 + row * 22), 304, 18}, label.c_str(), mouse)) {
            networkSelected_ = fi;
            textScroll_ = 0;
            return;
        }
    }
    if (networkPage_ > 0 && uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse))
        networkPage_--;
    if (networkPage_ + 1 < pages && uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse))
        networkPage_++;
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) screen_ = JOURNAL;
}

void Game::drawRumors(Vector2 mouse) {
    drawTopBar();
    DrawText("RUMOR EXCHANGE", 8, 20, 10, PAL_GOLD);
    if (rumorDetail_ >= 0 && rumorDetail_ < (int)rumors_.size()) {
        int id = rumors_[rumorDetail_].id;
        const Rumor& value = rumors_[rumorDetail_];
        std::string text = value.text + "\n\n";
        text += "Age " + std::to_string(value.age) + " days. Reach " +
                std::to_string(value.reach) + " regions.\n";
        if (value.origin >= 0 && value.origin < (int)world_.regions.size())
            text += "First heard near " + world_.regions[value.origin].name + ".\n";
        if (value.figure >= 0 && value.figure < (int)history_.figures.size() &&
            relationIfKnown(value.figure))
            text += "Associated with " + history_.figures[value.figure].name + ".\n";
        if (verifiedRumors_.count(id)) {
            const char* assessment = value.truth >= 70 ? "well supported" :
                value.truth >= 40 ? "contested" : "manufactured or badly distorted";
            text += "\nYour investigation: " + std::string(assessment) + ".";
        } else {
            text += "\nIts reliability is unknown. Confidence is not evidence.";
        }
        if (!value.foreshadowEvent.empty())
            text += "\nThis rumor sounds less like gossip than a warning.";
        int readerSize = profile_.largeText ? 12 : 10;
        drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 36, kH - 42,
                       PAL_INK, mouse, false);

        if (!verifiedRumors_.count(id) &&
            uiButton({4, (float)(kH - 39), 60, 16}, "VERIFY", mouse)) {
            verifiedRumors_.insert(id);
            dailyTick();
            saveRun();
            return;
        }
        if (uiButton({68, (float)(kH - 39), 60, 16}, "AMPLIFY", mouse)) {
            for (Rumor& rumor : rumors_)
                if (rumor.id == id) {
                    rumor.reach += 2;
                    if (currentRegion_ >= 0 && currentRegion_ < (int)regionStates_.size()) {
                        if (rumor.truth < 35) regionStates_[currentRegion_].unrest++;
                        if (rumor.truth >= 70) regionStates_[currentRegion_].solidarity++;
                    }
                    break;
                }
            saveRun();
        }
        if (ch_.money >= 4 && uiButton({132, (float)(kH - 39), 60, 16}, "BURY 4G", mouse)) {
            ch_.money -= 4;
            for (auto it = rumors_.begin(); it != rumors_.end(); ++it)
                if (it->id == id) {
                    rumors_.erase(it);
                    break;
                }
            rumorDetail_ = -1;
            saveRun();
            return;
        }
        if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "LIST", mouse) ||
            IsKeyPressed(KEY_ESCAPE)) {
            rumorDetail_ = -1;
            textScroll_ = 0;
        }
        return;
    }

    const int perPage = 5;
    int pages = std::max(1, ((int)rumors_.size() + perPage - 1) / perPage);
    rumorPage_ = std::max(0, std::min(pages - 1, rumorPage_));
    if (rumors_.empty())
        DrawTextWrapped("Nobody is talking. Either peace has arrived or counsel has advised silence.",
                        8, 44, kW - 16, PAL_DIM);
    for (int row = 0; row < perPage; row++) {
        int reversePos = rumorPage_ * perPage + row;
        int index = (int)rumors_.size() - 1 - reversePos;
        if (index < 0) break;
        const Rumor& value = rumors_[index];
        std::string label = verifiedRumors_.count(value.id) ? "V " : "? ";
        label += value.text;
        while (!label.empty() && MeasureText(label.c_str(), 10) > 296) label.pop_back();
        if (uiButton({8, (float)(42 + row * 22), 304, 18}, label.c_str(), mouse)) {
            rumorDetail_ = index;
            textScroll_ = 0;
            return;
        }
    }
    if (rumorPage_ > 0 && uiButton({4, (float)(kH - 20), 48, 16}, "< PREV", mouse))
        rumorPage_--;
    if (rumorPage_ + 1 < pages && uiButton({56, (float)(kH - 20), 48, 16}, "NEXT >", mouse))
        rumorPage_++;
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) screen_ = JOURNAL;
}

void Game::drawDirector(Vector2 mouse) {
    DrawText("STORY DIRECTOR", 8, 6, 10, PAL_GOLD);
    std::string text = "This screen explains card selection only. It never reveals hidden outcomes or changes a roll.\n\n";
    if (current_) {
        std::vector<std::string> lines = director_.explain(
            *current_, storyContext(deckTag_));
        for (const std::string& line : lines) text += line + "\n";
    } else {
        text += "No card is currently in the Director's hands.\n";
    }
    text += "\nRECENT FAMILIES\n";
    int shown = 0;
    for (auto it = director_.recentFamilies().rbegin();
         it != director_.recentFamilies().rend() && shown++ < 8; ++it)
        text += *it + (shown % 4 == 0 ? "\n" : "  ");
    text += "\n\nLEAST USED TAGS\n";
    std::vector<std::pair<int, std::string>> counts;
    for (const auto& pair : director_.tagCounts()) counts.push_back({pair.second, pair.first});
    std::sort(counts.begin(), counts.end());
    for (int i = 0; i < (int)counts.size() && i < 12; i++)
        text += counts[i].second + " " + std::to_string(counts[i].first) +
                (i % 3 == 2 ? "\n" : "  ");
    text += "\n\nRUN CARDS " + std::to_string(eventSerial_) +
            "  UNIQUE " + std::to_string(lastEventSerial_.size()) +
            "  PENDING CONSEQUENCES " + std::to_string(consequences_.size());
    int readerSize = profile_.largeText ? 12 : 10;
    drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 22, kH - 20,
                   PAL_INK, mouse, false);
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) {
        textScroll_ = 0;
        screen_ = returnScreen_;
    }
}

void Game::drawBalance(Vector2 mouse) {
    DrawText("BALANCE DASHBOARD", 8, 6, 10, PAL_GOLD);
#if defined(__EMSCRIPTEN__)
    if (!balanceRequested_) {
        rr_fetch_balance();
        balanceRequested_ = true;
    }
    if (balanceJson_.empty()) {
        char* raw = rr_get_balance();
        balanceJson_ = raw ? raw : "";
        free(raw);
    }
#endif
    std::string text = "LOCAL RUN\n";
    text += "Cards shown " + std::to_string(eventSerial_) +
            ", unique " + std::to_string(lastEventSerial_.size()) +
            ", deck used " + std::to_string(deck_.used().size()) + "/" +
            std::to_string(deck_.size()) + ".\n";
    text += "Active arcs " + std::to_string(activeStoryFamilies().size()) +
            ", pending beats " + std::to_string(consequences_.size()) +
            ", autonomous endings " + std::to_string(autonomousArcResolutions_) + ".\n";
    text += "Story echoes " + std::to_string(storyEchoes_.size()) +
            ", known people " + std::to_string(npcRelations_.size()) + ".\n\n";

    text += "LOCATION PRESSURE\n";
    std::map<std::string, int> seenByDeck;
    for (const auto& pair : lastEventSerial_) {
        const Event* event = deck_.find(pair.first);
        if (!event) continue;
        for (const std::string& location : event->locations) seenByDeck[location]++;
    }
    for (const auto& pair : seenByDeck)
        text += pair.first + " " + std::to_string(pair.second) + " seen\n";

    text += "\nANONYMOUS GLOBAL TUNING\n";
#if defined(__EMSCRIPTEN__)
    nlohmann::json rows = nlohmann::json::parse(balanceJson_, nullptr, false);
    if (balanceJson_.empty()) {
        text += "Fetching aggregated counters...\n";
    } else if (rows.is_discarded() || !rows.is_array() || rows.empty()) {
        text += "No aggregate sample is available yet.\n";
    } else {
        std::vector<nlohmann::json> values;
        for (auto& row : rows) values.push_back(row);
        std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
            int ae = a.value("run_ends", 0), be = b.value("run_ends", 0);
            if (ae != be) return ae > be;
            return a.value("avg_gap", 999.0) < b.value("avg_gap", 999.0);
        });
        text += "Watch list: death association, then short repeat gaps.\n";
        for (int i = 0; i < (int)values.size() && i < 8; i++) {
            const auto& row = values[i];
            text += row.value("event", "unknown") + " choice " +
                    std::to_string(row.value("choice", 0) + 1) + ": n " +
                    std::to_string(row.value("n", 0)) + ", ends " +
                    std::to_string(row.value("run_ends", 0)) + ", gap " +
                    std::to_string((int)row.value("avg_gap", 999.0)) + ".\n";
        }
    }
#else
    text += "Global counters are available in the browser build.\n";
#endif
    text += "\nThis screen contains aggregate card IDs and counters only. It has no names, seeds, free text, or individual runs.";
    int readerSize = profile_.largeText ? 12 : 10;
    drawScrollText(WrapLines(text, kW - 24, readerSize), 8, 22, kH - 20,
                   PAL_INK, mouse, false);
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_ESCAPE)) {
        textScroll_ = 0;
        screen_ = OPTIONS;
    }
}

void Game::drawInfo(Vector2 mouse) {
    drawTopBar();
    // Books, contracts, and long excerpts scroll instead of clipping (R9b).
    int readerSize = profile_.largeText ? 12 : 10;
    bool scrolled = drawScrollText(WrapLines(infoText_, kW - 24, readerSize), 8, 22, kH - 18,
                                   PAL_INK, mouse, false);
    const char* prompt = "[Enter]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (!scrolled && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        IsKeyPressed(KEY_TAB) || pressed_)) {
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
    bool hasShip = false;
    for (auto& item : ch_.pack)
        if (item.type == "ship") hasShip = true;
    int hovered = -1;
    for (int i = 0; i < n; i++) {
        bool seen = visitedRegions_.count(i) > 0;
        Color c = seen ? biomeColor(world_.regions[i].biome) : Color{58, 54, 70, 255};
        DrawRectangle((int)pos[i].x - 3, (int)pos[i].y - 3, 6, 6, c);
        if (i == currentRegion_) {
            DrawRectangleLines((int)pos[i].x - 5, (int)pos[i].y - 5, 10, 10, PAL_GOLD);
        }
        // A living named beast lairs here, and you've walked this land (R8).
        if (seen)
            for (auto& b : history_.beasts)
                if (b.died < 0 && b.region == i)
                    DrawRectangle((int)pos[i].x - 1, (int)pos[i].y - 8, 3, 3, PAL_RED);
        // With a ship, every coast is a possible landfall.
        if (hasShip && world_.regions[i].biome == "coast")
            DrawRectangle((int)pos[i].x - 3, (int)pos[i].y + 5, 6, 2,
                          Color{92, 128, 168, 255});
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
        // Second line: the sites you know of there (R8).
        if (seen) {
            std::string sl;
            if (label < (int)regionStates_.size())
                sl = regionStates_[label].description() + ". ";
            if (!r.sites.empty()) sl += "Sites: ";
            for (int si : r.sites) {
                if (sl.size() >= 2 && sl.substr(sl.size() - 2) != ": ") sl += ", ";
                sl += world_.sites[si].name;
            }
            while (!sl.empty() && MeasureText((sl + "..").c_str(), 10) > kW - 12)
                sl.pop_back();
            if (!sl.empty())
                DrawText(sl.c_str(), (kW - MeasureText(sl.c_str(), 10)) / 2, kH - 42,
                         10, PAL_DIM);
        }
    }
    std::string tally = std::to_string((int)visitedRegions_.size()) + "/" +
                        std::to_string(n) + " walked   red: lair" +
                        (hasShip ? "   blue: landfall" : "");
    while (!tally.empty() && MeasureText(tally.c_str(), 10) > kW - 64) tally.pop_back();
    DrawText(tally.c_str(), 8, kH - 16, 10, PAL_DARK);
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16}, "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        screen_ = returnScreen_;
    }
}

// Page through the whole thousand-year Chronicle: era jumps, latest,
// and follow-the-thread filters by figure or faction (R3, R10).
void Game::drawChronicle(Vector2 mouse) {
    const int kPerPage = 8;
    bool entityFiltered = chronFilterActor_ >= 0 || chronFilterFaction_ >= 0 ||
                          chronFilterSite_ >= 0 || chronFilterArtifact_ >= 0 ||
                          chronFilterBeast_ >= 0;
    bool topicFiltered = chronTopic_ > 0;
    bool filtered = entityFiltered || topicFiltered;
    int total = filtered ? (int)chronFilterList_.size() : (int)history_.chron.size();
    auto entryAt = [&](int i) -> int {
        return filtered ? chronFilterList_[i] : i;
    };
    int pages = total > 0 ? (total + kPerPage - 1) / kPerPage : 1;
    if (chronPage_ < 0) chronPage_ = 0;
    if (chronPage_ >= pages) chronPage_ = pages - 1;
    if (chronCachedPage_ != chronPage_) {
        chronLines_.clear();
        chronIdx_.clear();
        // A throwaway RNG: browsing must never disturb run determinism.
        Rng browse(masterSeed_ ^ 0xB00C5ULL, (uint64_t)(chronPage_ + 7));
        for (int i = chronPage_ * kPerPage;
             i < total && i < (chronPage_ + 1) * kPerPage; i++) {
            const ChronEntry& e = history_.chron[entryAt(i)];
            std::string line = "y" + std::to_string(e.year) + "  " +
                RenderChronEntry(e, history_, world_, grammar_, browse);
            chronLines_.push_back(line);
            chronIdx_.push_back(entryAt(i));
        }
        chronCachedPage_ = chronPage_;
    }
    std::string head = "CHRONICLE: " + world_.name.conlang;
    for (auto& c : head) c = (char)toupper((unsigned char)c);
    bool headCut = false;
    while (!head.empty() && MeasureText((head + "..").c_str(), 10) > kW - 160) {
        head.pop_back();
        headCut = true;
    }
    if (headCut) head += "..";
    DrawText(head.c_str(), 4, 6, 10, PAL_GOLD);
    // A tapped line opens the whole entry, unclipped (R9), with a THREAD
    // button that filters the whole book to that figure or faction (R10).
    if (chronDetail_ >= 0 && chronDetail_ < (int)chronLines_.size()) {
        DrawTextWrapped(chronLines_[chronDetail_], 12, 30, kW - 24, PAL_INK, kH - 40);
        const ChronEntry& de = history_.chron[chronIdx_[chronDetail_]];
        if (!filtered && (de.actor >= 0 || de.faction >= 0 || de.site >= 0 ||
                          de.artifact >= 0 || de.beast >= 0) &&
            uiButton({(float)(kW / 2 - 52), (float)(kH - 22), 104, 18},
                     "FOLLOW THIS THREAD", mouse)) {
            chronFilterActor_ = de.actor;
            chronTopic_ = 0;
            chronFilterArtifact_ = de.actor < 0 ? de.artifact : -1;
            chronFilterBeast_ = de.actor < 0 && de.artifact < 0 ? de.beast : -1;
            chronFilterSite_ = de.actor < 0 && de.artifact < 0 && de.beast < 0
                ? de.site : -1;
            chronFilterFaction_ = de.actor < 0 && de.artifact < 0 && de.beast < 0 &&
                                  de.site < 0 ? de.faction : -1;
            chronFilterList_.clear();
            for (int i = 0; i < (int)history_.chron.size(); i++) {
                const ChronEntry& e = history_.chron[i];
                bool hit = (chronFilterActor_ >= 0 && e.actor == chronFilterActor_) ||
                           (chronFilterFaction_ >= 0 &&
                            (e.faction == chronFilterFaction_ ||
                             e.faction2 == chronFilterFaction_)) ||
                           (chronFilterSite_ >= 0 && e.site == chronFilterSite_) ||
                           (chronFilterArtifact_ >= 0 && e.artifact == chronFilterArtifact_) ||
                           (chronFilterBeast_ >= 0 && e.beast == chronFilterBeast_);
                if (hit) chronFilterList_.push_back(i);
            }
            chronPage_ = 0;
            chronCachedPage_ = -1;
            chronDetail_ = -1;
            return;
        }
        const char* hint = "tap to return";
        DrawText(hint, 8, kH - 16, 10, PAL_DARK);
        if (pressed_ || GetKeyPressed() != 0) chronDetail_ = -1;
        return;
    }
    std::string era;
    if (!chronLines_.empty()) {
        if (filtered) {
            std::string who;
            if (topicFiltered) {
                static const char* kTopicNames[4] = {
                    "all history", "player lives", "conflict", "civic change"};
                era = std::string("topic: ") + kTopicNames[chronTopic_] + "  -  " +
                      std::to_string(total) + " entries";
            } else if (chronFilterActor_ >= 0) who = history_.figures[chronFilterActor_].name;
            else if (chronFilterFaction_ >= 0) who = history_.factions[chronFilterFaction_].name;
            else if (chronFilterSite_ >= 0) who = world_.sites[chronFilterSite_].name;
            else if (chronFilterArtifact_ >= 0) who = history_.artifacts[chronFilterArtifact_].display();
            else if (chronFilterBeast_ >= 0) who = history_.beasts[chronFilterBeast_].name;
            if (!topicFiltered)
                era = "the thread of " + who + "  -  " + std::to_string(total) + " entries";
        } else {
            era = std::string(EraName(history_.chron[entryAt(chronPage_ * kPerPage)].year)) +
                  "  -  page " + std::to_string(chronPage_ + 1) + "/" + std::to_string(pages);
        }
    }
    while (!era.empty() && MeasureText(era.c_str(), 10) > kW - 12) era.pop_back();
    DrawText(era.c_str(), (kW - MeasureText(era.c_str(), 10)) / 2, 18, 10, PAL_DIM);
    // Era jump row: the five ages plus NOW (R10). Hidden inside a thread.
    if (!filtered) {
        static const int kEraStart[6] = {0, 251, 451, 651, 751, -1};
        static const char* kEraBtn[6] = {"I", "II", "III", "IV", "V", "NOW"};
        for (int b = 0; b < 6; b++) {
            if (uiButton({(float)(kW - 152 + b * 25), 2, 22, 13}, kEraBtn[b], mouse)) {
                if (kEraStart[b] < 0) {
                    chronPage_ = pages - 1;
                } else {
                    int idx = 0;
                    for (int i = 0; i < (int)history_.chron.size(); i++)
                        if (history_.chron[i].year >= kEraStart[b]) { idx = i; break; }
                    chronPage_ = idx / kPerPage;
                }
                chronCachedPage_ = -1;
                return;
            }
        }
    }
    int y = 32;
    for (int i = 0; i < (int)chronLines_.size(); i++) {
        const std::string& line = chronLines_[i];
        std::string l = line;
        bool clipped = false;
        while (!l.empty() && MeasureText((l + "..").c_str(), 10) > kW - 12) {
            l.pop_back();
            clipped = true;
        }
        if (clipped) l += "..";
        Rectangle row{4, (float)(y - 1), kW - 8, 12};
        bool hover = CheckCollisionPointRec(mouse, row);
        bool playerMade = history_.chron[chronIdx_[i]].type.rfind("pc_", 0) == 0;
        DrawText(l.c_str(), 6, y, 10, hover ? PAL_GOLD : (playerMade ? PAL_GREEN : PAL_INK));
        if (hover && pressed_) {
            audio_.blip();
            chronDetail_ = i;
            return;
        }
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
    static const char* kTopicButtons[4] = {"TOPIC: ALL", "TOPIC: YOU", "TOPIC: WAR", "TOPIC: CIVIC"};
    if (!entityFiltered && uiButton({108, (float)(kH - 20), 100, 16},
                                    kTopicButtons[chronTopic_], mouse)) {
        chronTopic_ = (chronTopic_ + 1) % 4;
        chronFilterList_.clear();
        if (chronTopic_ > 0) {
            for (int i = 0; i < (int)history_.chron.size(); i++) {
                const std::string& type = history_.chron[i].type;
                bool hit = false;
                if (chronTopic_ == 1)
                    hit = type.rfind("pc_", 0) == 0 || type.find("companion") != std::string::npos;
                else if (chronTopic_ == 2)
                    hit = type.find("war") != std::string::npos ||
                          type.find("battle") != std::string::npos ||
                          type.find("siege") != std::string::npos ||
                          type.find("killed") != std::string::npos ||
                          type.find("slain") != std::string::npos;
                else if (chronTopic_ == 3)
                    hit = type == "region_shift" || type.find("agenda_") == 0 ||
                          type.find("founded") != std::string::npos ||
                          type.find("reform") != std::string::npos ||
                          type.find("trade") != std::string::npos ||
                          type.find("refugee") != std::string::npos ||
                          type.find("plague") != std::string::npos;
                if (hit) chronFilterList_.push_back(i);
            }
        }
        chronPage_ = 0;
        chronCachedPage_ = -1;
        chronDetail_ = -1;
        return;
    }
    if (uiButton({(float)(kW - 52), (float)(kH - 20), 48, 16},
                 filtered ? "ALL" : "BACK", mouse) ||
        IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        if (filtered) {
            // Leave the thread, back to the whole book.
            chronFilterActor_ = chronFilterFaction_ = -1;
            chronFilterSite_ = chronFilterArtifact_ = chronFilterBeast_ = -1;
            chronTopic_ = 0;
            chronFilterList_.clear();
            chronPage_ = 0;
            chronCachedPage_ = -1;
        } else {
            screen_ = TITLE;
        }
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
            nlohmann::json relations = nlohmann::json::object();
            for (auto& [fig, rel] : npcRelations_)
                relations[std::to_string(fig)] = {
                    {"trust", rel.trust}, {"fear", rel.fear},
                    {"respect", rel.respect}, {"debt", rel.debt},
                    {"affection", rel.affection}, {"grudge", rel.grudge},
                    {"knowledge", rel.knowledge}, {"lastSeen", rel.lastSeen}};
            SaveMarks(masterSeed_, nlohmann::json{{"marks", marks},
                                                   {"relations", relations}}.dump());
            clearRun();
#if defined(__EMSCRIPTEN__)
            // Shared world? Your epitaph joins the fallen.
            int bk = boardKeyFor(masterSeed_);
            if (bk >= 0 && !scoreSubmitted_) {
                scoreSubmitted_ = true;
                nlohmann::json relics = nlohmann::json::array();
                for (auto& [rn, rq] : rec.relics)
                    relics.push_back({{"name", rn}, {"quirk", rq}});
                nlohmann::json steps = nlohmann::json::array();
                for (auto& st : journey_)
                    steps.push_back({{"d", st.day}, {"s", st.site},
                                     {"c", st.choice}, {"o", st.outcome}});
                nlohmann::json s = {{"day", bk}, // the board this world belongs to
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
