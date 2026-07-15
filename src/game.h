#pragma once
#include <map>
#include <set>
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
    enum Screen { TITLE, CLASSPICK, AMBITION, TRAVEL, EVENT, OUTCOME, DEATH,
                  INVENTORY, INFO, VENDOR };

    // A hired sword, a talking badger, a disgraced accountant (P5).
    struct Companion {
        std::string id, name, kind, trait, passive;
        int packBonus = 0;
        bool active = false;
    };
    struct Ambition {
        std::string name, desc;
        int id = -1; // index into the ambition table; -1 = none
        bool done = false;
    };
    struct Contract {
        std::string desc;
        int artifactId = -1; // fetch this artifact...
        int siteId = -1;     // ...or visit this site
        int faction = -1;
        int reward = 0;
        bool active = false;
    };

    struct StartClass {
        std::string name;
        std::string blurb;
        std::string lockHint; // requirement text when locked
        bool unlocked;
    };
    std::vector<StartClass> startClasses() const;
    void injectLegacy(int classIdx); // dead PCs -> figures, items -> relics
    void dailyTick();                // the living world advances one day

    void newRun(int classIdx);
    ItemInstance makeItem(const std::string& id);
    void bindQuirk(ItemInstance& item);
    // Evaluates a `when` condition string against world + character state.
    bool evalCond(const std::string& cond) const;
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
    void drawAmbitionPick(Vector2 mouse);
    void drawTravel(Vector2 mouse);
    void setCompanion(const std::string& id);
    void dismissCompanion(bool died);
    void offerContract();
    void checkPurposes(); // ambition + contract completion
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
    int slotFigure_ = -1;         // figure bound by a figure_alive slot
    bool blessingSpent_ = false;  // a blessed trait absorbed death this outcome
    // Per-run NPC memory: chronicle figure index -> marks ("robbed", ...)
    std::map<int, std::set<std::string>> npcMarks_;
    std::map<std::string, std::string> traitNames_; // id -> display name
    bool pendingShop_ = false;    // "shop" effect fired this outcome
    std::string forcedNextId_;    // "goto <id>" effect: chain to this event

    // inventory / info
    Screen returnScreen_ = TRAVEL;
    Screen infoBack_ = INVENTORY;
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

    // company & purpose (P5)
    Companion comp_;
    std::vector<Companion> compKinds_;
    Ambition ambition_;
    Contract contract_;
    std::string compLine_;      // this event's interjection, if any
    int finalesSeen_ = 0;
    int booksThisRun_ = 0;
    std::vector<int> ambitionChoices_; // rolled at AMBITION screen
    int pendingClass_ = 0;

    // the unbroken chronicle (P2+P3)
    std::vector<LegacyRecord> pendingLegacy_; // ghosts of nextSeed_'s world
    uint64_t cachedLegacySeed_ = ~0ULL;
    Rng liveRng_;
    std::string season_ = "spring", weather_ = "clear";
    std::string newsLine_;
    bool legacySaved_ = false; // this death already recorded

    // audio / juice / meta
    AudioBank audio_;
    Profile profile_;
    float shake_ = 0.0f;   // decaying screen-shake amplitude in pixels
    float reveal_ = 0.0f;  // typewriter progress into currentText_
};
