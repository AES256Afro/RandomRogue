#include "history.h"
#include <algorithm>

// The five ages. Their fingerprints end up in every rumor and ruin.
const char* EraName(int year) {
    if (year <= 250) return "the Founding Age";
    if (year <= 450) return "the Long Peace";
    if (year <= 650) return "the Sundering";
    if (year <= 750) return "the Sky-Fall Century";
    return "the Present Age";
}

namespace {

struct EraMod {
    int war;      // percent multiplier (100 = baseline)
    int trade;
    int culturey; // books & monuments
    int plague;
    int debris;
};

EraMod eraMod(int year) {
    if (year <= 250) return {100, 100, 100, 100, 100};   // Founding
    if (year <= 450) return {15, 220, 250, 80, 100};     // Long Peace
    if (year <= 650) return {300, 40, 60, 160, 100};     // Sundering
    if (year <= 750) return {80, 100, 120, 100, 900};    // Sky-Fall Century
    return {100, 130, 150, 100, 60};                     // Present Age
}

int scaled(int basePct, int mult) {
    int v = basePct * mult / 100;
    return v < 0 ? 0 : (v > 95 ? 95 : v);
}

const std::vector<std::string>& traits() {
    static const std::vector<std::string> v = {
        "brave", "greedy", "petty", "kind", "paranoid", "scholarly",
        "ambitious", "humble", "spiteful", "flatulent", "unlucky", "smug"};
    return v;
}
const std::vector<std::string>& professions() {
    static const std::vector<std::string> v = {
        "hero", "smith", "scholar", "merchant", "cultist", "bandit"};
    return v;
}

struct Sim {
    World& w;
    History& h;
    Rng rng;
    const Grammar& g;
    const NameForge& forge;
    int year = 0;

    struct War { int a, b, endYear, declaredEntry; };
    std::vector<War> wars;

    Sim(World& world, History& hist, uint64_t seed, const Grammar& grammar,
        const NameForge& f)
        : w(world), h(hist), rng(seed, STREAM_HISTORY), g(grammar), forge(f) {}

    ChronEntry& log(const std::string& type) {
        ChronEntry e;
        e.id = (int)h.chron.size();
        e.year = year;
        e.type = type;
        h.chron.push_back(e);
        return h.chron.back();
    }

    int aliveFigures() {
        int n = 0;
        for (auto& f : h.figures)
            if (f.died < 0) n++;
        return n;
    }

    int randomAliveFigure() {
        std::vector<int> pool;
        for (int i = 0; i < (int)h.figures.size(); i++)
            if (h.figures[i].died < 0) pool.push_back(i);
        return pool.empty() ? -1 : pool[rng.range(0, (int)pool.size() - 1)];
    }

    int randomSiteInRegionOrAnywhere(int region) {
        std::vector<int> pool;
        for (int i = 0; i < (int)w.sites.size(); i++)
            if (w.sites[i].region == region) pool.push_back(i);
        if (pool.empty())
            for (int i = 0; i < (int)w.sites.size(); i++) pool.push_back(i);
        return pool.empty() ? -1 : pool[rng.range(0, (int)pool.size() - 1)];
    }

    void newFigure(int faction, bool announce) {
        Figure f;
        int culture = (faction >= 0 && faction < (int)h.factions.size())
                          ? h.factions[faction].culture : CULT_OLD;
        f.name = forge.person(rng, culture).conlang;
        f.faction = faction;
        f.trait = rng.pick(traits());
        f.profession = rng.pick(professions());
        f.born = year;
        h.figures.push_back(f);
        if (announce) {
            ChronEntry& e = log("figure_rise");
            e.actor = (int)h.figures.size() - 1;
            e.faction = faction;
        }
    }

