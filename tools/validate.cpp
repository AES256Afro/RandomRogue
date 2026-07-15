// Content linter (PLAN.md tools/): catches dangling item ids, unknown effect
// verbs, broken goto targets, unknown slot queries, and grammar tokens that
// don't resolve. Run after every content edit; CI runs it too.
// Usage: validate [assets_dir]
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int errors = 0;
static void fail(const std::string& where, const std::string& what) {
    fprintf(stderr, "ERROR [%s] %s\n", where.c_str(), what.c_str());
    errors++;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static const std::set<std::string> kVerbs = {
    "hp", "maxhp", "money", "credits", "stat", "item", "loot", "removeitem",
    "rep", "shop", "goto", "take_artifact", "die"};
static const std::set<std::string> kSlotQueries = {
    "chronicle_random", "artifact_here", "figure_alive", "figure_dead"};
static const std::set<std::string> kDecks = {
    "city", "tavern", "dungeon", "dungeon_finale", "cave", "forest", "road"};
static const std::set<std::string> kStats = {"str", "dex", "con", "int", "wis", "cha"};

// Tokens the engine injects at runtime.
static const std::set<std::string> kBuiltinCtx = {"site", "world", "placeword",
                                                  "year", "actor", "trait",
                                                  "profession", "faction",
                                                  "faction2", "artifact",
                                                  "beast", "extra"};

int main(int argc, char** argv) {
    std::string assets = (argc > 1) ? argv[1] : "assets";

    json items = json::parse(readFile(assets + "/data/items.json"), nullptr, false);
    json events = json::parse(readFile(assets + "/data/events.json"), nullptr, false);
    json prose = json::parse(readFile(assets + "/data/recipes/prose.json"), nullptr, false);
    json lang = json::parse(readFile(assets + "/data/recipes/language.json"), nullptr, false);
    json quirks = json::parse(readFile(assets + "/data/quirks.json"), nullptr, false);
    if (items.is_discarded()) fail("items.json", "does not parse");
    if (events.is_discarded()) fail("events.json", "does not parse");
    if (prose.is_discarded()) fail("prose.json", "does not parse");
    if (lang.is_discarded()) fail("language.json", "does not parse");
    if (quirks.is_discarded()) fail("quirks.json", "does not parse");
    if (errors) return 1;

    std::set<std::string> itemIds;
    for (auto& t : items) {
        std::string id = t.value("id", "");
        if (id.empty()) fail("items.json", "item without id");
        if (!itemIds.insert(id).second) fail("items.json", "duplicate item id: " + id);
    }

    std::set<std::string> grammarKeys;
    for (auto& [k, v] : prose.items()) grammarKeys.insert(k);

    std::set<std::string> eventIds, gotoTargets;
    for (auto& e : events) {
        std::string id = e.value("id", "");
        if (!eventIds.insert(id).second) fail(id, "duplicate event id");
    }

    auto checkTokens = [&](const std::string& id, const std::string& text,
                           const std::set<std::string>& slotNames) {
        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] != '{') continue;
            size_t close = text.find('}', i);
            if (close == std::string::npos) {
                fail(id, "unclosed { in text: " + text.substr(i, 20));
                return;
            }
            std::string token = text.substr(i + 1, close - i - 1);
            bool ok = grammarKeys.count(token) || kBuiltinCtx.count(token);
            for (auto& s : slotNames)
                if (token == s || token.rfind(s + "_", 0) == 0) ok = true;
            if (!ok) fail(id, "unresolvable token {" + token + "}");
            i = close;
        }
    };

    auto checkEffects = [&](const std::string& id, const json& effects) {
        if (!effects.is_array()) return;
        for (auto& fx : effects) {
            std::istringstream ss(fx.get<std::string>());
            std::string verb, a;
            ss >> verb >> a;
            if (!kVerbs.count(verb)) { fail(id, "unknown effect verb: " + verb); continue; }
            if (verb == "item" || verb == "removeitem")
                if (!itemIds.count(a)) fail(id, "unknown item id in effect: " + a);
            if (verb == "stat" && !kStats.count(a)) fail(id, "unknown stat: " + a);
            if (verb == "loot" && a != "common" && a != "fine") fail(id, "unknown loot tier: " + a);
            if (verb == "goto") gotoTargets.insert(a);
        }
    };

    auto checkOutcomes = [&](const std::string& id, const json& arr,
                             const std::set<std::string>& slots) {
        if (!arr.is_array() || arr.empty()) { fail(id, "empty outcome list"); return; }
        for (auto& o : arr) {
            checkTokens(id, o.value("text", ""), slots);
            checkEffects(id, o.value("effects", json::array()));
        }
    };

    for (auto& e : events) {
        std::string id = e.value("id", "");
        std::set<std::string> slots;
        if (e.contains("slots"))
            for (auto& [name, q] : e["slots"].items()) {
                slots.insert(name);
                if (!kSlotQueries.count(q.get<std::string>()))
                    fail(id, "unknown slot query: " + q.get<std::string>());
            }
        if (e.contains("locations"))
            for (auto& l : e["locations"])
                if (!kDecks.count(l.get<std::string>()))
                    fail(id, "unknown deck tag: " + l.get<std::string>());
        checkTokens(id, e.value("text", ""), slots);
        const json& choices = e["choices"];
        if (!choices.is_array() || choices.empty() || choices.size() > 4)
            fail(id, "events need 1-4 choices, has " +
                         std::to_string(choices.is_array() ? choices.size() : 0));
        for (auto& c : choices) {
            checkTokens(id, c.value("text", ""), slots);
            if (c.contains("requires")) {
                const json& r = c["requires"];
                std::string item = r.value("item", "");
                if (!item.empty() && !itemIds.count(item))
                    fail(id, "requires unknown item: " + item);
                std::string stat = r.value("stat", "");
                if (!stat.empty() && !kStats.count(stat)) fail(id, "requires unknown stat: " + stat);
            }
            if (c.contains("check")) {
                std::string stat = c["check"].value("stat", "");
                if (!kStats.count(stat)) fail(id, "check on unknown stat: " + stat);
                checkOutcomes(id, c.value("success", json::array()), slots);
                checkOutcomes(id, c.value("fail", json::array()), slots);
            } else {
                checkOutcomes(id, c.value("outcomes", json::array()), slots);
            }
        }
    }

    for (auto& target : gotoTargets)
        if (!eventIds.count(target)) fail("goto", "target event does not exist: " + target);

    // Grammar self-check: every {token} inside grammar rules must resolve.
    for (auto& [key, variants] : prose.items()) {
        if (!variants.is_array()) continue;
        for (auto& v : variants) {
            std::string text = v.get<std::string>();
            for (size_t i = 0; i < text.size(); i++) {
                if (text[i] != '{') continue;
                size_t close = text.find('}', i);
                if (close == std::string::npos) { fail(key, "unclosed {"); break; }
                std::string token = text.substr(i + 1, close - i - 1);
                if (!grammarKeys.count(token) && !kBuiltinCtx.count(token) &&
                    token[0] != 'q') // q* tokens come from the quirk binder
                    fail(key, "grammar token {" + token + "} unresolvable");
                i = close;
            }
        }
    }

    printf("validate: %d events, %d items, %d grammar keys -> %s\n",
           (int)events.size(), (int)itemIds.size(), (int)grammarKeys.size(),
           errors ? "FAILED" : "all good");
    return errors ? 1 : 0;
}
