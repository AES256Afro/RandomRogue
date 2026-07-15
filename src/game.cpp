#include "game.h"
#include <ctime>

namespace {

constexpr int kW = 320, kH = 180;

const Color PAL_BG      = {24, 20, 37, 255};
const Color PAL_INK     = {222, 222, 213, 255};
const Color PAL_GOLD    = {255, 205, 117, 255};
const Color PAL_DIM     = {139, 155, 180, 255};
const Color PAL_DARK = {90, 105, 136, 255};
const Color PAL_RED     = {231, 82, 86, 255};
const Color PAL_GREEN   = {126, 196, 93, 255};
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
    struct { const char* path; int loaded; } files[3] = {
        {"assets/data/recipes/prose.json", 0},
        {"assets/data/recipes/language.json", 0},
        {"assets/data/events.json", 0},
    };
    char* prose = LoadFileText(files[0].path);
    char* lang = LoadFileText(files[1].path);
    char* events = LoadFileText(files[2].path);
    if (prose) { grammar_.loadJsonText(prose); UnloadFileText(prose); }
    if (lang) { forge_.loadJsonText(lang); UnloadFileText(lang); }
    if (events) { deck_.loadJsonText(events); UnloadFileText(events); }
    if (deck_.size() == 0) dataError_ = "content missing: assets/data/events.json";
    return dataError_.empty();
}

void Game::newRun() {
    masterSeed_ = (uint64_t)time(nullptr) * 2654435761u + (++runCounter_) * 40503u;
    runRng_ = Rng(masterSeed_, STREAM_RUNTIME);
    Rng langRng(masterSeed_, STREAM_LANG);
    world_ = GenerateWorld(masterSeed_, grammar_, forge_);
    ch_ = Character::roll(langRng, forge_);
    deck_.resetUsed();
    enterTravel();
}

void Game::enterTravel() {
    screen_ = TRAVEL;
    travelOptions_.clear();
    // 3 distinct random sites + wander.
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

void Game::dealEvent() {
    current_ = deck_.draw(runRng_, deckTag_);
    if (!current_) { enterTravel(); return; }
    Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
    currentText_ = grammar_.expand(current_->text, runRng_, ctx);
    screen_ = EVENT;
}

void Game::chooseOption(int idx) {
    if (!current_ || idx < 0 || idx >= (int)current_->choices.size()) return;
    const Choice& choice = current_->choices[idx];
    if (!choice.requires_.met(ch_)) return;
    outcome_ = EventDeck::resolve(choice, ch_, runRng_);
    Grammar::Ctx ctx = {{"site", siteName_}, {"world", world_.name.conlang}};
    outcome_.text = grammar_.expand(outcome_.text, runRng_, ctx);
    EventDeck::apply(outcome_.effects, ch_);
    ch_.eventsSurvived++;
    if (ch_.dead && ch_.epitaph.empty())
        ch_.epitaph = grammar_.expand("{epitaph}", runRng_, ctx);
    screen_ = OUTCOME;
}

void Game::frame(Vector2 mouse) {
    ClearBackground(PAL_BG);
    switch (screen_) {
        case TITLE:   drawTitle(); break;
        case TRAVEL:  drawTravel(mouse); break;
        case EVENT:   drawEvent(mouse); break;
        case OUTCOME: drawOutcome(); break;
        case DEATH:   drawDeath(); break;
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
    for (int i = 0; i < n && i < 4; i++)
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
    // Right-aligned site name, truncated so it never collides with the day.
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

    std::vector<std::string> rows;
    std::vector<bool> ok;
    for (auto& o : travelOptions_) { rows.push_back(o.label); ok.push_back(true); }
    int pick = optionRows(rows, ok, mouse);
    if (pick >= 0) {
        siteName_ = travelOptions_[pick].siteName;
        deckTag_ = travelOptions_[pick].deck;
        eventsLeftHere_ = runRng_.range(2, 3);
        ch_.day++;
        dealEvent();
    }
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
    if (pick >= 0) chooseOption(pick);
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
    // Effect readout so consequences are legible.
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

void Game::drawDeath() {
    const char* title = "YOU DIED";
    DrawText(title, (kW - MeasureText(title, 20)) / 2, 34, 20, PAL_RED);
    std::string who = ch_.name.conlang + " \"" + ch_.name.meaning + "\"";
    DrawText(who.c_str(), (kW - MeasureText(who.c_str(), 10)) / 2, 60, 10, PAL_INK);
    int y = DrawTextWrapped(ch_.epitaph, 30, 78, kW - 60, PAL_GOLD);
    std::string run = "Survived " + std::to_string(ch_.day) + " days and " +
                      std::to_string(ch_.eventsSurvived) + " questionable decisions. Died holding " +
                      std::to_string(ch_.money) + " PAL_GOLD.";
    DrawTextWrapped(run, 30, y + 6, kW - 60, PAL_DIM);
    if (((long)(GetTime() * 2)) % 2 == 0) {
        const char* prompt = "press any key";
        DrawText(prompt, (kW - MeasureText(prompt, 10)) / 2, kH - 20, 10, PAL_DARK);
    }
    if (anyKeyPressed()) screen_ = TITLE;
}
