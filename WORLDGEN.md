# Random Rogue — Worldgen & Chronicle System ("The Recipe")

Companion to [PLAN.md](PLAN.md). Goal: a Dwarf Fortress–style generator,
narrowed to what a choice-driven text game actually consumes:
**names, people, places, histories, and descriptions.** No tile maps, no
organs, no pathfinding. We simulate just enough history to produce a
**Chronicle** — a queryable log of facts — and every card, item, NPC, book,
and rumor at runtime is rendered *from* that log.

The DF trick we're stealing: the game never writes stories. It writes a
logbook during generation, then paraphrases entries from it forever. Books,
tavern gossip, item provenance, NPC grudges — all free content once the log
exists.

---

## 0. Determinism: One Seed to Rule Them

- One master seed (typed in or random) → hashed into independent **streams**:
  `geo`, `lang`, `history`, `runtime`. (PCG32 or SplitMix64 — tiny,
  reproducible everywhere including WASM.)
- Same seed = same world, same history, same names, on every platform.
  Different runtime stream = same world can be *played* differently.
- Sharing a seed shares the whole world → daily runs, "explore my world"
  posts.

## 1. Phase A — World Skeleton (geography as a graph, not a map)

Since nothing is rendered, skip noise-based tile maps. Generate a
**region graph**:

- 40–80 regions, each: biome (mountain, forest, desert, plains, swamp,
  coast, cave-riddled hills…), climate tags (cold/temperate/hot, wet/dry),
  and edges to neighbors.
- Adjacency rules keep it plausible: deserts don't touch swamps without a
  scrubland between; rivers are edge-paths from mountains to coast; cold
  clusters at one "pole" of the graph.
- **Sites** get placed into regions by suitability rules: cities on rivers/
  coasts, dwarf-style holds in mountains, taverns on roads between cities,
  caves and ruins anywhere, **hidden areas** flagged `undiscovered` (only
  reachable via clues/maps found at runtime).
- Optional dev tool: dump the graph to an ASCII/Graphviz map for debugging.
  Never shown to players — they experience geography as travel choices and
  flavor ("three days through the Saltmarsh Fringe").

Output: `World { regions[], sites[], roads[] }`.

## 2. Phase B — Language & Names (cheap, enormous payoff)

DF-style constructed vocabularies. Per culture:

- A **phoneme/syllable inventory** (from data: `phonemes.json`) → raw words:
  *Kazak*, *Urdim*, *Sslith*.
- A small **lexicon** mapping raw words to meanings: *kazak* = "hammer",
  *urdim* = "coin".
- Names are **composed of meanings**, so every name is translatable:
  - Person: **Kazak Urdim, "Hammercoin"** — instant character.
  - City: **Ostvel, "The Salt Gate"**.
  - Artifact: **Zonlirash, "Oathbreaker"**.
- Separate name **grammars** per thing-type: taverns ("The {adj} {animal}"
  → *The Flatulent Basilisk*), ships/spaceships (*ISV Regrettable
  Decision*), roads, plagues, wars ("The War of the Three Lies").
- Cultures share the engine but differ in data → goblin names sound goblin,
  corporate spacer names sound like LLC filings. The genre-mash lives here.

Output: `Lexicon` per culture + a `NameForge` service the historian and
runtime both call.

## 3. Phase C — History Simulation (the zero-player game)

Fast-forward **250–500 years** in 1-year ticks. Simulate only **notable
entities** (DF does the same — "historical figures"), keep populations as
plain numbers:

**Entities**
- **Civilizations/Factions** (5–10): culture, values (3–4 from a pool:
  honor, greed, knowledge, secrecy, spice tolerance…), home region,
  relationships matrix (war/trade/alliance/grudge, -100..100).
- **Figures** (~300–800 total over all history): name, culture, birth/death
  year, **personality traits rolled from a pool** (brave, greedy, kind,
  petty, cursed-with-honesty…), 1–2 relationships (parent, rival, sworn
  enemy, ex), a profession (hero, smith, cult leader, merchant prince,
  scooter bandit).
- **Sites**: owner faction, founded year, fate (thriving / sacked year N /
  abandoned / infested). **A sacked or abandoned site becomes a dungeon.**
