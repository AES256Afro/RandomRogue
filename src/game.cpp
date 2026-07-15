#include "game.h"
#include <ctime>
#include <sstream>

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

bool anyKeyPressed() {
    return GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

} // namespace

bool Game::init() {
    struct Loader { const char* path; void (*fn)(Game*, const char*); };
    auto loadText = [&](const char* path, auto&& fn) {
        char* text = LoadFileText(path);
        if (text) { fn(text); UnloadFileText(text); }
    };
    loadText("assets/data/recipes/prose.json", [&](const char* t) { grammar_.loadJsonText(t); });
    loadText("assets/data/recipes/language.json", [&](const char* t) { forge_.loadJsonText(t); });
    loadText("assets/data/events.json", [&](const char* t) { deck_.loadJsonText(t); });
    loadText("assets/data/items.json", [&](const char* t) { items_.loadItemsJsonText(t); });
    loadText("assets/data/quirks.json", [&](const char* t) { items_.loadQuirksJsonText(t); });
    if (deck_.size() == 0) dataError_ = "content missing: assets/data/events.json";
    if (items_.size() == 0) dataError_ = "content missing: assets/data/items.json";
    return dataError_.empty();
}

void Game::newRun() {
    masterSeed_ = (uint64_t)time(nullptr) * 2654435761u + (++runCounter_) * 40503u;
    runRng_ = Rng(masterSeed_, STREAM_RUNTIME);
    Rng langRng(masterSeed_, STREAM_LANG);
    world_ = GenerateWorld(masterSeed_, grammar_, forge_);
    history_ = SimulateHistory(world_, masterSeed_, grammar_, forge_);
    ch_ = Character::roll(langRng, forge_);
    ch_.pack.push_back(items_.make("rations", runRng_));
    ch_.pack.push_back(items_.make(runRng_.chance(50) ? "rusty_sword" : "club", runRng_));
    deck_.resetUsed();
    pendingArtifact_ = -1;
    enterTravel();
}

void Game::enterTravel() {
    screen_ = TRAVEL;
    travelOptions_.clear();
    std::vector<int> pool;
    for (int i = 0; i < (int)world_.sites.size(); i++) pool.push_back(i);
    for (int n = 0; n < 3 && !pool.empty(); n++) {
        int pi = runRng_.range(0, (int)pool.size() - 1);
        const Site& s = world_.sites[pool[pi]];
        pool.erase(pool.begin() + pi);
        TravelOption o;
        o.deck = s.deck;
        o.siteName = s.name;
        o.label = s.name + "  (" + s.type + ", " + world_.regions[s.region].name + ")";
        travelOptions_.push_back(o);
    }
    TravelOption wander;
    wander.deck = runRng_.chance(50) ? "road" : "forest";
    wander.siteName = "the wilds";
    wander.label = "Wander off in a random direction";
    travelOptions_.push_back(wander);
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
        } else if (query == "figure_dead") {
            std::vector<int> pool;
            for (int i = 0; i < (int)history_.figures.size(); i++)
                if (history_.figures[i].died >= 0) pool.push_back(i);
            if (pool.empty()) return false;
            ctx[name] = history_.figures[pool[runRng_.range(0, (int)pool.size() - 1)]].name;
        } else {
            return false; // unknown query: skip the event, loudly absent
        }
    }
    return true;
}

void Game::dealEvent() {
    // A few tries to find an event whose slots can be satisfied here.
    for (int attempt = 0; attempt < 6; attempt++) {
        current_ = deck_.draw(runRng_, deckTag_);
        if (!current_) { enterTravel(); return; }
        Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
        pendingArtifact_ = -1;
        if (!resolveSlots(*current_, ctx)) continue;
        currentText_ = grammar_.expand(current_->text, runRng_, ctx);
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
            }
            ch_.hp += v;
            if (ch_.hp > ch_.maxHp) ch_.hp = ch_.maxHp;
        } else if (verb == "maxhp") {
            int v = 0; ss >> v;
            ch_.maxHp += v;
            if (ch_.hp > ch_.maxHp) ch_.hp = ch_.maxHp;
        } else if (verb == "money") {
            int v = 0; ss >> v;
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
            if ((int)ch_.pack.size() < Character::kPackMax)
                ch_.pack.push_back(items_.make(id, runRng_));
        } else if (verb == "loot") {
            std::string tier; ss >> tier;
            if ((int)ch_.pack.size() < Character::kPackMax)
                ch_.pack.push_back(items_.loot(runRng_, tier));
        } else if (verb == "removeitem") {
            std::string id; ss >> id;
            ch_.removeItem(id);
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
        } else if (verb == "die") {
            std::string rest;
            std::getline(ss, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            ch_.dead = true;
            if (!rest.empty()) ch_.epitaph = rest;
        }
    }
    if (ch_.hp <= 0) ch_.dead = true;
}

