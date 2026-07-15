// NameForge — DF-style constructed-language names. Words are built from a
// syllable inventory; meanings are drawn from lexicon lists, so every name is
// translatable: Kazak Urdim, "Hammercoin".
#pragma once
#include <string>
#include <vector>
#include "rng.h"

struct GenName {
    std::string conlang; // "Kazak Urdim"
    std::string meaning; // "Hammercoin"
    std::string full() const { return conlang + " \"" + meaning + "\""; }
};

class NameForge {
public:
    void loadJsonText(const char* jsonText);

    std::string word(Rng& rng, int syllables) const;
    GenName person(Rng& rng) const;   // given name + translatable surname
    GenName place(Rng& rng) const;    // single word + "the X Y" style meaning
    GenName artifact(Rng& rng) const; // dramatic compound

private:
    std::vector<std::string> syllables_;
    std::vector<std::string> nouns_;
    std::vector<std::string> adjectives_;
};
