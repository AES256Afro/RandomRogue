#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "audio.h"
#include "character.h"
#include "events.h"
#include "grammar.h"
#include "history.h"
#include "items.h"
#include "language.h"
#include "save.h"
#include "world.h"

class Game {
public:
    bool init(); // load data; returns false if content missing
    void shutdown();
    // Draw + input for one frame. Called inside the virtual-canvas texture
    // mode; mouse is already in virtual (320x180) coordinates. `pressed` is
    // the unified click/tap edge from the platform layer.
    void frame(Vector2 mouse, bool pressed);
    // Pixels of screen shake main.cpp should apply to the canvas blit.
    float shakeAmount() const { return shake_ > 0.0f ? shake_ : 0.0f; }

private:
    enum Screen { TITLE, CLASSPICK, TRAVEL, EVENT, OUTCOME, DEATH, INVENTORY, INFO, VENDOR };

    struct StartClass {
        const char* name;
        const char* blurb;
        const char* lockHint; // requirement text when locked
        bool unlocked;
    };
    std::vector<StartClass> startClasses() const;

    void newRun(int classIdx);
    ItemInstance makeItem(const std::string& id);
    void bindQuirk(ItemInstance& item);
    void enterTravel();
    void dealEvent();
    bool resolveSlots(const Event& e, Grammar::Ctx& ctx);
    void chooseOption(int idx);
    void applyEffects(const std::vector<std::string>& effects);
    void continueAfterOutcome();
    std::string randomRumor();
    std::string chronicleExcerpt(int entries);
    void openInventory();
    void openVendor();
    void showInfo(const std::string& text);
    int regionOwner(int region) const; // faction index or -1
    int localRep() const;
    int buyPrice(const ItemInstance& item) const;
    int sellPrice(const ItemInstance& item) const;

    void drawTitle(Vector2 mouse);
    void drawClassPick(Vector2 mouse);
    void drawTravel(Vector2 mouse);
    void drawEvent(Vector2 mouse);
    void drawOutcome();
    void drawDeath();
    void drawInventory(Vector2 mouse);
    void drawInfo();
    void drawVendor(Vector2 mouse);
    void drawTopBar();
    int optionRows(const std::vector<std::string>& rows,
                   const std::vector<bool>& enabled, Vector2 mouse);
    // Touch-friendly clickable button; returns true on click/tap.
    bool uiButton(Rectangle r, const char* label, Vector2 mouse);

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
    uint64_t nextSeed_ = 0;
    unsigned runCounter_ = 0;
    std::string dataError_;

    // reputation with each faction, -50..50, reset per run
    std::vector<int> rep_;

    // travel
    struct TravelOption { std::string label, deck, siteName; int site = -1; };
    std::vector<TravelOption> travelOptions_;

    // current event
    const Event* current_ = nullptr;
    std::string currentText_;
    Grammar::Ctx currentCtx_;               // slot context, reused for outcomes
    std::vector<std::string> choiceTexts_;  // pre-expanded choice labels
    std::string siteName_;
    std::string deckTag_;
    int currentSite_ = -1; // world site index, -1 when wandering
    int eventsLeftHere_ = 0;
    ResolvedOutcome outcome_;
    int pendingArtifact_ = -1;    // artifact bound by an artifact_here slot
    bool pendingShop_ = false;    // "shop" effect fired this outcome
    std::string forcedNextId_;    // "goto <id>" effect: chain to this event

    // inventory / info
    Screen returnScreen_ = TRAVEL;
    int invSelected_ = -1;
    std::string infoText_;

    // vendor
    std::vector<ItemInstance> vendorStock_;
    bool vendorSellMode_ = false;
    std::string vendorLine_;

    // seed entry (title screen)
    bool enteringSeed_ = false;
    std::string seedInput_;

    bool pressed_ = false; // unified click/tap edge for this frame

    // audio / juice / meta
    AudioBank audio_;
    Profile profile_;
    float shake_ = 0.0f;   // decaying screen-shake amplitude in pixels
    float reveal_ = 0.0f;  // typewriter progress into currentText_
};
