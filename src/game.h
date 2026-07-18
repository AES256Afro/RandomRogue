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
    // Death-card export: true once when the player asked for a PNG this frame.
    bool consumeCardRequest() {
        bool r = cardRequested_;
        cardRequested_ = false;
        return r;
    }

private:
    enum Screen { TITLE, CLASSPICK, AMBITION, TRAVEL, EVENT, OUTCOME, DEATH,
                  INVENTORY, INFO, VENDOR, WORLDMAP, CHRONICLE, SAGA, REPLAY,
                  OPTIONS };

    // A hired sword, a talking badger, a disgraced accountant (P5).
    struct Companion {
        std::string id, name, kind, trait, passive;
        int packBonus = 0;
        bool active = false;
        int daysTogether = 0; // trust, measured the only way roads can (R10)
        bool devoted() const { return active && daysTogether >= 10; }
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
    void injectStrangers(const std::string& json); // other players' fallen (R4)
    void dailyTick();                // the living world advances one day

    void newRun(int classIdx);
    ItemInstance makeItem(const std::string& id);
    void bindQuirk(ItemInstance& item);
    // Pack management (R10): loot never vanishes silently. A full pack
    // swaps out the cheapest thing you carry (if the new item beats it)
    // and the outcome text says so.
    void takeItem(const ItemInstance& item);
    bool dropCheapest(const std::string& why);
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
    void drawOutcome(Vector2 mouse);
    void drawDeath(Vector2 mouse);
    void drawInventory(Vector2 mouse);
    void drawInfo(Vector2 mouse);
    void drawVendor(Vector2 mouse);
    void drawTopBar();
    void drawWorldMap(Vector2 mouse);
    void drawChronicle(Vector2 mouse);
    void drawSaga(Vector2 mouse);
    void drawReplay(Vector2 mouse);
    void drawOptions(Vector2 mouse);
    void drawIntro(Vector2 mouse); // first-run how-to cards (R7)
    // Shared worlds (R7): daily and weekly seeds and their board keys.
    // Weekly board keys live in the 7,000,000+ namespace so the two
    // leaderboards never collide in D1.
    static uint64_t dailySeed() {
        return (uint64_t)(time(nullptr) / 86400) % 1000000000ULL;
    }
    static uint64_t weeklySeed() {
        return ((uint64_t)(time(nullptr) / (86400 * 7)) * 7777777ULL) % 1000000000ULL;
    }
    int boardKeyFor(uint64_t seed) const {
        if (seed == dailySeed()) return (int)(time(nullptr) / 86400);
        if (seed == weeklySeed()) return 7000000 + (int)(time(nullptr) / (86400 * 7));
        return -1;
    }
    void drawPortrait(int x, int y, int scale, const std::string& name);
    void saveRun();
    bool loadRun();
    void clearRun();
    int optionRows(const std::vector<std::string>& rows,
                   const std::vector<bool>& enabled, Vector2 mouse);
    // Total pixel height optionRows will use (rows wrap to two lines, R9b).
    int optionRowsHeight(const std::vector<std::string>& rows) const;
    // Scrollable reader: draws `lines` from textScroll_ within [yTop, maxY),
    // with ^ / v controls when they overflow. Returns true when a scroll
    // control consumed this frame's press, so tap-to-continue screens don't
    // also advance. No scenario text is ever unreadable again (R9b).
    bool drawScrollText(const std::vector<std::string>& lines, int x, int yTop,
                        int maxY, Color color, Vector2 mouse, bool follow);
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

    // travel (P6: the world has distance now)
    struct TravelOption { std::string label, deck, siteName; int site = -1; int days = 1;
                          bool sail = false; };
    std::vector<TravelOption> travelOptions_;
    int currentRegion_ = 0;
    std::vector<int> regionDistances() const; // BFS hops from currentRegion_

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
    int slotBeast_ = -1;          // beast bound by a beast_here slot
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
    bool legacySaved_ = false;   // this death already recorded
    bool finishedWell_ = false;  // ended alive: ascended, retired, complete
    bool afterlifeShown_ = false; // the gate only opens once per death
    bool heirBlessing_ = false;  // afterlife bargain: bless the next of the line
    int contractsDone_ = 0;      // faction career ladder
    std::set<int> visitedRegions_;
    int chronPage_ = 0;                    // chronicle browser page
    std::vector<std::string> chronLines_;  // cached rendered page
    int chronCachedPage_ = -1;
    int chronDetail_ = -1; // tapped line: show the full entry (R9)
    std::vector<int> chronIdx_;      // chron entry index per cached line (R10)
    int chronFilterActor_ = -1;      // following one figure's thread
    int chronFilterFaction_ = -1;    // ...or one faction's
    std::vector<int> chronFilterList_;
    std::string scoresJson_;     // today's fallen (web leaderboard)
    bool scoresRequested_ = false;
    bool scoreSubmitted_ = false;

    // R4: the online graveyard, the rival, the tongue you're learning.
    struct Stranger { // another player's death, walked into this world
        int figure = -1; // injected figure index
        std::string name, epitaph;
        int days = 0, site = -1;
    };
    std::vector<Stranger> strangers_;
    std::string ghostsRaw_; // the JSON we injected, kept for the autosave
    bool ghostsRequested_ = false;
    bool suppressStrangers_ = false; // loadRun re-injects from the save instead
    struct Rival { // the other wanderer this world generated
        std::string name, meaning;
        bool alive = true;
        int deeds = 0;
    };
    Rival rival_;
    bool cardRequested_ = false; // death-card PNG wanted (handled by main.cpp)

    // R5: the saga, the gods, the replays, the feed.
    std::vector<std::pair<uint64_t, LegacyRecord>> sagaLives_;
    int sagaPage_ = 0;
    int slotGod_ = -1;             // god bound by the last god slot
    std::map<int, int> favor_;     // god index -> favor earned this run
    bool miracleUsed_ = false;     // one divine intervention per life
    struct JourneyStep { int day = 0; std::string site, choice, outcome; };
    std::vector<JourneyStep> journey_;  // this run, recorded for replay
    std::vector<JourneyStep> replay_;   // someone else's run, being watched
    std::string replayWho_;
    int replayPage_ = 0;
    bool replayRequested_ = false;
    std::vector<int> scoreIds_;         // D1 row ids of today's fallen
    std::vector<std::string> scoreNames_;
    int replayCursor_ = 0;              // which of the fallen we're watching
    std::vector<std::string> deeds_;    // live deeds feed (daily worlds)
    size_t deedNext_ = 0;
    bool deedsRequested_ = false;

    // R7: ambition counters, world age, weekly worlds, mods, onboarding.
    bool beastSlainThisRun_ = false;
    int wordsThisRun_ = 0;
    int landfalls_ = 0;
    bool settledThisRun_ = false;
    int worldGen_ = 0;    // lives this world has taken from you already
    int introPage_ = -1;  // >=0: the how-to cards are showing
    bool modLoaded_ = false;
    std::string modLine_; // "mod loaded: N events" on the title
    int lastBoardKey_ = -2; // refetch scores/ghosts/deeds when this changes

    int textScroll_ = 0; // line offset of the scrollable reader (R9b)

    // audio / juice / meta
    AudioBank audio_;
    Profile profile_;
    float shake_ = 0.0f;   // decaying screen-shake amplitude in pixels
    float reveal_ = 0.0f;  // typewriter progress into currentText_
};
