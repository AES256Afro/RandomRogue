#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "character.h"
#include "events.h"
#include "grammar.h"
#include "language.h"
#include "world.h"

class Game {
public:
    bool init(); // load data; returns false if content missing
    // Draw + input for one frame. Called inside the virtual-canvas texture
    // mode; mouse is already in virtual (320x180) coordinates.
    void frame(Vector2 mouse);

private:
    enum Screen { TITLE, TRAVEL, EVENT, OUTCOME, DEATH };

    void newRun();
    void enterTravel();
    void dealEvent();
    void chooseOption(int idx);

    void drawTitle();
    void drawTravel(Vector2 mouse);
    void drawEvent(Vector2 mouse);
    void drawOutcome();
    void drawDeath();
    void drawTopBar();
    // Draws numbered option rows anchored to the card bottom; returns clicked
    // or key-pressed index, -1 if none. Disabled entries render dim.
    int optionRows(const std::vector<std::string>& rows,
                   const std::vector<bool>& enabled, Vector2 mouse);

    Screen screen_ = TITLE;
    Grammar grammar_;
    NameForge forge_;
    EventDeck deck_;
    World world_;
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
    int eventsLeftHere_ = 0;
    ResolvedOutcome outcome_;
};
