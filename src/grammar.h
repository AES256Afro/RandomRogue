// Tracery-style text expansion. Rules live in JSON: { "key": ["variant {other}", ...] }
// "{token}" expands from the context map first, then from rules (recursively).
// Unknown tokens are left as-is so missing content is visible, not fatal.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "rng.h"

class Grammar {
public:
    // Merge all string-array entries from a JSON object (text of a .json file).
    void loadJsonText(const char* jsonText);

    using Ctx = std::map<std::string, std::string>;
    std::string expand(const std::string& templ, Rng& rng, const Ctx& ctx = {}) const;

    bool has(const std::string& key) const { return rules_.count(key) > 0; }

private:
    std::string expandDepth(const std::string& templ, Rng& rng, const Ctx& ctx, int depth) const;
    std::map<std::string, std::vector<std::string>> rules_;
};
