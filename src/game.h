#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "character.h"
#include "events.h"
#include "grammar.h"
#include "history.h"
#include "items.h"
#include "language.h"
#include "world.h"

class Game {
public:
    bool init(); // load data; returns false if content missing
    // Draw + input for one frame. Called inside the virtual-canvas texture
    // mode; mouse is already in virtual (320x180) coordinates.
    void frame(Vector2 mouse);

private:
    enum Screen { TITLE, TRAVEL, EVENT, OUTCOME, DEATH, INVENTORY, INFO };

    void newRun();
    void enterTravel();
    void dealEvent();
    bool resolveSlots(const Event& e, Grammar::Ctx& ctx);
    void chooseOption(int idx);
    void applyEffects(const std::vector<std::string>& effects);
    std::string randomRumor();
    std::string chronicleExcerpt(int entries);
    void openInventory();
    void showInfo(const std::string& text);

    void drawTitle();
    void drawTravel(Vector2 mouse);
    void drawEvent(Vector2 mouse);
    void drawOutcome();
    void drawDeath();
    void drawInventory(Vector2 mouse);
    void drawInfo();
    void drawTopBar();
    int optionRows(const std::vector<std::string>& rows,
                   const std::vector<bool>& enabled, Vector2 mouse);

    Screen screen_ = TITLE;
    Grammar grammar_;
    NameForge forge_;
    EventDeck deck_;
    ItemDb items_;
    World world_;
    History history_;
    Character ch_;
    Rng runRng_;
    uint64_t masterSeed_ = 0;
    unsigned runCounter_ = 0;
    std::string dataError_;

    // travel
    struct TravelOption { std::string label, deck, siteName; };
    std::vector<TravelOption> travelOptions_;

    // current event
    const Event* current_ = nullptr;
    std::string currentText_;
    std::string siteName_;
    std::string deckTag_;
    int currentSite_ = -1; // world site index, -1 when wandering
    int eventsLeftHere_ = 0;
    ResolvedOutcome outcome_;
    int pendingArtifact_ = -1; // artifact bound by an artifact_here slot

    // inventory / info
    Screen returnScreen_ = TRAVEL;
    int invSelected_ = -1;
    std::string infoText_;
};
