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

// Wrap by words; honors '\n'. Returns y after the last line.
int DrawTextWrapped(const std::string& text, int x, int y, int width, Color color) {
    const int fs = 10, lineH = 11;
    std::string line;
    auto flush = [&]() {
        if (!line.empty()) DrawText(line.c_str(), x, y, fs, color);
        y += lineH;
        line.clear();
    };
    std::string word;
    for (size_t i = 0; i <= text.size(); i++) {
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
    if (!line.empty()) flush();
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
    // Events live in per-deck files listed by a manifest.
    loadText("assets/data/events/manifest.json", [&](const char* t) {
        nlohmann::json m = nlohmann::json::parse(t, nullptr, false);
        if (m.is_discarded() || !m.contains("files")) return;
        for (auto& f : m["files"])
            if (f.is_string())
                loadText("assets/data/events/" + f.get<std::string>(),
                         [&](const char* et) { deck_.loadJsonText(et); });
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
    return {
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

void Game::newRun(int classIdx) {
    masterSeed_ = nextSeed_;
    runCounter_++;
    runRng_ = Rng(masterSeed_, STREAM_RUNTIME);
    Rng langRng(masterSeed_, STREAM_LANG);
    world_ = GenerateWorld(masterSeed_, grammar_, forge_);
    history_ = SimulateHistory(world_, masterSeed_, grammar_, forge_);
    rep_.assign(history_.factions.size(), 0);
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
    enterTravel();
}

void Game::enterTravel() {
    screen_ = TRAVEL;
    travelOptions_.clear();
    std::vector<int> pool;
    for (int i = 0; i < (int)world_.sites.size(); i++) pool.push_back(i);
    for (int n = 0; n < 3 && !pool.empty(); n++) {
        int pi = runRng_.range(0, (int)pool.size() - 1);
        int si = pool[pi];
        const Site& s = world_.sites[si];
        pool.erase(pool.begin() + pi);
        TravelOption o;
        o.deck = s.deck;
        o.siteName = s.name;
        o.site = si;
        o.label = s.name + "  (" + s.type + ", " + world_.regions[s.region].name + ")";
        travelOptions_.push_back(o);
    }
    TravelOption wander;
    wander.deck = runRng_.chance(50) ? "road" : "forest";
    wander.siteName = "the wilds";
    wander.label = "Wander off in a random direction";
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
    const ChronEntry& e = history_.chron[runRng_.range(0, (int)history_.chron.size() - 1)];
    std::string rumor = RenderChronEntry(e, history_, world_, grammar_, runRng_);
    if (runRng_.chance(15))
        rumor += " (At least, that's the version going around.)";
    return rumor;
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
            if ((int)ch_.pack.size() < Character::kPackMax) {
                ch_.pack.push_back(makeItem(id));
                audio_.chime();
            }
        } else if (verb == "loot") {
            std::string tier; ss >> tier;
            if ((int)ch_.pack.size() < Character::kPackMax) {
                ItemInstance item = items_.loot(runRng_, tier);
                bindQuirk(item);
                ch_.pack.push_back(item);
                audio_.chime();
            }
        } else if (verb == "removeitem") {
            std::string id; ss >> id;
            ch_.removeItem(id);
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
            if (pendingArtifact_ >= 0 && (int)ch_.pack.size() < Character::kPackMax) {
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

    std::string seedLine = "world seed: " + std::to_string(nextSeed_);
    DrawText(seedLine.c_str(), (kW - MeasureText(seedLine.c_str(), 10)) / 2, 88, 10, PAL_DIM);
    const char* hint = "S: set seed   M: mute";
    DrawText(hint, (kW - MeasureText(hint, 10)) / 2, 102, 10, PAL_DARK);

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
        newRun(pick);
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
        eventsLeftHere_ = (deckTag_ == "dungeon") ? runRng_.range(3, 4) : runRng_.range(2, 3);
        ch_.day++;
        dealEvent();
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
    DrawTextWrapped(currentText_.substr(0, (size_t)reveal_), 8, 22, kW - 16, PAL_INK);

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
        y = DrawTextWrapped(outcome_.rollText, 8, y, kW - 16, success ? PAL_GREEN : PAL_RED);
        y += 4;
    }
    y = DrawTextWrapped(outcome_.text, 8, y, kW - 16, PAL_INK);
    std::string fx;
    for (auto& e : outcome_.effects)
        if (e.rfind("die", 0) != 0 && e != "shop" && e.rfind("goto", 0) != 0)
            fx += "[" + e + "] ";
    if (!fx.empty()) y = DrawTextWrapped(fx, 8, y + 4, kW - 16, PAL_DARK);
    if (blessingSpent_)
        DrawTextWrapped("The blessing spends itself. You live. Somewhere, a ledger updates.",
                        8, y + 4, kW - 16, PAL_GOLD);

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
            ok.push_back(ch_.money >= price && (int)ch_.pack.size() < Character::kPackMax);
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
                           std::to_string(Character::kPackMax) + ")";
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
    DrawTextWrapped(infoText_, 8, 22, kW - 16, PAL_INK);
    const char* prompt = "[Enter]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_TAB) ||
        pressed_) {
        screen_ = ch_.dead ? DEATH : INVENTORY;
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
        nextSeed_ = (masterSeed_ * 2654435761ULL + runCounter_ * 977ULL) % 1000000000ULL;
        screen_ = TITLE;
    }
}
