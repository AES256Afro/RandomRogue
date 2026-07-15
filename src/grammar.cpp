#include "grammar.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void Grammar::loadJsonText(const char* jsonText) {
    if (!jsonText) return;
    json j = json::parse(jsonText, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return;
    for (auto& [key, val] : j.items()) {
        if (!val.is_array()) continue;
        auto& list = rules_[key];
        for (auto& v : val) {
            if (v.is_string()) list.push_back(v.get<std::string>());
        }
    }
}

std::string Grammar::expand(const std::string& templ, Rng& rng, const Ctx& ctx) const {
    return expandDepth(templ, rng, ctx, 0);
}

std::string Grammar::expandDepth(const std::string& templ, Rng& rng, const Ctx& ctx, int depth) const {
    if (depth > 8) return templ;
    std::string out;
    out.reserve(templ.size());
    for (size_t i = 0; i < templ.size();) {
        if (templ[i] == '{') {
            size_t close = templ.find('}', i);
            if (close != std::string::npos) {
                std::string token = templ.substr(i + 1, close - i - 1);
                auto c = ctx.find(token);
                if (c != ctx.end()) {
                    out += c->second;
                } else {
                    auto r = rules_.find(token);
                    if (r != rules_.end() && !r->second.empty()) {
                        const std::string& variant = r->second[rng.next() % r->second.size()];
                        out += expandDepth(variant, rng, ctx, depth + 1);
                    } else {
                        out += "{" + token + "}"; // visible, not fatal
                    }
                }
                i = close + 1;
                continue;
            }
        }
        out += templ[i++];
    }
    return out;
}
