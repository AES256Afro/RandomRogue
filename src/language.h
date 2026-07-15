// NameForge — DF-style constructed-language names across five cultures.
// Words are built from a culture's syllable inventory; meanings are drawn
// from a shared lexicon, so every name is translatable: Kazak Urdim,
// "Hammercoin". Goblins sound goblin. Spacer corporations sound like
// liability waivers.
#pragma once
#include <string>
#include <vector>
#include "rng.h"

enum Culture {
    CULT_OLD = 0,     // the old tongue: default names for places and folk
    CULT_GOBLIN = 1,  // hard consonants, no patience
    CULT_SPACER = 2,  // corporate sky-debris people
    CULT_SWAMP = 3,   // wet vowels, mud in the phonology
    CULT_LITURGICAL = 4, // long, flowing, faintly disapproving
    CULT_COUNT = 5
};

struct GenName {
    std::string conlang; // "Kazak Urdim"
    std::string meaning; // "Hammercoin"
    std::string full() const { return conlang + " \"" + meaning + "\""; }
};

class NameForge {
public:
    void loadJsonText(const char* jsonText);

    std::string word(Rng& rng, int syllables, int culture = CULT_OLD) const;
    GenName person(Rng& rng, int culture = CULT_OLD) const;
    GenName place(Rng& rng, int culture = CULT_OLD) const;
    GenName artifact(Rng& rng, int culture = CULT_OLD) const;

private:
    const std::vector<std::string>& syllables(int culture) const;
    std::vector<std::string> syllables_[CULT_COUNT];
    std::vector<std::string> nouns_;
    std::vector<std::string> adjectives_;
};
