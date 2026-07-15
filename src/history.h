// The zero-player game (WORLDGEN.md §3). Simulates ~250 years over the
// generated world and produces the Chronicle: an append-only log of linked
// facts. Runtime content (rumors, books, artifact provenance) renders from it.
// Notable side effect: sacked cities become dungeons.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "grammar.h"
#include "language.h"
#include "rng.h"
#include "world.h"

struct Faction {
    std::string name;
    int home = -1; // region index
    std::vector<int> rel; // relation to each other faction, -100..100
};

struct Figure {
    std::string name;
    int faction = -1;
    std::string trait, profession;
    int born = 0, died = -1;
};

struct HArtifact {
    std::string conlang, meaning; // Zonlirash, "Oathbreaker"
    std::string material;
    int forgedBy = -1, forgedYear = 0;
    int restingSite = -1; // world site index
    bool claimed = false; // taken by the player this run
    std::vector<int> deeds; // chronicle entry ids
    std::string display() const {
        return meaning.empty() ? conlang : conlang + " \"" + meaning + "\"";
    }
};

struct Beast {
    std::string name;
    int region = -1;
    int kills = 0, died = -1;
};

struct ChronEntry {
    int id = 0, year = 0;
    std::string type; // artifact_forged, war_declared, city_sacked, ...
    int actor = -1, faction = -1, faction2 = -1;
    int site = -1, artifact = -1, beast = -1;
    int cause = -1;        // chronicle entry id that led to this one
    std::string extra;     // book title, plague name, cult name, region name...
};

// A war burning RIGHT NOW, while the player walks around in it.
struct LiveWar {
    int a = -1, b = -1;
    int daysLeft = 0;
    int declaredEntry = -1;
};

struct History {
    std::vector<Faction> factions;
    std::vector<Figure> figures;
    std::vector<HArtifact> artifacts;
    std::vector<Beast> beasts;
    std::vector<ChronEntry> chron;
    int years = 0;       // last simulated year
    int presentYear = 0; // the year the player walks in

    // The living world (P2): state that changes daily during play.
    std::vector<LiveWar> liveWars;
    std::map<int, int> plaguedRegions; // region index -> days remaining
    int liveStartId = 0; // chron entries >= this are NEWS, not history
};

// Mutates world: sacked cities become ruins (dungeon deck), sky debris adds
// crash sites.
History SimulateHistory(World& world, uint64_t seed, const Grammar& grammar,
                        const NameForge& forge);

// Continue an existing history for [fromYear+1 .. toYear] — the gap between
// player runs. Same rules, same world, new scars.
void SimulateYears(World& world, History& h, uint64_t seed, int fromYear,
                   int toYear, const Grammar& grammar, const NameForge& forge);

// One in-game day of the living world: wars ignite and burn, plagues spread
// and fade, artifacts get stolen, beasts eat someone's favorite scholar.
void SimulateLiveDay(World& world, History& h, Rng& rng, const Grammar& grammar,
                     const NameForge& forge);

std::string RenderChronEntry(const ChronEntry& e, const History& h, const World& w,
                             const Grammar& grammar, Rng& rng);
