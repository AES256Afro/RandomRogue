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
    loadList(j, "syllables", syllables_);
    loadList(j, "nouns", nouns_);
    loadList(j, "adjectives", adjectives_);
}

std::string NameForge::word(Rng& rng, int syllables) const {
    if (syllables_.empty()) return "urist";
    std::string w;
    for (int i = 0; i < syllables; i++) w += rng.pick(syllables_);
    return w;
}

// NOTE: every rng draw below is sequenced into its own statement on purpose.
// Operand order in `a + b` is unspecified in C++, and GCC vs Clang/Emscripten
// disagree — which silently breaks cross-platform seed determinism.

GenName NameForge::person(Rng& rng) const {
    GenName n;
    std::string given = capitalize(word(rng, 2));
    std::string sur = capitalize(word(rng, 2));
    n.conlang = given + " " + sur;
    if (!adjectives_.empty() && !nouns_.empty()) {
        std::string adj = capitalize(rng.pick(adjectives_));
        std::string noun = rng.pick(nouns_);
        n.meaning = adj + noun;
    }
    return n;
}

GenName NameForge::place(Rng& rng) const {
    GenName n;
    int sylls = rng.range(2, 3);
    n.conlang = capitalize(word(rng, sylls));
    if (!adjectives_.empty() && !nouns_.empty()) {
        std::string adj = rng.pick(adjectives_);
        std::string noun = rng.pick(nouns_);
        n.meaning = "the " + adj + " " + noun;
    }
    return n;
}

GenName NameForge::artifact(Rng& rng) const {
    GenName n;
    int sylls = rng.range(2, 3);
    n.conlang = capitalize(word(rng, sylls));
    if (!nouns_.empty()) {
        std::string first = capitalize(rng.pick(nouns_));
        std::string second = rng.pick(nouns_);
        n.meaning = first + second;
    }
    return n;
}