    void setup() {
        int nf = rng.range(4, 6);
        std::vector<int> homes;
        for (int i = 0; i < nf; i++) {
            Faction f;
            // Cultures: mostly old-tongue, with a goblin band, swamp folk,
            // or a liturgical order in the mix. Spacers only arrive with
            // the Sky-Fall (their names come via crash sites, not banners).
            static const int kCulturePool[6] = {CULT_OLD, CULT_OLD, CULT_OLD,
                                                CULT_GOBLIN, CULT_SWAMP,
                                                CULT_LITURGICAL};
            f.culture = kCulturePool[rng.range(0, 5)];
            for (int attempt = 0; attempt < 12; attempt++) {
                std::string placeword = forge.place(rng, f.culture).conlang;
                f.name = g.expand("{faction_name}", rng, {{"placeword", placeword}});
                bool taken = false;
                for (auto& other : h.factions)
                    if (other.name == f.name) taken = true;
                if (!taken) break;
            }
            f.home = rng.range(0, (int)w.regions.size() - 1);
            h.factions.push_back(f);
        }
        for (auto& f : h.factions) f.rel.assign(nf, 0);
        for (int i = 0; i < nf; i++) {
            ChronEntry& e = log("faction_founded");
            e.faction = i;
            e.extra = w.regions[h.factions[i].home].name;
            for (int k = 0; k < 3; k++) newFigure(i, false);
        }
        int nb = rng.range(3, 5);
        for (int i = 0; i < nb; i++) {
            Beast b;
            b.name = g.expand("{beast_name}", rng);
            b.region = rng.range(0, (int)w.regions.size() - 1);
            h.beasts.push_back(b);
        }
        // The pantheon: named in the liturgical tongue, moody by nature.
        int ng = rng.range(3, 5);
        for (int i = 0; i < ng; i++) {
            God god;
            std::string raw = forge.word(rng, rng.range(2, 3), CULT_LITURGICAL);
            raw[0] = (char)(raw[0] >= 'a' && raw[0] <= 'z' ? raw[0] - 'a' + 'A' : raw[0]);
            god.name = raw;
            god.domain = g.expand("{god_domain}", rng);
            god.mood = rng.range(-1, 1);
            h.gods.push_back(god);
        }
    }

