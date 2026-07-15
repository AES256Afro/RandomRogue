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

    bool has(const std::string& id) const { return templates_.count(id) > 0; }
    ItemInstance make(const std::string& id, Rng& rng, bool allowQuirk = true) const;
    // Random item by tier: "common" (value <= 8) or "fine" (value > 8).
    ItemInstance loot(Rng& rng, const std::string& tier) const;
    size_t size() const { return templates_.size(); }

private:
    void rollQuirk(ItemInstance& item, Rng& rng) const;
    std::map<std::string, ItemTemplate> templates_;
    std::vector<std::string> order_; // for random draws
    std::vector<std::string> quirkTexts_;
    std::vector<std::string> goodPassives_, badPassives_;
};
