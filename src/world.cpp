#include "world.h"
#include <array>

namespace {

struct BiomeDef {
    const char* biome;
    // site types this biome can host: {type, deckTag}
    std::vector<std::pair<const char*, const char*>> siteTypes;
};

const std::vector<BiomeDef>& biomes() {
    static const std::vector<BiomeDef> defs = {
        {"plains",    {{"city", "city"}, {"tavern", "tavern"}}},
        {"coast",     {{"city", "city"}, {"tavern", "tavern"}}},
        {"forest",    {{"grove", "forest"}, {"tavern", "tavern"}, {"cave", "cave"}}},
        {"mountains", {{"cave", "cave"}, {"dungeon", "dungeon"}}},
        {"swamp",     {{"dungeon", "dungeon"}, {"cave", "cave"}}},
        {"desert",    {{"dungeon", "dungeon"}, {"city", "city"}}},
    };
    return defs;
}

// Biome pairs that shouldn't sit next to each other.
bool badNeighbors(const std::string& a, const std::string& b) {
    return (a == "desert" && b == "swamp") || (a == "swamp" && b == "desert");
}

} // namespace

World GenerateWorld(uint64_t seed, const Grammar& grammar, const NameForge& forge) {
    Rng rng(seed, STREAM_WORLD);
    World w;
    w.name = forge.place(rng);

    int regionCount = rng.range(28, 36); // the wide world (ROADMAP P6)
    w.regions.resize(regionCount);

    // Chain the regions, then sprinkle extra edges — connected, slightly tangled.
    for (int i = 1; i < regionCount; i++) {
        w.regions[i - 1].neighbors.push_back(i);
        w.regions[i].neighbors.push_back(i - 1);
    }
    for (int i = 0; i < regionCount / 3; i++) {
        int a = rng.range(0, regionCount - 1);
        int b = rng.range(0, regionCount - 1);
        if (a != b) {
            w.regions[a].neighbors.push_back(b);
            w.regions[b].neighbors.push_back(a);
        }
    }

    // Biomes with the adjacency constraint (a few retries, then whatever — the
    // world is allowed to be a little wrong; that's flavor).
    for (int i = 0; i < regionCount; i++) {
        Region& r = w.regions[i];
        for (int attempt = 0; attempt < 8; attempt++) {
            const BiomeDef& b = rng.pick(biomes());
            bool ok = true;
            for (int n : r.neighbors)
                if (n < i && badNeighbors(w.regions[n].biome, b.biome)) ok = false;
            if (ok || attempt == 7) { r.biome = b.biome; break; }
        }
        r.name = grammar.expand("The {adj} {noun_" + r.biome + "}", rng);
    }

    // Sites: 1-2 per region, drawn from what the biome can host.
    for (int i = 0; i < regionCount; i++) {
        Region& r = w.regions[i];
        const BiomeDef* def = nullptr;
        for (auto& b : biomes())
            if (r.biome == b.biome) def = &b;
        if (!def) continue;

        int siteCount = rng.range(1, 2);
        for (int s = 0; s < siteCount; s++) {
            auto& [type, deck] = def->siteTypes[rng.next() % def->siteTypes.size()];
            Site site;
            site.type = type;
            site.deck = deck;
            site.region = i;
            if (site.type == "city") {
                site.name = forge.place(rng).conlang;
            } else if (site.type == "tavern") {
                site.name = grammar.expand("{tavern_name}", rng);
            } else if (site.type == "dungeon") {
                site.name = grammar.expand("{dungeon_name}", rng,
                                           {{"placeword", forge.place(rng).conlang}});
            } else if (site.type == "cave") {
                site.name = grammar.expand("The {adj} Cave", rng);
            } else { // grove
                site.name = grammar.expand("The {adj} Grove", rng);
            }
            r.sites.push_back((int)w.sites.size());
            w.sites.push_back(site);
        }
    }
    return w;
}