    void figureActs(int fi) {
        Figure& f = h.figures[fi];
        const std::string& p = f.profession;
        if (p == "smith") {
            GenName an = forge.artifact(rng);
            HArtifact a;
            a.conlang = an.conlang;
            a.meaning = an.meaning;
            a.material = g.expand("{material}", rng);
            a.forgedBy = fi;
            a.forgedYear = year;
            a.restingSite = randomSiteInRegionOrAnywhere(h.factions[f.faction].home);
            h.artifacts.push_back(a);
            ChronEntry& e = log("artifact_forged");
            e.actor = fi;
            e.artifact = (int)h.artifacts.size() - 1;
            e.site = a.restingSite;
            h.artifacts.back().deeds.push_back(e.id);
        } else if (p == "hero") {
            std::vector<int> alive;
            for (int i = 0; i < (int)h.beasts.size(); i++)
                if (h.beasts[i].died < 0) alive.push_back(i);
            if (alive.empty()) return;
            int bi = alive[rng.range(0, (int)alive.size() - 1)];
            // A hero may take up an artifact resting nearby; it tips the odds
            // and the deed attaches to the artifact's provenance forever.
            int wielded = -1;
            for (int i = 0; i < (int)h.artifacts.size(); i++) {
                int rs = h.artifacts[i].restingSite;
                if (rs >= 0 && rs < (int)w.sites.size() &&
                    w.sites[rs].region == h.factions[f.faction].home) { wielded = i; break; }
            }
            int slayChance = (wielded >= 0 && rng.chance(60)) ? 70 : 45;
            if (slayChance == 70) {
                if (rng.chance(70)) {
                    h.beasts[bi].died = year;
                    ChronEntry& e = log("artifact_slaying");
                    e.actor = fi;
                    e.beast = bi;
                    e.artifact = wielded;
                    h.artifacts[wielded].deeds.push_back(e.id);
                } else {
                    f.died = year;
                    h.beasts[bi].kills++;
                    ChronEntry& e = log("figure_eaten");
                    e.actor = fi;
                    e.beast = bi;
                }
            } else if (rng.chance(45)) {
                h.beasts[bi].died = year;
                ChronEntry& e = log("beast_slain");
                e.actor = fi;
                e.beast = bi;
            } else {
                f.died = year;
                h.beasts[bi].kills++;
                ChronEntry& e = log("figure_eaten");
                e.actor = fi;
                e.beast = bi;
            }
        } else if (p == "scholar") {
            ChronEntry& e = log("book_written");
            e.actor = fi;
            e.extra = g.expand("{book_title}", rng);
        } else if (p == "merchant") {
            if (!rng.chance(scaled(60, eraMod(year).trade))) return;
            int other = rng.range(0, (int)h.factions.size() - 1);
            if (other == f.faction) return;
            h.factions[f.faction].rel[other] += 15;
            h.factions[other].rel[f.faction] += 15;
            ChronEntry& e = log("trade_pact");
            e.faction = f.faction;
            e.faction2 = other;
        } else if (p == "cultist") {
            ChronEntry& e = log("cult_founded");
            e.actor = fi;
            e.extra = g.expand("{cult_name}", rng);
            f.profession = "cult leader"; // once is plenty
        } else if (p == "bandit") {
            std::vector<int> loose;
            for (int i = 0; i < (int)h.artifacts.size(); i++)
                if (h.artifacts[i].restingSite >= 0) loose.push_back(i);
            if (loose.empty()) return;
            int ai = loose[rng.range(0, (int)loose.size() - 1)];
            h.artifacts[ai].restingSite = rng.range(0, (int)w.sites.size() - 1);
            ChronEntry& e = log("artifact_stolen");
            e.actor = fi;
            e.artifact = ai;
            e.site = h.artifacts[ai].restingSite;
            h.artifacts[ai].deeds.push_back(e.id);
        }
    }

    void factionPolitics() {
        int nf = (int)h.factions.size();
        for (int a = 0; a < nf; a++) {
            for (int b = a + 1; b < nf; b++) {
                h.factions[a].rel[b] += rng.range(-3, 2);
                h.factions[b].rel[a] = h.factions[a].rel[b];
                bool atWar = false;
                for (auto& war : wars)
                    if ((war.a == a && war.b == b)) atWar = true;
                if (!atWar && h.factions[a].rel[b] < -45 &&
                    rng.chance(scaled(30, eraMod(year).war))) {
                    ChronEntry& e = log("war_declared");
                    e.faction = a;
                    e.faction2 = b;
                    wars.push_back({a, b, year + rng.range(3, 8), e.id});
                }
            }
        }
        for (size_t i = 0; i < wars.size();) {
            War& war = wars[i];
            if (rng.chance(30)) {
                // A battle; loser may have a city sacked. Ruins are future dungeons.
                int loser = rng.chance(50) ? war.a : war.b;
                int winner = (loser == war.a) ? war.b : war.a;
                std::vector<int> cities;
                for (int s = 0; s < (int)w.sites.size(); s++)
                    if (w.sites[s].type == "city" &&
                        w.sites[s].region == h.factions[loser].home)
                        cities.push_back(s);
                if (!cities.empty() && rng.chance(60)) {
                    int s = cities[rng.range(0, (int)cities.size() - 1)];
                    ChronEntry& e = log("city_sacked");
                    e.faction = winner;
                    e.faction2 = loser;
                    e.site = s;
                    e.cause = war.declaredEntry;
                    w.sites[s].name = "The Ruins of " + w.sites[s].name;
                    w.sites[s].type = "ruins";
                    w.sites[s].deck = "dungeon";
                }
            }
            if (year >= war.endYear) {
                ChronEntry& e = log("peace_signed");
                e.faction = war.a;
                e.faction2 = war.b;
                e.cause = war.declaredEntry;
                h.factions[war.a].rel[war.b] = -10;
                h.factions[war.b].rel[war.a] = -10;
                wars.erase(wars.begin() + i);
            } else {
                i++;
            }
        }
    }

