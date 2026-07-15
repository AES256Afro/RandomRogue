// chronicle_dump — generate a world from a seed and print its entire history.
// THE worldgen dev tool (WORLDGEN.md §7): if this dump is fun to read, the
// game will be. Usage: chronicle_dump [seed] [assets_dir]
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/grammar.h"
#include "../src/history.h"
#include "../src/language.h"
#include "../src/rng.h"
#include "../src/world.h"

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    uint64_t seed = (argc > 1) ? strtoull(argv[1], nullptr, 10) : 12345;
    std::string assets = (argc > 2) ? argv[2] : "assets";

    Grammar grammar;
    NameForge forge;
    std::string prose = readFile(assets + "/data/recipes/prose.json");
    std::string lang = readFile(assets + "/data/recipes/language.json");
    if (prose.empty() || lang.empty()) {
        fprintf(stderr, "cannot read recipes under '%s' (pass assets dir as arg 2)\n",
                assets.c_str());
        return 1;
    }
    grammar.loadJsonText(prose.c_str());
    forge.loadJsonText(lang.c_str());

    World world = GenerateWorld(seed, grammar, forge);
    History h = SimulateHistory(world, seed, grammar, forge);
    Rng rng(seed, STREAM_RUNTIME);

    printf("==== THE WORLD OF %s, %s ====\n", world.name.conlang.c_str(),
           world.name.meaning.c_str());
    printf("seed %llu | %d regions, %d sites | %d years simulated\n\n",
           (unsigned long long)seed, (int)world.regions.size(),
           (int)world.sites.size(), h.years);

    printf("---- GEOGRAPHY ----\n");
    for (auto& r : world.regions) {
        printf("%s (%s)\n", r.name.c_str(), r.biome.c_str());
        for (int si : r.sites)
            printf("    - %s (%s)\n", world.sites[si].name.c_str(),
                   world.sites[si].type.c_str());
    }

    printf("\n---- FACTIONS ----\n");
    for (auto& f : h.factions)
        printf("%s, of %s\n", f.name.c_str(), world.regions[f.home].name.c_str());

    printf("\n---- THE CHRONICLE (%d entries) ----\n", (int)h.chron.size());
    for (auto& e : h.chron)
        printf("%s\n", RenderChronEntry(e, h, world, grammar, rng).c_str());

    printf("\n---- SURVIVING ARTIFACTS ----\n");
    for (auto& a : h.artifacts)
        if (a.restingSite >= 0)
            printf("%s (%s), resting at %s\n", a.display().c_str(),
                   a.material.c_str(), world.sites[a.restingSite].name.c_str());

    printf("\n---- LIVING LEGENDS ----\n");
    for (auto& f : h.figures)
        if (f.died < 0)
            printf("%s, %s %s of %s\n", f.name.c_str(), f.trait.c_str(),
                   f.profession.c_str(), h.factions[f.faction].name.c_str());
    for (auto& b : h.beasts)
        if (b.died < 0)
            printf("%s, still at large (%d kills)\n", b.name.c_str(), b.kills);
    return 0;
}
