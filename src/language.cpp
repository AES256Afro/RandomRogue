#include "language.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::string capitalize(std::string s) {
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
    return s;
}
void loadList(const json& j, const char* key, std::vector<std::string>& out) {
    if (!j.contains(key) || !j[key].is_array()) return;
    for (auto& v : j[key])
        if (v.is_string()) out.push_back(v.get<std::string>());
}
} // namespace

void NameForge::loadJsonText(const char* jsonText) {
    if (!jsonText) return;
    json j = json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;
    loadList(j, "nouns", nouns_);
    loadList(j, "adjectives", adjectives_);
    // v2: per-culture syllable sets under "cultures"; v1 fallback: flat list.
    static const char* kCultureKeys[CULT_COUNT] = {"old", "goblin", "spacer",
                                                   "swamp", "liturgical"};
    if (j.contains("cultures") && j["cultures"].is_object()) {
        for (int c = 0; c < CULT_COUNT; c++)
            if (j["cultures"].contains(kCultureKeys[c]))
                loadList(j["cultures"][kCultureKeys[c]], "syllables", syllables_[c]);
    }
    loadList(j, "syllables", syllables_[CULT_OLD]);
}

const std::vector<std::string>& NameForge::syllables(int culture) const {
    if (culture >= 0 && culture < CULT_COUNT && !syllables_[culture].empty())
        return syllables_[culture];
    return syllables_[CULT_OLD];
}

// NOTE: every rng draw below is sequenced into its own statement on purpose.
// Operand order in `a + b` is unspecified in C++, and GCC vs Clang/Emscripten
// disagree — which silently breaks cross-platform seed determinism.

std::string NameForge::word(Rng& rng, int sylls, int culture) const {
    const auto& pool = syllables(culture);
    if (pool.empty()) return "urist";
    std::string w;
    for (int i = 0; i < sylls; i++) w += rng.pick(pool);
    return w;
}

GenName NameForge::person(Rng& rng, int culture) const {
    GenName n;
    std::string given = capitalize(word(rng, 2, culture));
    std::string sur = capitalize(word(rng, 2, culture));
    n.conlang = given + " " + sur;
    if (!adjectives_.empty() && !nouns_.empty()) {
        std::string adj = capitalize(rng.pick(adjectives_));
        std::string noun = rng.pick(nouns_);
        n.meaning = adj + noun;
    }
    return n;
}

GenName NameForge::place(Rng& rng, int culture) const {
    GenName n;
    int sylls = rng.range(2, 3);
    n.conlang = capitalize(word(rng, sylls, culture));
    if (!adjectives_.empty() && !nouns_.empty()) {
        std::string adj = rng.pick(adjectives_);
        std::string noun = rng.pick(nouns_);
        n.meaning = "the " + adj + " " + noun;
    }
    return n;
}

GenName NameForge::artifact(Rng& rng, int culture) const {
    GenName n;
    int sylls = rng.range(2, 3);
    n.conlang = capitalize(word(rng, sylls, culture));
    if (!nouns_.empty()) {
        std::string first = capitalize(rng.pick(nouns_));
        std::string second = rng.pick(nouns_);
        n.meaning = first + second;
    }
    return n;
}