    void disasters() {
        EraMod mod = eraMod(year);
        if (rng.chance(scaled(2, mod.plague))) {
            std::string plague = g.expand("{plague_name}", rng);
            ChronEntry& e = log("plague");
            e.extra = plague;
            for (auto& f : h.figures)
                if (f.died < 0 && rng.chance(20)) f.died = year;
        }
        int crashCount = 0;
        for (auto& s : w.sites)
            if (s.type == "crash") crashCount++;
        if (crashCount < 8 && rng.chance(scaled(1, mod.debris))) {
            // Sky debris: sci-fi enters the world, canonically. During the
            // Sky-Fall Century it POURS. Crash sites carry spacer names.
            Site s;
            s.type = "crash";
            s.deck = "crash";
            s.region = rng.range(0, (int)w.regions.size() - 1);
            s.name = g.expand("{crash_name}", rng,
                              {{"placeword", forge.place(rng, CULT_SPACER).conlang}});
            w.regions[s.region].sites.push_back((int)w.sites.size());
            w.sites.push_back(s);
            ChronEntry& e = log("sky_debris");
            e.site = (int)w.sites.size() - 1;
        }
        // Old beasts die; new ones rise, or nothing would be left to slay.
        int aliveBeasts = 0;
        for (auto& b : h.beasts)
            if (b.died < 0) aliveBeasts++;
        if (aliveBeasts < 4 && rng.chance(2)) {
            Beast b;
            b.name = g.expand("{beast_name}", rng);
            b.region = rng.range(0, (int)w.regions.size() - 1);
            h.beasts.push_back(b);
            ChronEntry& e = log("beast_risen");
            e.beast = (int)h.beasts.size() - 1;
        }
        for (int bi = 0; bi < (int)h.beasts.size(); bi++) {
            if (h.beasts[bi].died >= 0 || !rng.chance(4)) continue;
            int fi = randomAliveFigure();
            if (fi < 0) continue;
            h.figures[fi].died = year;
            h.beasts[bi].kills++;
            ChronEntry& e = log("figure_eaten");
            e.actor = fi;
            e.beast = bi;
        }
    }

    void society() {
        EraMod mod = eraMod(year);
        // Monuments: pure flavor, and future rumor fodder.
        for (int fa = 0; fa < (int)h.factions.size(); fa++) {
            if (!rng.chance(scaled(1, mod.culturey))) continue;
            ChronEntry& e = log("monument_built");
            e.faction = fa;
            e.extra = g.expand("{monument_name}", rng);
        }
        // Duels: two notable people, one grudge, zero mediation.
        if (rng.chance(3)) {
            int a = randomAliveFigure();
            int b = randomAliveFigure();
            if (a >= 0 && b >= 0 && a != b) {
                h.figures[b].died = year;
                ChronEntry& e = log("duel_fought");
                e.actor = a;
                e.extra = h.figures[b].name;
            }
        }
        // Exiles: the petty and the spiteful eventually wear out their welcome.
        if (rng.chance(2)) {
            std::vector<int> pool;
            for (int i = 0; i < (int)h.figures.size(); i++) {
                const Figure& f = h.figures[i];
                if (f.died < 0 && (f.trait == "petty" || f.trait == "spiteful" || f.trait == "smug"))
                    pool.push_back(i);
            }
            if (!pool.empty() && h.factions.size() > 1) {
                int fi = pool[rng.range(0, (int)pool.size() - 1)];
                int oldFaction = h.figures[fi].faction;
                int newFaction = oldFaction;
                while (newFaction == oldFaction)
                    newFaction = rng.range(0, (int)h.factions.size() - 1);
                h.figures[fi].faction = newFaction;
                ChronEntry& e = log("figure_exiled");
                e.actor = fi;
                e.faction = oldFaction;
                e.faction2 = newFaction;
            }
        }
    }

