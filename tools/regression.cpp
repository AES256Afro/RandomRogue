#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "../src/character.h"
#include "../src/grammar.h"
#include "../src/language.h"
#include "../src/story.h"
#include "../src/world.h"

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}

static bool expect(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "regression failed: %s\n", message);
    return false;
}

int main(int argc, char** argv) {
    std::string assets = argc > 1 ? argv[1] : "assets";
    Grammar grammar;
    NameForge forge;
    std::string prose = readFile(assets + "/data/recipes/prose.json");
    std::string language = readFile(assets + "/data/recipes/language.json");
    if (prose.empty() || language.empty()) {
        std::fprintf(stderr, "regression failed: recipes not found\n");
        return 1;
    }
    grammar.loadJsonText(prose.c_str());
    forge.loadJsonText(language.c_str());

    bool ok = true;
    ok &= expect(Character::mod(9) == -1, "stat 9 must have modifier -1");
    ok &= expect(Character::mod(7) == -2, "stat 7 must have modifier -2");
    ok &= expect(Character::mod(12) == 1, "stat 12 must have modifier +1");

    Character c;
    ItemInstance weak, strong, trinket, satchel;
    weak.type = "weapon"; weak.passive = "check str +1";
    strong.type = "weapon"; strong.passive = "check str +3";
    trinket.type = "misc"; trinket.passive = "check str +1";
    satchel.templateId = "satchel"; satchel.type = "bag";
    c.pack = {weak, strong, trinket, satchel};
    ok &= expect(c.checkBonus(STAT_STR) == 4,
                 "only the best weapon plus trinkets may affect a check");
    ok &= expect(c.capacity() == c.packMax + 2,
                 "a satchel must increase carrying capacity");
    ItemInstance reinforced;
    reinforced.templateId = "reinforced_satchel";
    reinforced.type = "bag";
    c.pack = {reinforced};
    ok &= expect(c.capacity() == c.packMax + 4,
                 "a reinforced satchel must increase carrying capacity by four");

    Event relationship;
    relationship.id = "relationship_test";
    relationship.locations = {"city"};
    relationship.tags = {"relationship", "social"};
    relationship.family = "meeting";
    Choice mercy;
    mercy.approach = "mercy";
    relationship.choices.push_back(mercy);
    Event unrelated;
    unrelated.id = "unrelated_test";
    unrelated.locations = {"city"};
    unrelated.tags = {"economy"};
    unrelated.family = "market";
    unrelated.choices.push_back(Choice{});
    StoryDirector director;
    StoryContext story;
    story.knownNpc = true;
    int connected = director.score(relationship, story);
    int neutral = director.score(unrelated, story);
    ok &= expect(connected > neutral,
                 "known NPCs must raise relationship event relevance");
    story.activeFamilies.insert("meeting");
    story.arcBeatDue = true;
    ok &= expect(director.score(relationship, story) > connected + 20,
                 "a due active arc must reserve strong Director priority");
    std::vector<std::string> explanation = director.explain(relationship, story);
    bool explainsScore = false, explainsContext = false;
    for (const std::string& line : explanation) {
        if (line.find("SCORE") != std::string::npos) explainsScore = true;
        if (line.find("+relationship") != std::string::npos) explainsContext = true;
    }
    ok &= expect(explainsScore && explainsContext,
                  "Director diagnostics must explain score and context without outcomes");
    Event labor = unrelated;
    labor.id = "labor_test";
    labor.family = "organizing";
    labor.tags = {"labor", "solidarity"};
    StoryContext political;
    int laborBase = director.score(labor, political);
    political.solidarity = true;
    ok &= expect(director.score(labor, political) >= laborBase + 30,
                 "organized regions must raise the relevance of labor stories");
    Event ecology = unrelated;
    ecology.id = "ecology_test";
    ecology.family = "pollution";
    ecology.tags = {"ecology"};
    StoryContext environmental;
    int ecologyBase = director.score(ecology, environmental);
    environmental.pollution = true;
    ok &= expect(director.score(ecology, environmental) >= ecologyBase + 38,
                 "polluted regions must raise the relevance of ecology stories");
    story.activeFamilies.clear();
    story.arcBeatDue = false;
    director.record(relationship, &relationship.choices[0], 2);
    ok &= expect(director.score(relationship, story) < director.score(unrelated, story),
                 "recent event families must cool down below fresh material");

    EventDeck directedDeck;
    directedDeck.loadJsonText(
        "[{\"id\":\"a\",\"locations\":[\"road\"],\"choices\":[{\"text\":\"a\","
        "\"outcomes\":[{\"text\":\"a\"}]}]},"
        "{\"id\":\"b\",\"locations\":[\"road\"],\"choices\":[{\"text\":\"b\","
        "\"outcomes\":[{\"text\":\"b\"}]}]}]");
    Rng directedRng(12, 34);
    const Event* directed = directedDeck.draw(
        directedRng, "road", nullptr,
        [](const Event& event) { return event.id == "b" ? 100 : 0; });
    ok &= expect(directed && directed->id == "b",
                 "the event deck must honor director eligibility scores");

    EventDeck noRepeatDeck;
    noRepeatDeck.loadJsonText(
        "[{\"id\":\"first\",\"locations\":[\"tavern\"],\"choices\":[{\"text\":\"a\","
        "\"outcomes\":[{\"text\":\"a\"}]}]},"
        "{\"id\":\"second\",\"locations\":[\"tavern\"],\"choices\":[{\"text\":\"b\","
        "\"outcomes\":[{\"text\":\"b\"}]}]}]");
    Rng noRepeatRng(56, 78);
    const Event* firstDraw = noRepeatDeck.draw(noRepeatRng, "tavern");
    ok &= expect(firstDraw != nullptr, "a fresh location must deal a card");
    if (firstDraw) noRepeatDeck.markUsed(firstDraw->id);
    const Event* secondDraw = noRepeatDeck.draw(noRepeatRng, "tavern");
    ok &= expect(secondDraw && firstDraw && secondDraw->id != firstDraw->id,
                 "a location must choose unseen material while any remains");
    if (secondDraw) noRepeatDeck.markUsed(secondDraw->id);
    ok &= expect(noRepeatDeck.draw(noRepeatRng, "tavern") == nullptr,
                 "an exhausted location must not recycle cards during one life");

    EventDeck worldCooldownDeck;
    worldCooldownDeck.loadJsonText(
        "[{\"id\":\"old\",\"locations\":[\"cave\"],\"choices\":[{\"text\":\"a\","
        "\"outcomes\":[{\"text\":\"a\"}]}]},"
        "{\"id\":\"fresh\",\"locations\":[\"cave\"],\"choices\":[{\"text\":\"b\","
        "\"outcomes\":[{\"text\":\"b\"}]}]}]");
    worldCooldownDeck.setCooldown({"old"});
    Rng cooldownRng(90, 12);
    const Event* freshDraw = worldCooldownDeck.draw(cooldownRng, "cave");
    ok &= expect(freshDraw && freshDraw->id == "fresh",
                 "world memory must prefer a card not seen by an earlier life");
    if (freshDraw) worldCooldownDeck.markUsed(freshDraw->id);
    const Event* relaxedDraw = worldCooldownDeck.draw(cooldownRng, "cave");
    ok &= expect(relaxedDraw && relaxedDraw->id == "old",
                 "world memory may relax rather than empty an old location");

    static const std::set<std::string> required = {
        "plains", "coast", "forest", "mountains", "swamp", "desert"
    };
    for (uint64_t seed = 1; seed <= 5000; seed++) {
        World w = GenerateWorld(seed, grammar, forge);
        std::set<std::string> found;
        for (auto& region : w.regions) found.insert(region.biome);
        if (found != required) {
            std::fprintf(stderr, "regression failed: seed %llu lacks a biome\n",
                         (unsigned long long)seed);
            ok = false;
            break;
        }
    }

    if (!ok) return 1;
    std::printf("regression: character math, equipment, story direction, and 5000 worlds pass\n");
    return 0;
}
