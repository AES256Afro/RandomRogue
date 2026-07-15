# Random Rogue — Design & Technical Plan

A lofi-pixel, Reigns-style roguelike. Every playthrough is a stream of randomly
generated events with 2–4 choices, resolved by stats, inventory, faction
standing, and dumb luck. Tone: Dungeon Crawler Carl / Rick & Morty — tragic,
funny, or ironically good outcomes are all on the table.

Targets: **Windows, Linux, macOS, Browser** — one codebase.

> **Companion doc:** [WORLDGEN.md](WORLDGEN.md) — the Dwarf Fortress–style
> seed → world → simulated-history "recipe" system. It generates a queryable
> **Chronicle** of names, people, places, and events that the event cards,
> items, NPCs, books, and rumors below all draw from. It supersedes the
> milestone table in §7.

---

## 1. Tech Stack (recommendation)

**C++17 + raylib + Emscripten**, built with CMake.

| Piece | Choice | Why |
|---|---|---|
| Language | C++17 | Your preference; portable, fast, no runtime needed |
| Rendering/Input/Audio | [raylib](https://www.raylib.com) | Tiny, C-style API, first-class pixel-art support, official Emscripten (WASM) support — same code runs native and in browser |
| Build | CMake + presets | One config for MSVC, gcc/clang, and `emcmake` (web) |
| Content data | JSON via [nlohmann/json](https://github.com/nlohmann/json) | All events/items/locations are data files, not code — add content without recompiling |
| Saves | Local file (native) / IndexedDB via Emscripten IDBFS (browser) | |
| Web hosting | itch.io or Cloudflare Pages | Drop the WASM build in, done |

**Alternatives considered** (in case you want to trade control for speed):
- **Godot 4** — easiest 4-platform export, but you'd write GDScript/C# and fight the engine less than enjoy it for a text-heavy game.
- **TypeScript/web-first** (wrap with Tauri/Electron for desktop) — fastest iteration, but desktop builds are heavier and it abandons C++.

C++ + raylib is the sweet spot here: this game is 90% text + a card frame +
a pixel font, so the "engine" is small and the real work is content.

**Keep dependencies to exactly two** (raylib, nlohmann/json). Every extra dep
is an Emscripten headache.

---

## 2. Core Game Loop

```
 ┌─────────────────────────────────────────────┐
 │  WORLD SCREEN: choose where to go           │
 │  (city, tavern, dungeon, forest, desert,    │
 │   swamp, plains, cave, house, hidden area)  │
 └──────────────────┬──────────────────────────┘
                    ▼
 ┌─────────────────────────────────────────────┐
 │  EVENT STREAM: location deals event cards   │
 │  · card = flavor text + 2–4 choices         │
 │  · choice gates: stats / items / faction /  │
 │    money / luck roll                        │
 │  · outcome: stat Δ, loot, HP, faction rep,  │
 │    follow-up event, or death                │
 └──────────────────┬──────────────────────────┘
                    ▼
        travel again, or die → run summary
        → (meta unlocks) → new character
```

- **No maps, no navigation.** "Travel" is just picking which event deck deals
  next. A dungeon is a chain of 5–15 events with escalating stakes and a
  finale. Loot is automatic when an outcome grants it.
- **Death is the roguelike part.** Runs are short (15–45 min), death is
  frequent and usually hilarious, and a run summary screen ("Killed by: a
  vending machine you shook too hard") makes losing shareable.
- **Seeded RNG** from day one: lets you replay/share seeds and do daily runs
  later.

### Reigns-likeness, adapted
Reigns is 2 choices (swipe left/right) with 4 resource meters. Random Rogue
generalizes to **2–4 choices** (keys 1–4 / click), and the "meters" are the
stat block + HP + money + faction bars. Suggestion: keep Reigns' best trick —
**before you pick, show *which* stats/resources a choice will touch, but not
how much or in which direction.** That tension is the whole genre.

---

## 3. Character

- **Stats:** STR, DEX, CON, INT, WIS, CHA — rolled 3–18 at character gen,
  modified by events/items. Plus **HP** (derived from CON) and a hidden
  **LUCK** stat that quietly bends rolls (and that events can curse/bless).
- **Resources:** Money (coins) and **Credits** (a second, sci-fi currency —
  some vendors only take one; conversion scams are an event opportunity).
- **Checks:** `d20 + stat modifier vs. difficulty`, luck shifts the roll.
  Show the dice roll on screen — visible rolls make unfair deaths funny
  instead of frustrating.

---

## 4. Content Systems (this is the real game)

### 4.1 Events — the atom of everything
Everything is an event: a tavern brawl, a vendor, a dungeon room, a talking
mushroom. One JSON schema:

```json
{
  "id": "tavern_mysterious_stranger",
  "locations": ["tavern"],
  "weight": 10,
  "requires": "money >= 5",
  "text": "A hooded figure slides you a drink. It is fizzing. Drinks here don't fizz.",
  "choices": [
    { "text": "Drink it", 
      "outcomes": [
        { "weight": 60, "text": "It's just soda. From space. +1 WIS, weirdly.", "effects": ["wis +1"] },
        { "weight": 30, "text": "Poison. Classic.", "effects": ["hp -6"] },
        { "weight": 10, "text": "You can now taste colors. Permanently.", "effects": ["trait synesthete"] }
      ]},
    { "text": "[CHA 12] Make them drink it first", "requires": "cha >= 12",
      "outcomes": [ { "weight": 100, "text": "They vanish, leaving a key.", "effects": ["item key_rusty", "faction thieves -5"] } ]},
    { "text": "Leave immediately", 
      "outcomes": [ { "weight": 100, "text": "Coward. Effective, living coward.", "effects": [] } ]}
  ]
}
```

- `requires` is a tiny condition language: `str >= 12`, `has(rope)`,
  `faction(smiths) > 20`, `money >= 50`, `trait(cursed)` — implement as a
  ~200-line recursive-descent parser, or even simpler: structured JSON
  conditions. Start structured, add the string parser if authoring gets tedious.
- `effects` is a flat verb list: `hp -6`, `item <id>`, `money +25`,
  `faction <id> ±n`, `goto <event_id>` (chains!), `trait <id>`, `die <epitaph>`.
- **Follow-up chains (`goto`) are how dungeons work**: a dungeon "generator"
  picks N room-events from themed pools (entrance → rooms → complication →
  boss/treasure finale) and links them.

### 4.2 Items — MUD-style weirdness
Item = **base template + rolled quirk descriptions**, where a quirk *may or
may not* mean anything mechanically:

- Base: weapons, armor, jewelry, food, drink, medications, books, magazines,
  notes, recordings, clues, treasure maps, bags, campfires, sewing kits,
  armor repair kits, keys.
- Quirk pool: `"smells faintly of ozone"`, `"has someone's name scratched
  out"`, `"is slightly warm"`, `"hums when you're lying"`. Each quirk has a
  ~40% chance of carrying a hidden modifier (good or bad); the player only
  finds out through use or an INT/WIS check ("appraise" events). Identical
  quirk text can be a blessing on one item and a curse on another — per-item
  roll, so players can never fully trust the flavor text.
- Vehicles as items/unlocks: scooters, wagons, buggies, cars, trucks,
  spaceships — they gate travel options and open their own event pools
  ("Your spaceship has a check-engine rune").

### 4.3 Inventory
- Grid/list UI, carry capacity from STR + bags. Click item → use/inspect/drop.
- Using items *inside events* is a choice modifier: some choices only appear
  if you hold the right item (`[Use: Treasure Map]`).
- Food/drink/meds as consumables; campfire = rest event; sewing/repair kits
  fix armor degradation.

### 4.4 World & Factions
- **Locations** (each = an event deck + a vendor pool + ambient flavor):
  cities, taverns, houses, forests, deserts, plains, swamps, caves, dungeons,
  hidden areas (revealed by clues/maps/rumors).
- **Factions** (suggest 4–6 to start: e.g. Merchants Guild, Thieves, Smiths,
  a Cult, City Watch, The Scooter Gang). Rep from -100..100 changes prices,
  unlocks events, and occasionally sends hit squads. Faction hits are a great
  humor vector.
- **NPCs/vendors** are events too: smiths (weapons/repair), tailors (armor/
  bags), general vendors, shady tavern dealers. Buy/sell = generated stock
  from location + faction rep pricing.
- **Ecology:** plants (forage checks), animals (mounts/food/mistakes),
  monsters (fight/flee/befriend — always offer the dumb third option).

---

## 5. Presentation (lofi pixel, no drawn scenes)

- Virtual resolution **320×180** (or 480×270), integer-scaled. Instant lofi.
- **Text does the imagery.** UI = card frame, pixel font (e.g. a CC0 bitmap
  font), stat bars, coin/credit counters, 1–4 choice buttons.
- Cheap juice that sells it: palette per biome (swamp = sickly greens),
  screen shake on damage, dice-roll animation, typewriter text, CRT scanline
  toggle.
- Optional later: tiny procedural item icons (recolored 16×16 sprites) — but
  ship without them.
- Audio: chiptune loop per biome + a handful of SFX (rlAudio handles both
  native and web).

---

## 6. Project Structure

```
RandomRogue/
├── CMakeLists.txt, CMakePresets.json
├── src/
│   ├── main.cpp            # raylib loop, screen stack
│   ├── game_state.*        # character, inventory, world, RNG (seeded)
│   ├── events.*            # loader, condition eval, effect executor, deck draw
│   ├── items.*             # templates + quirk generator
│   ├── dungeon.*           # event-chain generator
│   ├── worldgen/           # region graph, NameForge, history sim,
│   │                       # Chronicle, grammar engine — see WORLDGEN.md §6
│   ├── ui/                 # card view, inventory view, run summary
│   └── platform/           # save paths, IDBFS glue for web
├── assets/
│   ├── data/               # events/*.json, items/*.json, quirks.json,
│   │                       # locations.json, factions.json, names.json
│   ├── data/recipes/       # phonemes, lexicons, name grammars, history
│   │                       # rules, prose templates — see WORLDGEN.md §6
│   ├── fonts/  └── audio/
├── tools/
│   └── validate.cpp        # content linter: dangling ids, unreachable events,
│                           # bad conditions — run in CI, saves your sanity
└── web/shell.html          # Emscripten shell for itch.io
```

---

## 7. Milestones

> Superseded by the revised table in [WORLDGEN.md §7](WORLDGEN.md), which
> inserts the worldgen/Chronicle milestones. Summary of the revised order:

| # | Milestone | Proves |
|---|---|---|
| 0 | **Skeleton** — raylib window, pixel font, CMake builds native **and** WASM | The whole platform story, before writing gameplay |
| 1 | **Card engine** — load events from JSON, render card, pick choice, apply effects, stats/HP on screen, death → restart | The Reigns loop is fun with 20 placeholder events |
| 2 | **Grammar engine + NameForge** — Tracery-style text expansion, constructed-language names | Generated prose reads well before history exists |
| 3 | **Region graph + sites** — biomes, adjacency rules, site placement, hidden areas | A world to point at |
| 4 | **History sim + Chronicle + `chronicle_dump`** — factions, figures, wars, artifacts, beasts | The dump is fun to read on its own — gate here until it is |
| 5 | **Slot queries** — event cards pull figures/artifacts/sites from the Chronicle | History surfaces in play |
| 6 | **Items & inventory** — quirk generator + artifact provenance, inventory UI, item-gated choices | The MUD-weirdness hook works |
| 7 | **World layer** — travel screen, per-location decks, vendors, factions from world state, money+credits | The "go somewhere, stuff happens" loop |
| 8 | **Dungeons** — sacked/abandoned sites become event-chain dungeons; books, rumors, treasure maps from Chronicle | The centerpiece content type |
| 9 | **Content blitz** — event templates, prose grammars, quirks, history rules; validator + authoring pipeline | Replayability (THE long pole — budget half the project here) |
| 10 | **Meta, polish & release** — seed sharing/daily run, unlockable starts, audio, juice, saves, itch.io + CI builds | Shippable |

---

## 8. Suggestions & Opinions

1. **Content is the game.** The engine for this is 2–4 weeks of work; 300
   good events is months. Build the JSON pipeline and validator *early* and
   make authoring frictionless — that decides whether this ships.
2. **Genre-mash the setting on purpose.** You listed swords AND spaceships —
   lean in: a fantasy world where sci-fi junk washes in and nobody finds it
   strange. It justifies credits vs. coins, medications next to potions, and
   it's a humor engine by itself.
3. **Every event: 3 outcomes minimum in the pool** — one bad, one funny, one
   ironically good. Write to that template and the tone takes care of itself.
4. **Show the dice.** Visible rolls + absurd epitaphs turn RNG deaths into
   the reason people post screenshots.
5. **Hidden info is the replay hook**: quirks that might be lies, LUCK you
   can't see, choices that show *which* stat they touch but not how.
6. **Start the WASM build in Milestone 0**, not at the end. Emscripten
   problems found late are miserable; found early they're trivial.
7. **Later, not now:** procedural icons, meta-progression tree, mod support
   (the JSON-everything design gives you modding nearly for free anyway).

---

## 9. First Session (when you're ready to build)

1. `git init`, CMake + raylib skeleton, window with "RANDOM ROGUE" in a pixel font.
2. Verify `emcmake` web build runs in a browser.
3. Define the event JSON schema, hardcode-load 5 test events, render the card loop.
4. Die horribly to a fizzing drink. Celebrate.
