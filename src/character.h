#pragma once
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "items.h"
#include "language.h"
#include "rng.h"

enum Stat { STAT_STR, STAT_DEX, STAT_CON, STAT_INT, STAT_WIS, STAT_CHA, STAT_COUNT };

inline const char* statName(int s) {
    static const char* names[STAT_COUNT] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    return names[s];
}

inline int statFromName(const std::string& n) {
    for (int i = 0; i < STAT_COUNT; i++) {
        std::string lower = statName(i);
        for (char& c : lower) c = (char)(c - 'A' + 'a');
        if (n == lower) return i;
    }
    return STAT_STR;
}

struct Character {
    GenName name;
    int stats[STAT_COUNT] = {};
    int hp = 10, maxHp = 10;
    int money = 20, credits = 0;
    int day = 1;
    int eventsSurvived = 0;
    bool dead = false;
    std::string epitaph;
    std::vector<ItemInstance> pack;
    // Afflictions & reputations-made-flesh: "cursed", "famous", "wanted"...
    // Granted/removed by effects, gate choices/outcomes/events.
    std::set<std::string> traits;
    static constexpr int kPackMax = 9;

    static int mod(int stat) { return (stat - 10) / 2; }

    bool hasTrait(const std::string& t) const { return traits.count(t) > 0; }

    bool hasItem(const std::string& templateId) const {
        for (auto& i : pack)
            if (i.templateId == templateId) return true;
        return false;
    }

    bool removeItem(const std::string& templateId) {
        for (size_t i = 0; i < pack.size(); i++) {
            if (pack[i].templateId == templateId) {
                pack.erase(pack.begin() + i);
                return true;
            }
        }
        return false;
    }

    // Sum of "check <stat> +N" passives across carried items (incl. quirks).
    int checkBonus(int stat) const {
        int total = 0;
        auto scan = [&](const std::string& p) {
            if (p.empty()) return;
            std::istringstream ss(p);
            std::string verb, which; int v = 0;
            ss >> verb >> which >> v;
            if (verb == "check" && statFromName(which) == stat) total += v;
        };
        for (auto& i : pack) { scan(i.passive); scan(i.quirkPassive); }
        return total;
    }

    // Sum of "armor N" passives; flat damage reduction (never below 1 damage).
    int armor() const {
        int total = 0;
        auto scan = [&](const std::string& p) {
            if (p.empty()) return;
            std::istringstream ss(p);
            std::string verb; int v = 0;
            ss >> verb >> v;
            if (verb == "armor") total += v;
        };
        for (auto& i : pack) { scan(i.passive); scan(i.quirkPassive); }
        return total > 3 ? 3 : total;
    }

    static Character roll(Rng& rng, const NameForge& forge) {
        Character c;
        c.name = forge.person(rng);
        for (int i = 0; i < STAT_COUNT; i++)
            c.stats[i] = rng.d(6) + rng.d(6) + rng.d(6);
        c.maxHp = 10 + mod(c.stats[STAT_CON]) * 2;
        if (c.maxHp < 6) c.maxHp = 6;
        c.hp = c.maxHp;
        c.money = 15 + rng.range(0, 10);
        return c;
    }
};