void Game::chooseOption(int idx) {
    if (!current_ || idx < 0 || idx >= (int)current_->choices.size()) return;
    const Choice& choice = current_->choices[idx];
    if (!choice.requires_.met(ch_)) return;
    outcome_ = EventDeck::resolve(choice, ch_, runRng_);
    Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
    outcome_.text = grammar_.expand(outcome_.text, runRng_, ctx);
    applyEffects(outcome_.effects);
    ch_.eventsSurvived++;
    if (ch_.dead && ch_.epitaph.empty())
        ch_.epitaph = grammar_.expand("{epitaph}", runRng_, ctx);
    screen_ = OUTCOME;
}

void Game::openInventory() {
    returnScreen_ = screen_;
    invSelected_ = -1;
    screen_ = INVENTORY;
}

void Game::showInfo(const std::string& text) {
    infoText_ = text;
    screen_ = INFO;
}

void Game::frame(Vector2 mouse) {
    ClearBackground(PAL_BG);
    switch (screen_) {
        case TITLE:     drawTitle(); break;
        case TRAVEL:    drawTravel(mouse); break;
        case EVENT:     drawEvent(mouse); break;
        case OUTCOME:   drawOutcome(); break;
        case DEATH:     drawDeath(); break;
        case INVENTORY: drawInventory(mouse); break;
        case INFO:      drawInfo(); break;
    }
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
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
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

void Game::drawTitle() {
    const char* title = "RANDOM ROGUE";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 48, 20, PAL_GOLD);
    if (!dataError_.empty()) {
        DrawTextWrapped(dataError_, 20, 90, kW - 40, PAL_RED);
        return;
    }
    if (((long)(GetTime() * 2)) % 2 == 0) {
        const char* prompt = "press any key to be born";
        DrawText(prompt, (kW - MeasureText(prompt, 10)) / 2, 96, 10, PAL_DIM);
    }
    const char* sub = "a game of poor decisions";
    DrawText(sub, (kW - MeasureText(sub, 10)) / 2, 74, 10, PAL_DARK);
    if (anyKeyPressed()) newRun();
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
    DrawText("Where to?", 8, y + 4, 10, PAL_GOLD);
    DrawText("Tab: pack", kW - MeasureText("Tab: pack", 10) - 4, y + 4, 10, PAL_DARK);

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& o : travelOptions_) { rows.push_back(o.label); ok.push_back(true); }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        siteName_ = travelOptions_[pick].siteName;
        deckTag_ = travelOptions_[pick].deck;
        currentSite_ = -1;
        for (int i = 0; i < (int)world_.sites.size(); i++)
            if (world_.sites[i].name == travelOptions_[pick].siteName) currentSite_ = i;
        eventsLeftHere_ = runRng_.range(2, 3);
        ch_.day++;
        dealEvent();
        return;
    }
    if (IsKeyPressed(KEY_TAB)) openInventory();
}

void Game::drawEvent(Vector2 mouse) {
    drawTopBar();
    DrawTextWrapped(currentText_, 8, 22, kW - 16, PAL_INK);

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& c : current_->choices) {
        rows.push_back(c.requires_.label() + c.text);
        ok.push_back(c.requires_.met(ch_));
    }
    int pick = optionRows(rows, ok, mouse);
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
        if (e.rfind("die", 0) != 0) fx += "[" + e + "] ";
    if (!fx.empty()) DrawTextWrapped(fx, 8, y + 4, kW - 16, PAL_DARK);

    const char* prompt = "[Enter]";
    DrawText(prompt, kW - MeasureText(prompt, 10) - 6, kH - 13, 10, PAL_DIM);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ch_.dead) { screen_ = DEATH; return; }
        eventsLeftHere_--;
        if (eventsLeftHere_ > 0) dealEvent();
        else enterTravel();
    }
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
        DrawText("Tab: back", kW - MeasureText("Tab: back", 10) - 4, kH - 13, 10, PAL_DARK);
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
            bool isBook = false;
            for (auto& u : item.useEffects)
                if (u == "read") isBook = true;
            if (isBook) {
                showInfo("You read from " + item.name + ":\n\n" + chronicleExcerpt(3));
                invSelected_ = -1;
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
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        screen_ = ch_.dead ? DEATH : INVENTORY;
    }
}

void Game::drawDeath() {
    const char* title = "YOU DIED";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 34, 20, PAL_RED);
    std::string who = ch_.name.conlang + " \"" + ch_.name.meaning + "\"";
    DrawText(who.c_str(), (kW - MeasureText(who.c_str(), 10)) / 2, 60, 10, PAL_INK);
    int y = DrawTextWrapped(ch_.epitaph, 30, 78, kW - 60, PAL_GOLD);
    std::string run = "Survived " + std::to_string(ch_.day) + " days and " +
                      std::to_string(ch_.eventsSurvived) + " questionable decisions. Died holding " +
                      std::to_string(ch_.money) + " gold.";
    DrawTextWrapped(run, 30, y + 6, kW - 60, PAL_DIM);
    if (((long)(GetTime() * 2)) % 2 == 0) {
        const char* prompt = "press any key";
        DrawText(prompt, (kW - MeasureText(prompt, 10)) / 2, kH - 20, 10, PAL_DARK);
    }
    if (anyKeyPressed()) screen_ = TITLE;
}
