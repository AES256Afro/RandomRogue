// Event cards: the atom of everything (PLAN.md §4.1).
#pragma once
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "character.h"
#include "rng.h"

// A condition string evaluated against world + character state by the Game:
// "trait cursed", "!trait wanted", "rep > 10", "money < 5", "hp < 4",
// "stat str >= 14", "has rope", "!has rope", "carrying_artifact",
// "day > 10", "npc robbed". All conditions in a `when` list must hold.
using CondEval = std::function<bool(const std::string&)>;

struct Outcome {
    int weight = 1;
    std::string text;
    std::vector<std::string> effects; // "hp -6", "money +25", "trait +cursed", ...
    std::vector<std::string> when;    // empty = default outcome
};

struct Requirement {
    std::string stat; // empty = no stat requirement
    int gte = 0;
    int moneyGte = 0;
    int creditsGte = 0;
    std::string item;     // template id the player must carry
    std::string trait;    // trait the player must have
    std::string notTrait; // trait the player must NOT have
    bool met(const Character& c) const;
    std::string label() const; // "[CHA 12]" style tag, empty if none
};

struct Check {
    std::string stat; // empty = no check on this choice
    int dc = 10;
};

struct Choice {
    std::string text;
    Requirement requires_;
    Check check;
    std::vector<Outcome> outcomes;        // weighted (no check)
    std::vector<Outcome> successOutcomes; // with check
    std::vector<Outcome> failOutcomes;
};

struct Event {
    std::string id;
    std::vector<std::string> locations;
    int weight = 10;
    std::string text;
    std::vector<Choice> choices;
    // Chronicle slot queries: name -> query ("chronicle_random",
    // "artifact_here", ...). If a query can't be satisfied at deal time the
    // event is skipped (WORLDGEN.md §4).
    std::vector<std::pair<std::string, std::string>> slots;
    // Event-level gate: only dealt when all conditions hold (e.g. bounty
    // hunters need "trait wanted").
    std::vector<std::string> when;
};

struct ResolvedOutcome {
    std::string rollText; // "STR check: d20(11)+2 = 13 vs DC 12 -- success!" or empty
    std::string text;
    std::vector<std::string> effects;
};

class EventDeck {
public:
    void loadJsonText(const char* jsonText);
    // Draws a weighted random unused event for a location. Does NOT mark it
    // used — call markUsed once the event is actually shown; gated or
    // slot-failed draws go back in the pool so they don't silently drain it
    // and cause early repeats (R9). `exclude` skips ids already tried this
    // deal so the retry loop can't spin on the same ineligible card.
    const Event* draw(Rng& rng, const std::string& location,
                      const std::set<std::string>* exclude = nullptr);
    void markUsed(const std::string& id) { used_.insert(id); }
    const Event* find(const std::string& id) const;
    void resetUsed() { used_.clear(); }
    size_t size() const { return events_.size(); }

    // Picks the outcome; item passives feed the check modifier. Outcomes
    // whose `when` conditions pass OVERRIDE the default pool — special
    // cases beat generic ones, which is what makes cards situational.
    static ResolvedOutcome resolve(const Choice& choice, const Character& c, Rng& rng,
                                   const CondEval& cond);

private:
    std::vector<Event> events_;
    std::set<std::string> used_;
};
