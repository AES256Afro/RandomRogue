// Monte-Carlo playtest bot (ROADMAP P7): plays thousands of headless runs
// with a random-choice policy and reports what the dice actually do to
// players — survival curves, economy, deadliest events, unreachable content.
// Balance by data, not vibes. Usage: playtest [runs] [assets_dir]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../src/character.h"
#include "../src/events.h"
#include "../src/items.h"
#include "../src/rng.h"

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct BotState {
    Character c;
    Rng rng;
    std::string lastEventId;
    bool finishedWell = false;
    int foodEaten = 0;
};

// A pared-down mirror of Game::evalCond — world conditions get coin flips.
static bool botCond(BotState& s, const std::string& cond) {
    std::istringstream ss(cond);
    std::string a, b, cc;
    ss >> a >> b >> cc;
    if (a == "trait") return s.c.hasTrait(b);
    if (a == "!trait") return !s.c.hasTrait(b);
    if (a == "has") return s.c.hasItem(b);
    if (a == "!has") return !s.c.hasItem(b);
    if (a == "companion") return false;
    if (a == "!companion") return true;
    if (a == "carrying_artifact") {
        for (auto& i : s.c.pack)
            if (i.artifactId >= 0) return true;
        return false;
    }
    if (a == "vehicle" || a == "!vehicle") {
        bool has = false;
        for (auto& i : s.c.pack)
            if (i.type == "vehicle") has = true;
        return a[0] == '!' ? !has : has;
    }
    if (a == "war_here" || a == "plague_here" || a == "raining" || a == "snowing")
        return s.rng.chance(12);
    if (a == "season") return s.rng.chance(25);
    if (a == "npc") return false;
    auto cmp = [&](int lhs) {
        int rhs = atoi(cc.c_str());
        if (b == ">") return lhs > rhs;
        if (b == "<") return lhs < rhs;
        if (b == ">=") return lhs >= rhs;
        if (b == "<=") return lhs <= rhs;
        return lhs == rhs;
    };
    if (a == "rep") return cmp(0);
    if (a == "money") return cmp(s.c.money);
    if (a == "credits") return cmp(s.c.credits);
    if (a == "hp") return cmp(s.c.hp);
    if (a == "day") return cmp(s.c.day);
    if (a == "stat") {
        std::string d;
        ss >> d;
        int lhs = s.c.stats[statFromName(b)];
        int rhs = atoi(d.c_str());
        if (cc == ">") return lhs > rhs;
        if (cc == "<") return lhs < rhs;
        if (cc == ">=") return lhs >= rhs;
        if (cc == "<=") return lhs <= rhs;
        return lhs == rhs;
    }
    return false;
}

