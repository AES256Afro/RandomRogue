// Items: base template + rolled quirk. A quirk's flavor text may or may not
// carry a hidden modifier, and the player is never told which (PLAN.md §4.2).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "rng.h"

struct ItemTemplate {
    std::string id, name, type; // type: weapon, armor, food, drink, med, book, bag, misc
    int value = 0;
    std::vector<std::string> useEffects; // consumables: applied on use
    std::string passive;                 // "check str +1", "armor 1", ""
};

struct ItemInstance {
    std::string templateId, name, type;
    int value = 0;
    std::vector<std::string> useEffects;
    std::string passive;      // template passive
    std::string quirk;        // flavor text, possibly meaningless
    std::string quirkPassive; // hidden modifier (or empty) — never shown
    std::string provenance;   // artifact history, if any
    int artifactId = -1;

    std::string displayName() const {
        return quirk.empty() ? name : name + " (" + quirk + ")";
    }
};

class ItemDb {
public:
    void loadItemsJsonText(const char* jsonText);
    void loadQuirksJsonText(const char* jsonText);
    // Weapon/armor families from material x type x tier tables — the recipe
    // philosophy applied to gear (ROADMAP P4).
    void generateFamilies();

    bool has(const std::string& id) const { return templates_.count(id) > 0; }
    // Quirks are NOT rolled here: the Game binds them, because every quirk
    // must tie to something — a hidden modifier or a piece of world history.
    ItemInstance make(const std::string& id, Rng& rng) const;
    // Random item by tier: "common" (value <= 8) or "fine" (value > 8).
    ItemInstance loot(Rng& rng, const std::string& tier) const;
    size_t size() const { return templates_.size(); }

    const std::vector<std::string>& quirkTexts() const { return quirkTexts_; }
    const std::vector<std::string>& goodPassives() const { return goodPassives_; }
    const std::vector<std::string>& badPassives() const { return badPassives_; }
    // Provenance-coherent quirks (R10): text and mechanics are one record,
    // so an item that "repels dogs" never quietly grants strength.
    const std::vector<std::pair<std::string, std::string>>& quirkPairs() const {
        return quirkPairs_;
    }

private:
    std::map<std::string, ItemTemplate> templates_;
    std::vector<std::string> order_; // for random draws
    std::vector<std::string> quirkTexts_;
    std::vector<std::string> goodPassives_, badPassives_;
    std::vector<std::pair<std::string, std::string>> quirkPairs_;
};
