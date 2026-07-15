// Event cards: the atom of everything (PLAN.md §4.1).
#pragma once
#include <set>
#include <string>
#include <vector>
#include "character.h"
#include "rng.h"

struct Outcome {
    int weight = 1;
    std::string text;
    std::vector<std::string> effects; // "hp -6", "money +25", "stat str +1", "die <epitaph>"
};

struct Requirement {
    std::string stat; // empty = no stat requirement
    int gte = 0;
    int moneyGte = 0;
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
};

struct ResolvedOutcome {
    std::string rollText; // "d20+2 = 14 vs DC 12 -- success!" or empty
    std::string text;
    std::vector<std::string> effects;
};

class EventDeck {
public:
    void loadJsonText(const char* jsonText);
    // Draw an event for a location tag; avoids repeats until the pool empties.
    const Event* draw(Rng& rng, const std::string& location);
    void resetUsed() { used_.clear(); }
    size_t size() const { return events_.size(); }

    static ResolvedOutcome resolve(const Choice& choice, const Character& c, Rng& rng);
    // Returns a death epitaph if the effects killed the character, else "".
    static void apply(const std::vector<std::string>& effects, Character& c);

private:
    std::vector<Event> events_;
    std::set<std::string> used_;
};
