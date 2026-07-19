#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "../src/character.h"
#include "../src/grammar.h"
#include "../src/language.h"
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
    std::printf("regression: character math, equipment, bags, and 5000 worlds pass\n");
    return 0;
}