    void agingAndBirths() {
        for (int i = 0; i < (int)h.figures.size(); i++) {
            Figure& f = h.figures[i];
            if (f.died >= 0) continue;
            if (year - f.born > 55 && rng.chance(15)) {
                f.died = year;
                ChronEntry& e = log("figure_died");
                e.actor = i;
            }
        }
        if (aliveFigures() < 60)
            for (int fa = 0; fa < (int)h.factions.size(); fa++)
                if (rng.chance(14)) newFigure(fa, true);
    }

    void run(int fromYear, int toYear) {
        for (year = fromYear; year <= toYear; year++) {
            for (int i = 0; i < (int)h.figures.size(); i++)
                if (h.figures[i].died < 0 && rng.chance(6)) figureActs(i);
            factionPolitics();
            disasters();
            society();
            agingAndBirths();
        }
        h.years = toYear;
        h.presentYear = toYear;
    }
};

} // namespace

History SimulateHistory(World& world, uint64_t seed, const Grammar& grammar,
                        const NameForge& forge) {
    History h;
    Sim sim(world, h, seed, grammar, forge);
    sim.setup();
    sim.run(1, kHistoryYears);
    return h;
}

void SimulateYears(World& world, History& h, uint64_t seed, int fromYear,
                   int toYear, const Grammar& grammar, const NameForge& forge) {
    Sim sim(world, h, seed, grammar, forge);
    sim.run(fromYear, toYear);
}

