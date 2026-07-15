// Region graph worldgen — geography as a graph, not a map (WORLDGEN.md §1).
#pragma once
#include <string>
#include <vector>
#include "grammar.h"
#include "language.h"
#include "rng.h"

struct Site {
    std::string name;
    std::string type;   // city, tavern, dungeon, cave, grove
    std::string deck;   // event deck tag: city, tavern, dungeon, cave, forest
    int region = 0;
};

struct Region {
    std::string name;
    std::string biome;  // forest, desert, plains, swamp, mountains, coast
    std::vector<int> neighbors;
    std::vector<int> sites;
};

struct World {
    GenName name;
    std::vector<Region> regions;
    std::vector<Site> sites;
};

World GenerateWorld(uint64_t seed, const Grammar& grammar, const NameForge& forge);