- **Artifacts** (~30–80): forged by a figure, material, deeds attached,
  current resting place (a site, a beast's hoard, "lost").
- **Beasts** (~10–20 named monsters): lair region, kill list.

**Yearly tick — rule table** (all data-driven weights; every rule appends
Chronicle entries):
- Notable births/deaths; heirs and succession squabbles.
- **War check**: adjacent factions + opposed values + a greedy leader →
  war → battles → site sackings → refugees → new grudges.
- **Trade/alliance check**: shared rivers/roads → pacts → richer cities.
- **Ambition check**: a figure's traits fire personal events — forges an
  artifact, founds a cult, slays (or is eaten by) a beast, steals an
  artifact, writes a scathing book about a rival.
- **Disaster roll**: plague (gets a name), flood, "sky debris" —
  **a crashed spaceship seeds a site with credits/tech and a cargo
  manifest**, which is how sci-fi bleeds into the setting canonically.
- **Beast activity**: raids, hoard growth, occasional named-hero showdown.

**Scope guardrails (important):**
- No body simulation. Injuries are trait tags with comedic value
  ("lost an ear to the Basilisk of Ost", "banned from three taverns").
- Cap the Chronicle at ~3,000–8,000 entries; stop simming a thread when
  nothing references it. Target: full worldgen **< 2 seconds** even in the
  browser.
- Depth comes from *cross-references*, not entry count: every entry links
  actors → sites → artifacts → causes ("because of entry #412").

**Output: the Chronicle** — an append-only list of typed, linked facts:

```json
{ "id": 1287, "year": 213, "type": "artifact_forged",
  "actor": "fig_144", "site": "site_23", "artifact": "art_31",
  "cause": 1101,
  "tags": ["war_of_three_lies", "revenge"] }
```

Plus the final **World State** snapshot (who's alive, who owns what, who
hates whom in year 0 of play).

## 4. Phase D — Runtime: The Chronicle Is the Content

This is the payoff. Runtime event cards (PLAN.md §4.1) get **slots** filled
by querying the Chronicle with constraints:

```json
{
  "id": "dungeon_named_blade",
  "locations": ["dungeon"],
  "slots": {
    "sword":  { "query": "artifact", "where": "resting_place == current_site && type == weapon" },
    "victim": { "query": "figure",   "where": "killed_by == slot.sword.wielder" }
  },
  "text": "Wedged in a rib cage: {sword.name}, '{sword.meaning}'. The plaque says it ended {victim.name}. The rib cage suggests irony.",
  "choices": [ "..." ]
}
```

- **Loot with provenance**: a rolled item can *bind* to a historical
  artifact → its description is its history, and factions/descendants react
  to you carrying it in later events. (Layers on top of PLAN.md's quirk
  system — quirks for mundane items, provenance for artifacts.)
- **NPCs**: tavern strangers are living minor figures or descendants of
  dead ones — their traits pick their dialogue grammar, their grudges
  generate quest-ish hooks ("You met his rival's granddaughter. She has
  opinions and a crossbow.").
- **Books / notes / recordings / clues / treasure maps** = Chronicle
  excerpts rendered through a text grammar. A "book" is 3–6 linked entries
  paraphrased. A treasure map is a pointer at an artifact's resting place
  or a hidden site. **This makes your entire readables category free.**
- **Rumors**: tavern gossip = random recent-ish Chronicle entry, rendered
  sloppy and possibly *wrong* (roll: 15% the rumor mutates — fun when the
  player acts on bad intel).
- **Factions at runtime** are the surviving civs — their year-0
  relationship matrix seeds prices, hit squads, and war-zone travel events.
- If a slot query finds nothing, the event falls back to pure-random
  generation (PLAN.md systems) — history flavor is a bonus layer, never a
  blocker.

## 5. The Text Grammar (the "recipe book" for prose)

One Tracery-style expansion engine renders *everything* — event text, item
descriptions, book excerpts, rumors, epitaphs:

```json
{
  "artifact_forged.report": [
    "In {year}, {actor.name} forged {artifact.name} at {site.name}. {motive_clause}.",
    "{artifact.name} — {actor.name}'s masterwork, and everyone at {site.name} heard about it. Repeatedly."
  ],
  "motive_clause": [
    "They said it was for {actor.rival.name}. It was",
    "Officially, a gift. Unofficially, a threat",
    "Nobody asked why. That was the mistake"
  ]
}
```

- Templates carry **tone tags** (`tragic`, `funny`, `ironic`, `deadpan`) so
  the renderer can vary voice — same fact, different vibe on re-tell.
- Grammar files are just JSON in `assets/data/recipes/` → writing content =
  writing templates, no recompile, trivially moddable.
- Rough authoring targets: ~50 phoneme/lexicon entries per culture,
  ~20 templates per Chronicle event type, ~10 name grammars. Small files,
  combinatorial output — exactly the DF "math vs. memory" trade.

## 6. Architecture & Files

```
src/worldgen/
├── rng.*         # seed streams (PCG32)
├── regions.*     # Phase A: region graph + site placement
├── language.*    # Phase B: NameForge, lexicons
├── history.*     # Phase C: entities, yearly tick, rule table
├── chronicle.*   # append/link/query API (the database)
└── grammar.*     # Phase D/5: template expansion engine
assets/data/recipes/
├── phonemes/*.json  lexicons/*.json  namegrammars/*.json
├── history_rules.json      # event weights, value-conflict table
├── traits.json  values.json
└── prose/*.json            # text grammars per event type & tone
tools/
└── chronicle_dump          # CLI: gen world from seed, print full history
                            # as readable text — THE dev tool. If reading a
                            # raw dump is entertaining, the game will be.
```

The Chronicle query API stays dumb on purpose: linear scans with predicate
filters over a few thousand entries is microseconds. Index by entity id
only. No database, no cleverness.

## 7. Build Order (revised milestones, replaces PLAN.md §7 rows 3–4 area)

| # | Milestone | Proves |
|---|---|---|
| 0 | Skeleton (native + WASM) | platform story |
| 1 | Card engine w/ placeholder events | the loop is fun |
| 2 | **Grammar engine + NameForge** | generated prose reads well *before* history exists |
| 3 | **Region graph + sites** | a world to point at |
| 4 | **History sim + Chronicle + `chronicle_dump`** | the dump is fun to read on its own — gate here until it is |
| 5 | **Slot queries: wire cards to Chronicle** | history surfaces in play |
| 6 | Items: quirks + artifact provenance; inventory | the MUD hook |
| 7 | Travel/vendors/factions from world state | living economy |
| 8 | Dungeons = sacked-site event chains; books/rumors/maps | the payoff loop |
| 9 | Content blitz on templates & rules | replayability |
| 10 | Meta, polish, release | ship it |

**The single most important dev habit**: after every worldgen change, read a
`chronicle_dump` of a fresh seed. It's the whole game in prototype form —
if the log makes you laugh, you're on track; if it's a spreadsheet, fix the
grammar and rules before writing more sim.