// The living world. Probabilities are tuned so a 30-day run sees a handful of
// headlines, not an apocalypse. Entries land at presentYear and count as NEWS.
void SimulateLiveDay(World& world, History& h, Rng& rng, const Grammar& grammar,
                     const NameForge& forge) {
    (void)forge;
    int year = h.presentYear;
    auto log = [&](const std::string& type) -> ChronEntry& {
        ChronEntry e;
        e.id = (int)h.chron.size();
        e.year = year;
        e.type = type;
        h.chron.push_back(e);
        return h.chron.back();
    };
    auto aliveFigure = [&]() {
        std::vector<int> pool;
        for (int i = 0; i < (int)h.figures.size(); i++)
            if (h.figures[i].died < 0) pool.push_back(i);
        return pool.empty() ? -1 : pool[rng.range(0, (int)pool.size() - 1)];
    };

    // Wars ignite...
    if ((int)h.liveWars.size() < 2 && h.factions.size() > 1 && rng.chance(4)) {
        int a = rng.range(0, (int)h.factions.size() - 1);
        int b = a;
        while (b == a) b = rng.range(0, (int)h.factions.size() - 1);
        ChronEntry& e = log("war_declared");
        e.faction = a;
        e.faction2 = b;
        h.liveWars.push_back({a, b, rng.range(12, 35), e.id});
    }
    // ...burn...
    for (size_t i = 0; i < h.liveWars.size();) {
        LiveWar& war = h.liveWars[i];
        if (rng.chance(6)) {
            int loser = rng.chance(50) ? war.a : war.b;
            int winner = (loser == war.a) ? war.b : war.a;
            std::vector<int> cities;
            for (int s = 0; s < (int)world.sites.size(); s++)
                if (world.sites[s].type == "city" &&
                    world.sites[s].region == h.factions[loser].home)
                    cities.push_back(s);
            if (!cities.empty()) {
                int s = cities[rng.range(0, (int)cities.size() - 1)];
                ChronEntry& e = log("city_sacked");
                e.faction = winner;
                e.faction2 = loser;
                e.site = s;
                e.cause = war.declaredEntry;
                world.sites[s].name = "The Ruins of " + world.sites[s].name;
                world.sites[s].type = "ruins";
                world.sites[s].deck = "dungeon";
            }
        }
        if (--war.daysLeft <= 0) {
            ChronEntry& e = log("peace_signed");
            e.faction = war.a;
            e.faction2 = war.b;
            e.cause = war.declaredEntry;
            h.liveWars.erase(h.liveWars.begin() + i);
        } else {
            i++;
        }
    }
    // Plagues spread and fade.
    if (h.plaguedRegions.size() < 2 && rng.chance(3)) {
        int r = rng.range(0, (int)world.regions.size() - 1);
        if (!h.plaguedRegions.count(r)) {
            h.plaguedRegions[r] = rng.range(8, 20);
            ChronEntry& e = log("plague");
            e.extra = grammar.expand("{plague_name}", rng) + " (in " +
                      world.regions[r].name + ")";
        }
    }
    for (auto it = h.plaguedRegions.begin(); it != h.plaguedRegions.end();) {
        if (--it->second <= 0) it = h.plaguedRegions.erase(it);
        else ++it;
    }
    // Petty history continues without you.
    if (rng.chance(3)) {
        std::vector<int> loose;
        for (int i = 0; i < (int)h.artifacts.size(); i++)
            if (h.artifacts[i].restingSite >= 0 && !h.artifacts[i].claimed)
                loose.push_back(i);
        int fi = aliveFigure();
        if (!loose.empty() && fi >= 0) {
            int ai = loose[rng.range(0, (int)loose.size() - 1)];
            h.artifacts[ai].restingSite = rng.range(0, (int)world.sites.size() - 1);
            ChronEntry& e = log("artifact_stolen");
            e.actor = fi;
            e.artifact = ai;
            e.site = h.artifacts[ai].restingSite;
            h.artifacts[ai].deeds.push_back(e.id);
        }
    }
    if (rng.chance(2)) {
        int fi = aliveFigure();
        std::vector<int> beasts;
        for (int i = 0; i < (int)h.beasts.size(); i++)
            if (h.beasts[i].died < 0) beasts.push_back(i);
        if (fi >= 0 && !beasts.empty()) {
            int bi = beasts[rng.range(0, (int)beasts.size() - 1)];
            h.figures[fi].died = year;
            h.beasts[bi].kills++;
            ChronEntry& e = log("figure_eaten");
            e.actor = fi;
            e.beast = bi;
        }
    }
    if (rng.chance(1)) {
        int fi = aliveFigure();
        if (fi >= 0) {
            ChronEntry& e = log("book_written");
            e.actor = fi;
            e.extra = grammar.expand("{book_title}", rng);
        }
    }
}

std::string RenderChronEntry(const ChronEntry& e, const History& h, const World& w,
                             const Grammar& grammar, Rng& rng) {
    Grammar::Ctx ctx;
    ctx["year"] = std::to_string(e.year);
    ctx["era"] = EraName(e.year);
    ctx["extra"] = e.extra;
    if (e.actor >= 0 && e.actor < (int)h.figures.size()) {
        ctx["actor"] = h.figures[e.actor].name;
        ctx["trait"] = h.figures[e.actor].trait;
        ctx["profession"] = h.figures[e.actor].profession;
    }
    if (e.faction >= 0 && e.faction < (int)h.factions.size())
        ctx["faction"] = h.factions[e.faction].name;
    if (e.faction2 >= 0 && e.faction2 < (int)h.factions.size())
        ctx["faction2"] = h.factions[e.faction2].name;
    if (e.site >= 0 && e.site < (int)w.sites.size())
        ctx["site"] = w.sites[e.site].name;
    if (e.artifact >= 0 && e.artifact < (int)h.artifacts.size())
        ctx["artifact"] = h.artifacts[e.artifact].display();
    if (e.beast >= 0 && e.beast < (int)h.beasts.size())
        ctx["beast"] = h.beasts[e.beast].name;
    std::string key = "chron_" + e.type;
    if (!grammar.has(key)) return "In year " + ctx["year"] + ", something " + e.type + " happened.";
    return grammar.expand("{" + key + "}", rng, ctx);
}
