#include "game.h"
#include <ctime>
#include <sstream>
#include <nlohmann/json.hpp>

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
    currentRegion_ = runRng_.range(0, (int)world_.regions.size() - 1);
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
    wander.deck = (biome == "forest" || biome == "swamp") ? "forest" : "road";
    wander.siteName = "the wilds";
    wander.label = "Wander " + world_.regions[currentRegion_].name;
    travelOptions_.push_back(wander);
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
        } else if (query == "god") {
            if (history_.gods.empty()) return false;
            const God& god = history_.gods[runRng_.range(0, (int)history_.gods.size() - 1)];
            ctx[name] = god.name;
            ctx[name + "_domain"] = god.domain;
            ctx[name + "_mood"] = god.mood > 0 ? "generous" : (god.mood < 0 ? "wrathful" : "indifferent");
        } else if (query == "figure_dead") {
            std::vector<int> pool;
            for (int i = 0; i < (int)history_.figures.size(); i++)
                if (history_.figures[i].died >= 0) pool.push_back(i);
            if (pool.empty()) return false;
            const Figure& f = history_.figures[pool[runRng_.range(0, (int)pool.size() - 1)]];
            ctx[name] = f.name;
            ctx[name + "_trait"] = f.trait;
            ctx[name + "_year"] = std::to_string(f.died);
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
    if (deckTag_ == "dungeon" && eventsLeftHere_ == 1) tag = "dungeon_finale";
    for (int attempt = 0; attempt < 6; attempt++) {
        current_ = deck_.draw(runRng_, tag);
        if (!current_ && tag == "dungeon_finale") { tag = "dungeon"; continue; }
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
        } else if (verb == "die") {
            std::string rest;
            std::getline(ss, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            ch_.dead = true;
            if (!rest.empty()) ch_.epitaph = rest;
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
    screen_ = OUTCOME;
}

void Game::continueAfterOutcome() {
    if (ch_.dead) { screen_ = DEATH; return; }
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

    ClearBackground(PAL_BG);
    switch (screen_) {
        case TITLE:     drawTitle(mouse); break;
        case CLASSPICK: drawClassPick(mouse); break;
        case AMBITION:  drawAmbitionPick(mouse); break;
        case TRAVEL:    drawTravel(mouse); break;
        case EVENT:     drawEvent(mouse); break;
        case OUTCOME:   drawOutcome(); break;
        case DEATH:     drawDeath(); break;
        case INVENTORY: drawInventory(mouse); break;
        case INFO:      drawInfo(); break;
        case VENDOR:    drawVendor(mouse); break;
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
    bool play = uiButton({kW / 2 - 90, 122, 84, 20}, "BEGIN", mouse);
    bool reroll = uiButton({kW / 2 + 6, 122, 84, 20}, "REROLL SEED", mouse);
    std::string runs = "deaths: " + std::to_string(profile_.deaths) +
                       "   best run: " + std::to_string(profile_.bestDays) + " days";
    DrawText(runs.c_str(), (kW - MeasureText(runs.c_str(), 10)) / 2, kH - 16, 10, PAL_DARK);

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
    std::string head = "You are " + ch_.name.conlang + " \"" + ch_.name.meaning +
                       "\", adrift in the world of " + world_.name.conlang + ", " +
                       world_.name.meaning + ".";
    int y = DrawTextWrapped(head, 8, 22, kW - 16, PAL_INK);
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

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& o : travelOptions_) { rows.push_back(o.label); ok.push_back(true); }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        siteName_ = travelOptions_[pick].siteName;
        deckTag_ = travelOptions_[pick].deck;
        currentSite_ = travelOptions_[pick].site;
        if (currentSite_ >= 0) currentRegion_ = world_.sites[currentSite_].region;
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

void Game::drawDeath() {
    const char* title = "YOU DIED";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 30, 20, PAL_RED);
    std::string who = ch_.name.conlang + " \"" + ch_.name.meaning + "\"";
    DrawText(who.c_str(), (kW - MeasureText(who.c_str(), 10)) / 2, 56, 10, PAL_INK);
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
        profile_.deaths++;
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
            for (auto& item : ch_.pack) {
                if (rec.relics.size() >= 2) break;
                if (item.artifactId >= 0 || item.type == "weapon" || item.type == "armor")
                    rec.relics.emplace_back(item.name, item.quirk);
            }
            AppendLegacy(masterSeed_, rec);
            legacySaved_ = true;
            cachedLegacySeed_ = ~0ULL; // force reload next time
        }
        // Same world by default: go back in, years later, among your ghosts.
        nextSeed_ = masterSeed_;
        screen_ = TITLE;
    }
}