int main(int argc, char** argv) {
    int runs = (argc > 1) ? atoi(argv[1]) : 3000;
    std::string assets = (argc > 2) ? argv[2] : "assets";

    ItemDb items;
    {
        std::string t = readFile(assets + "/data/items.json");
        items.loadItemsJsonText(t.c_str());
        std::string q = readFile(assets + "/data/quirks.json");
        items.loadQuirksJsonText(q.c_str());
        items.generateFamilies();
    }
    EventDeck deck;
    {
        std::string m = readFile(assets + "/data/events/manifest.json");
        // crude manifest parse: pull "xxx.json" tokens
        size_t pos = 0;
        while ((pos = m.find(".json", pos)) != std::string::npos) {
            size_t start = m.rfind('"', pos);
            std::string fn = m.substr(start + 1, pos + 5 - start - 1);
            std::string t = readFile(assets + "/data/events/" + fn);
            deck.loadJsonText(t.c_str());
            pos += 5;
        }
    }
    if (deck.size() == 0) {
        fprintf(stderr, "no events loaded\n");
        return 1;
    }

    static const char* kDecks[] = {"tavern", "city", "dungeon", "cave",
                                   "forest", "road", "crash"};
    static const int kDeckW[] = {20, 20, 18, 12, 15, 15, 5};

    std::map<std::string, int> killerEvents, seenEvents;
    std::map<std::string, int> finishEvents;
    std::vector<int> deathDays;
    int finished = 0;
    long long goldAtEnd = 0;
    int dieBefore5 = 0;

    NameForge dummyForge; // unused by items, but Character::roll wants one
    {
        std::string l = readFile(assets + "/data/recipes/language.json");
        dummyForge.loadJsonText(l.c_str());
    }

    for (int r = 0; r < runs; r++) {
        BotState s;
        s.rng = Rng(1000003ULL * (r + 1), 5);
        s.c = Character::roll(s.rng, dummyForge);
        s.c.pack.push_back(items.make("rations", s.rng));
        s.c.pack.push_back(items.make(s.rng.chance(50) ? "rusty_sword" : "club", s.rng));
        deck.resetUsed();

        while (!s.c.dead && s.c.day < 60) {
            // travel
            s.c.day++;
            if (s.rng.chance(30)) { // a longer haul: supplies or skin
                s.c.day++;
                bool ate = false;
                for (size_t i = 0; i < s.c.pack.size(); i++)
                    if (s.c.pack[i].type == "food") {
                        s.c.pack.erase(s.c.pack.begin() + i);
                        ate = true;
                        break;
                    }
                if (!ate) {
                    s.c.hp -= 2;
                    if (s.c.hp < 1) s.c.hp = 1;
                }
            }
            // deck pick
            int total = 0;
            for (int w : kDeckW) total += w;
            int roll = s.rng.range(1, total), di = 0;
            for (; di < 7; di++) {
                roll -= kDeckW[di];
                if (roll <= 0) break;
            }
            std::string tag = kDecks[di];
            bool dungeonish = (tag == "dungeon" || tag == "crash");
            int nEvents = dungeonish ? s.rng.range(3, 4) : s.rng.range(2, 3);

            for (int e = 0; e < nEvents && !s.c.dead; e++) {
                std::string useTag = tag;
                if (dungeonish && e == nEvents - 1) useTag = "dungeon_finale";
                const Event* ev = deck.draw(s.rng, useTag);
                if (!ev && useTag != tag) ev = deck.draw(s.rng, tag);
                if (!ev && tag == "crash") ev = deck.draw(s.rng, "dungeon");
                if (!ev) break;
                // event-level gate
                bool gated = false;
                for (auto& w : ev->when)
                    if (!botCond(s, w)) gated = true;
                if (gated) { e--; continue; }
                // skip events with unsatisfiable slots (approximate: 60% ok)
                if (!ev->slots.empty() && s.rng.chance(30)) continue;

                // follow goto chains up to 5 hops
                for (int hop = 0; hop < 5 && ev && !s.c.dead; hop++) {
                    seenEvents[ev->id]++;
                    std::vector<int> eligible;
                    for (int ci = 0; ci < (int)ev->choices.size(); ci++)
                        if (ev->choices[ci].requires_.met(s.c)) eligible.push_back(ci);
                    if (eligible.empty()) break;
                    const Choice& choice =
                        ev->choices[eligible[s.rng.range(0, (int)eligible.size() - 1)]];
                    ResolvedOutcome out = EventDeck::resolve(
                        choice, s.c, s.rng,
                        [&](const std::string& w) { return botCond(s, w); });
                    std::string gotoId;
                    for (auto& fx : out.effects) {
                        std::istringstream fs(fx);
                        std::string verb;
                        fs >> verb;
                        if (verb == "hp") {
                            int v; fs >> v;
                            if (v < 0) {
                                int red = -v - s.c.armor();
                                v = -(red < 1 ? 1 : red);
                            }
                            s.c.hp += v;
                            if (s.c.hp > s.c.maxHp) s.c.hp = s.c.maxHp;
                        } else if (verb == "maxhp") { int v; fs >> v; s.c.maxHp += v; }
                        else if (verb == "money") { int v; fs >> v; s.c.money = std::max(0, s.c.money + v); }
                        else if (verb == "credits") { int v; fs >> v; s.c.credits = std::max(0, s.c.credits + v); }
                        else if (verb == "stat") { std::string w; int v; fs >> w >> v; int i = statFromName(w); s.c.stats[i] = std::max(1, s.c.stats[i] + v); }
                        else if (verb == "trait") { std::string t; fs >> t; if (t[0] == '+') s.c.traits.insert(t.substr(1)); else s.c.traits.erase(t.substr(1)); }
                        else if (verb == "item") { std::string id; fs >> id; if ((int)s.c.pack.size() < s.c.packMax) s.c.pack.push_back(items.make(id, s.rng)); }
                        else if (verb == "loot") { std::string t; fs >> t; if ((int)s.c.pack.size() < s.c.packMax) s.c.pack.push_back(items.loot(s.rng, t)); }
                        else if (verb == "removeitem") { std::string id; fs >> id; s.c.removeItem(id); }
                        else if (verb == "goto") { fs >> gotoId; }
                        else if (verb == "die") { s.c.dead = true; }
                        else if (verb == "finish") { s.c.dead = true; s.finishedWell = true; finishEvents[ev->id]++; }
                    }
                    if (s.c.hp <= 0) s.c.dead = true;
                    if (s.c.dead && s.c.hasTrait("blessed")) {
                        s.c.traits.erase("blessed");
                        s.c.dead = false;
                        s.c.hp = 1;
                    }
                    if (s.c.dead && !s.finishedWell) killerEvents[ev->id]++;
                    ev = gotoId.empty() ? nullptr : deck.find(gotoId);
                }
            }
        }
        deathDays.push_back(s.c.day);
        if (s.finishedWell) finished++;
        goldAtEnd += s.c.money;
    }

    std::sort(deathDays.begin(), deathDays.end());
    printf("==== PLAYTEST: %d runs, random policy ====\n", runs);
    printf("median run: %d days | p25: %d | p75: %d | reached day-60 cap: %d%%\n",
           deathDays[runs / 2], deathDays[runs / 4], deathDays[3 * runs / 4],
           (int)(100.0 * std::count(deathDays.begin(), deathDays.end(), 60) / runs));
    printf("completed lives (ascended/retired): %.1f%%\n", 100.0 * finished / runs);
    printf("avg gold at end: %lld\n", goldAtEnd / runs);

    std::vector<std::pair<int, std::string>> killers;
    for (auto& [id, n] : killerEvents) killers.push_back({n, id});
    std::sort(killers.rbegin(), killers.rend());
    printf("\n-- deadliest events --\n");
    for (int i = 0; i < (int)killers.size() && i < 10; i++)
        printf("%5d  %s\n", killers[i].first, killers[i].second.c_str());

    printf("\n-- completions by event --\n");
    for (auto& [id, n] : finishEvents) printf("%5d  %s\n", n, id.c_str());

    printf("\n-- unreached events --\n");
    int unreached = 0;
    // reconstruct the full id list by re-parsing (deck doesn't expose ids)
    std::string m = readFile(assets + "/data/events/manifest.json");
    size_t pos = 0;
    std::set<std::string> allIds;
    while ((pos = m.find(".json", pos)) != std::string::npos) {
        size_t start = m.rfind('"', pos);
        std::string fn = m.substr(start + 1, pos + 5 - start - 1);
        std::string t = readFile(assets + "/data/events/" + fn);
        size_t p2 = 0;
        while ((p2 = t.find("\"id\":", p2)) != std::string::npos) {
            size_t q1 = t.find('"', p2 + 5);
            size_t q2 = t.find('"', q1 + 1);
            allIds.insert(t.substr(q1 + 1, q2 - q1 - 1));
            p2 = q2;
        }
        pos += 5;
    }
    for (auto& id : allIds)
        if (!seenEvents.count(id)) {
            printf("       %s\n", id.c_str());
            unreached++;
        }
    if (!unreached) printf("       (none - every event fired at least once)\n");
    printf("\nevents seen: %d/%d\n", (int)seenEvents.size(), (int)allIds.size());
    return 0;
}
