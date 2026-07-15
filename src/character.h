#pragma once
#include <string>
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

    static int mod(int stat) { return (stat - 10) / 2; }

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
