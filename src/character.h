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
    int packMax = 9;              // a mimic porter raises this
    std::string companionPassive; // companion's check bonus, if any

    // Floor division: stat 9 is -1 and stat 7 is -2, as the dice intend.
    // Plain (stat-10)/2 rounded toward zero and let weak stats off easy.
    static int mod(int stat) {
        return stat >= 10 ? (stat - 10) / 2 : -((11 - stat) / 2);
    }

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

    // Effective pack space: a satchel earns its slot back and then some (R10).
    int capacity() const { return packMax + (hasItem("satchel") ? 2 : 0); }

    // Best-in-kind equipment (R10): you fight with ONE weapon and wear ONE
    // suit of armor, so only the best of each counts toward a given check.
    // Trinkets and oddments (and quirks on the counted gear) still add up -
    // that is what pockets are for.
    static int passiveFor(const std::string& p, int stat) {
        if (p.empty()) return 0;
        std::istringstream ss(p);
        std::string verb, which; int v = 0;
        ss >> verb >> which >> v;
        return (verb == "check" && statFromName(which) == stat) ? v : 0;
    }

    int checkBonus(int stat) const {
        int total = 0, bestWeapon = 0, bestArmor = 0;
        for (auto& i : pack) {
            int contrib = passiveFor(i.passive, stat) + passiveFor(i.quirkPassive, stat);
            if (i.type == "weapon") {
                if (contrib > bestWeapon) bestWeapon = contrib;
            } else if (i.type == "armor") {
                if (contrib > bestArmor) bestArmor = contrib;
            } else {
                total += contrib;
            }
        }
        total += bestWeapon + bestArmor;
        if (!companionPassive.empty()) {
            std::istringstream ss(companionPassive);
            std::string verb, which; int v = 0;
            ss >> verb >> which >> v;
            if (verb == "check" && statFromName(which) == stat) total += v;
        }
        return total;
    }

    // Best single "armor N" (plus its quirk); plate does not stack on plate.
    int armor() const {
        int best = 0;
        auto val = [](const std::string& p) {
            if (p.empty()) return 0;
            std::istringstream ss(p);
            std::string verb; int v = 0;
            ss >> verb >> v;
            return verb == "armor" ? v : 0;
        };
        for (auto& i : pack) {
            int a = val(i.passive) + val(i.quirkPassive);
            if (a > best) best = a;
        }
        return best > 3 ? 3 : best;
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
