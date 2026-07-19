# Random Rogue 2.0 — "The Chronicle Absorbs You"

The massive update. Goal: complete randomness with a rich source to pull
from, nigh-endless play, and the core promise — **a player should never feel
like they know how to respond to X.**

## 0. The Design Law (read this before anything else)

Memorization dies when the same card can resolve differently for reasons the
player can sense but not fully compute. Three multipliers, in priority order:

1. **Context** — outcomes gated on hidden world state. The goblin toll
   collector waves you through free if his faction likes you, doubles the
   toll during a war, and flees if you carry the artifact that killed his
   grandfather. Same card, different world = different answer.
2. **Consequence** — choices leave marks (traits, reputation, NPC memory,
   chronicle entries) that alter *future* cards. The optimal answer to X
   depends on what you did about W.
3. **Volume** — 10x content so the surface area is too big to grind.

Volume without the first two just delays the spreadsheet. All three together
make the spreadsheet impossible.

---

## 1. Systems (the big swings)

### 1.1 The Living World ★ centerpiece
History does not stop when the player is born. Every in-game day, the
simulation ticks forward: wars break out mid-run, cities get sacked while
you're three regions away (you hear it as *news*, then see refugees on the
roads, then find the ruins), plagues spread region to region, artifacts get
stolen from dungeons you were saving for later, beasts migrate, prices move.
- Runtime chronicle entries are tagged `live` — taverns rumor about *this
  week*, not just ancient history.
- Decks re-weight by current events: war zones spawn deserters, checkpoints,
  and profiteers; plague regions spawn quarantines and quack doctors.
- Technical: the yearly tick already exists; add a daily micro-tick that
  advances a fractional year and emits localized consequences.

### 1.2 Legacy: your dead characters enter the Chronicle ★ centerpiece
The world persists across runs (per seed). When you die:
- Your character becomes a **historical figure** — epitaph, deeds, death
  entry, all of it written into the Chronicle.
- Your next character is born into the *same world, years later*. The world
  simulates the gap. You can hear tavern rumors about your own previous
  death. A bard sings the ballad. Badly.
- Items you carried scatter to sites as recoverable relics — your old sword,
  quirk intact, waiting in a dungeon with YOUR name in its provenance.
- Descendants: opt to be an heir of a previous character (stat echoes, a
  family grudge, one heirloom).
- "New world" always available; but the sales pitch is the 40-run-old world
  thick with your own ghosts.

### 1.3 Traits & afflictions (the player-side state machine)
The character accumulates **tags** — cursed, famous, wanted, glowing,
lycanthropy-adjacent, banned-from-taverns, blessed-by-a-forgotten-god,
owes-the-scooter-gang. Tags:
- gate choices (`requires: {trait: wanted}`), gate outcomes, spawn dedicated
  events (bounty hunters for `wanted`, autograph seekers for `famous`),
  and decay/cure through play.
- This is the #1 anti-memorization lever: the right answer to a guard
  shakedown is different when you're `famous` vs `wanted` vs `glowing`.

### 1.4 Conditional outcomes (engine upgrade that powers everything)
Outcomes get optional `when` clauses evaluated against world + character
state: `faction_at_war`, `rep < -20`, `trait(cursed)`, `carrying_artifact`,
`night`, `plague_here`, `year > 500`, `previous_character_died_here`.
Weighted pools filter by `when` first, then roll. Authoring cost: one line
per outcome; payoff: every event becomes situational.

### 1.5 Companions
Hirelings, strays, and volunteers: a mercenary with a trait, a talking
badger, a disgraced accountant, one (1) very loyal mimic. They add check
bonuses, interject in events (their trait picks the interjection), can be
targeted by outcomes (kidnapped, poisoned, unionized), and can betray or die
— entering the Chronicle when they do.

### 1.6 Contracts & ambitions (generated quests)
Multi-step goals generated from the Chronicle: retrieve artifact X resting
at Y for faction Z; escort a descendant to their grandparent's ruin; hunt
the beast that ate a named hero; deliver a sealed letter (DO NOT READ — a
trait if you do). Also **ambitions**: run-long win conditions chosen at
birth (die rich, slay a named beast, get a monument built about you, own a
spaceship) that give runs a shape without ending the sandbox.

### 1.7 Deep encounters (multi-round events)
Boss-tier cards that run 2–4 linked rounds via the existing `goto` machinery
with state (the duel where round 2 depends on round 1's stance; the
negotiation that escalates). Authored as small graphs; feels like combat
without building a combat system that fights the card game.

### 1.8 Religion & the gods
Generated pantheon per world (names, domains, moods, schisms in the
chronicle). Shrines, tithes, blessings-with-strings, the forgotten god arc
expanded into a full storyline. Gods remember. Gods take notes.

### 1.9 Vehicles & the travel layer
Vehicles become real: scooter/wagon/buggy/truck each add travel options,
their own event pools, breakdowns, upgrades, and passengers. Endgame: buy,
repair, and fuel a crashed **spaceship** across many steps — leaving the
planet is a legitimate run-ending achievement (the chronicle records the
world's reaction).

### 1.10 Exploration & hidden areas
Clue → rumor → map-fragment → hidden site chains, generated. Hidden sites
(glades, vaults, crash sites, sunken towns) hold the best relics and the
weirdest events. Spyglass/scooter/rope gate deeper access.

### 1.11 NPC memory
Named NPCs (vendors, legends, descendants) remember your last interaction
per world — the vendor you robbed prices accordingly forever; the legend you
beat at arm wrestling introduces you to rooms. Cheap to store, enormous felt
consequence.

### 1.12 Seasons, weather, night
A world clock: seasons re-weight biome decks, weather modifies checks
(rain: DEX outside), night swaps in nocturnal variants. Free variance on
every existing card.

---

## 2. The 10x Recipe

| Category | Now | Target | How |
|---|---|---|---|
| Events | 59 | **600+ effective** | ~250 new authored skeletons × parameterization (see 2.1) |
| Outcomes per choice | 1–3 | 4–8 (incl. conditional) | `when`-gated pools |
| Prose grammar keys | 49 | 200+ | new domains: dialogue, weather, gods, news, ballads |
| Variants per key | ~4 | 10–15 | straight authoring |
| Language cultures | 1 | 5 (old-tongue, goblin, spacer-corporate, swamp, liturgical) | per-culture phonemes + lexicons; factions speak differently |
| Items | 42 | 300+ | generated families: material × type × tier × maker-culture |
| Quirks | 60 | 300 (150 texts + 100 historical templates + 50 rare) | |
| History event types | 17 | 40+ | heresies, dynasties, inventions, scandals, disappearances, comets, prophecy (and prophecy *failing*) |
| History length | 250 yrs | **1,000+ yrs with eras** | see 2.2 |
| Epitaphs | 30 | 120 | the death screen must never repeat in a session |
| Site types | 7 | 15+ | monasteries, prisons, lighthouses, markets, battlefields, wizard towers, theme-taverns |

### 2.1 Event parameterization (how 250 skeletons become 600+)
Stop authoring "the goblin toll." Author "the {collector} toll" where the
collector is drawn from culture + faction + trait, the demanded currency
varies, the RULES rock is sometimes a scroll, a parrot, or a sibling, and
outcome pools branch on world state. Every skeleton ships with a variance
checklist: ≥2 slot axes, ≥1 conditional outcome, ≥1 trait interaction.
The validator enforces the checklist.

### 2.2 Deep history: 1,000 years in eras
- **Eras** with distinct flavor: Founding, the Long Peace, the Sundering
  (extra wars), the Sky-Fall Century (debris rate ×10 — this is where the
  sci-fi flood happened), the Present. Era tags color names, prose, and
  which ruins hold what tech.
- **Dynasties & institutions**: factions rise, split (schism events), merge,
  and *die* — a dead faction's crest on an item means something different.
- Chronicle scale: ~20–40k entries. Engine work: prune boring threads,
  index by entity for O(1) slot queries, and keep worldgen under ~4s in
  browser (budgeted, measured, CI-gated).
- Books become era histories; scholars cite (and misquote) each other.

### 2.3 Content pipeline (or the blitz collapses)
- Split `events.json` → `assets/data/events/<deck>/*.json` + build-time
  manifest (web preload needs a file list).
- `validate` grows: variance-checklist enforcement, deck-coverage stats,
  dead-slot detection, tone lint (flag events with zero funny outcomes).
- `chronicle_dump --era X --entity Y` filters for authoring against history.
- New `playtest` tool: headless Monte-Carlo runs (10k choices by policy
  bots) reporting death curves, economy inflation, deck exhaustion rates.
  Balance by data, not vibes.

---

## 3. Nigh-Endless Play

1. **Persistent worlds** (1.2) — the run ends; the world doesn't.
2. **Living world** (1.1) — the world changes faster than you can learn it.
3. **World size scaling**: regions 10–14 → 30–60; travel becomes regional
   (near sites cheap, far sites cost days/supplies/vehicle) so the map
   unfolds over many runs.
4. **Deck exhaustion resistance**: per-visit pools draw from
   skeleton × parameters, so even a repeated skeleton arrives re-cast.
5. **Save mid-run** (native file / localStorage) — long runs survive.
6. **Difficulty ratchet**: the world escalates with world-age, not player
   level — old worlds are dangerous *and* rich, and that's the appeal.
7. **Daily world**: everyone plays the same date-seed; your death epitaph is
   shareable against everyone else's (local stats now; leaderboard later).
8. **Ambitions** (1.6) give arcs; retirement is a victory (your character
   enters the Chronicle *alive* — innkeeper, legend, cult leader emeritus).

---

## 4. Build Order (phases; each ships playable)

| Phase | Contents | Why first |
|---|---|---|
| **P1 — The Engine of Doubt** | Conditional outcomes (`when`), traits/afflictions, NPC memory, event file split + manifest, validator v2 | Everything else hangs off these hooks |
| **P2+P3 — The Unbroken Chronicle** | Living World and Legacy merged: ONE timeline with two tick rates — daily while playing (wars, plagues, news, prices), yearly between runs. Dead PCs become figures inside the same sim, processed by the same rules: books written about them, their relics stolen and relocated, their killer beasts gaining fame, monuments raised. Persistent worlds, heirs, save/load. | One machine, two clocks; the retention engine |
| **P4 — 10x Content Blitz I** | 1,000-yr eras + new history types, 5 cultures, item/quirk generation families, +100 event skeletons | Feeds on P1 hooks |
| **P5 — Company & Purpose** | Companions, contracts, ambitions, deep encounters | Depth |
| **P6 — The Wide World** | 30–60 regions, regional travel, vehicles-for-real, exploration chains, religion | Breadth |
| **P7 — 10x Content Blitz II** | remaining skeletons to 250+, prose to 200 keys, epitaphs to 120, Monte-Carlo balance pass | Saturation |
| **P8 — Endgames & Ship** | Spaceship arc, retirement, daily world, mod folder support, itch/site update | The bow on top |

Rough shape: P1–P3 are mostly engine (the anti-memorization machinery);
P4–P7 are mostly authoring; content lands continuously after P1 since every
skeleton written to the new checklist works immediately.

## 5. Budget Reality Check
- 100 MB is unreachable for text — and that's good. 250 skeletons + 200
  grammar keys ≈ 2–4 MB. The wasm stays ~1 MB. Ship size stays trivial;
  the *possibility space* is what grows 10x–100x.
- The real budget is authoring hours. The parameterization + validator +
  Monte-Carlo pipeline is what makes 10x content affordable; build it first
  (P1), not last.
- Perf gates in CI: worldgen < 4s browser, chronicle queries < 1ms,
  1,000-yr dump still byte-identical native vs WASM.

---

## Release 3 (shipped)

Everything at once, per the brief:

- **Beast hunts** - named living beasts from the Chronicle become a
  bounty-board -> trail -> lair chain; `slay_beast` writes the kill into
  history (`chron_pc_beast_slain`) and the news line.
- **Afterlife interludes** - dying (not finishing) deals one gate card with
  a real god of this world; bargaining grants `legacy_bless`, so the next
  of your line starts blessed.
- **Faction careers** - completed contracts are counted; `contracts >= 2`
  earns the Guild Agent trait, `>= 4` unlocks the guildmaster chair (a
  `finish` ending) or the option to grift it.
- **Curse arcs** - `cursed` now escalates (`curse_manifests`) into a
  standing-stones rite that wants temple incense.
- **Mid-run save/load** - autosave every travel screen (localStorage /
  file); CONTINUE on the title restores character, packs, wars, weather,
  RNG state.
- **Persistent NPC marks** - marks are saved per world seed and outlive
  the run; the folk you rob remember your heirs.
- **World map screen** - MAP on the travel screen: ring-graph of all
  regions, biome colors, visited fog, wars/plagues/owner on hover.
- **Chronicle browser** - CHRONICLE on the title screen pages through the
  full thousand-year history of the next seed before you commit to it.
- **Procedural portraits** - name-hashed pixel faces on travel and death.
- **Daily leaderboard** - daily-world deaths POST to the Cloudflare worker
  (`/__score`, `/__scores`); top three fallen shown on the title screen.
- **Death share card** - COPY EPITAPH puts your name, days, epitaph, and
  seed on the clipboard.
- **Music stingers** - a dirge at the gate, a fanfare for slain beasts.

Deferred: naval arc, Steam packaging.

---

## Release 4 (shipped) - The Online World

Backed by Cloudflare D1 (database "random-rogue", table `deaths`), bound
to the same worker as the KV site. Endpoints: POST /__score,
GET /__scores?day=N, GET /__ghosts?day=N&n=3.

- **Shared graveyard** - daily-world deaths carry name, epitaph, death
  site, and relics to D1; other players' runs inject up to three
  strangers as fresh graves (`stranger_here` events), figures, and
  findable relics. No gap years simulated, so every player's daily
  world stays the same age.
- **Rival wanderer** - every world generates one more adventurer
  (deterministic per seed+generation). The daily tick advances them:
  they can slay YOUR bounty beast first, close contracts, win duels,
  and occasionally die on the road. Meet them in taverns
  (`rival_meeting`) or find their chalk taunts in dungeons.
- **Predecessor ghosts** - stand where a previous life of yours died
  (`ghost_here`) and they appear: cause of death, relic hints, or the
  rarest thing - hearing how the story continued.
- **Vendetta chains** - unsettled `robbed` marks (persisted per world)
  spawn kin who hunt you across runs; pay, talk, or take the swing.
  `npc_unmark` retires the grudge.
- **The old tongue** - profile-persisted lexicon (`learn` verb, books
  teach 2 words, study circles more). Artifacts carry script that
  becomes readable when you know enough words.
- **Death card PNG** - SAVE CARD on the death screen exports a 3x PNG
  (browser download / next to the exe). F11 screenshots any screen.

---

## Release 5 (shipped) - The Land, the Gods, the Watchers

- **Biome decks** - swamp, mountains, and coast each have their own
  event deck (8 events apiece); wandering draws from the land itself,
  with graceful fallback when a small deck runs dry.
- **The Book of You** - YOUR SAGA on the title: every stored life
  across every remembered world as chapters, with portraits, days,
  death sites, epitaphs, and profile totals (LoadAllLegacy).
- **Divine favor** - the `favor` verb banks devotion with the god an
  event summoned (shrines, temples, pilgrim roads in gods.json);
  5+ favor buys one miracle per life - the god spends it all to put
  you back up at 3 HP. Deep blasphemy converts to a curse.
- **Run replays** - every choice is journaled (day, site, choice,
  outcome; capped 240 steps); daily deaths upload the journey with
  the score (D1 `journey` column, /__replay endpoint); the title's
  "today's fallen" line is clickable and pages through the fall,
  card by card, with NEXT FALLEN to cycle victims.
- **Live deeds feed** - beast kills and completed lives POST to
  /__deed; daily runs pull /__deeds and surface real players' acts
  as "WORD ARRIVES" news lines mid-run (deeds table in D1).
- **PWA** - manifest, pixel icons, and a network-first service worker;
  random-rogue.com/play installs to the iPad home screen and plays
  offline after the first visit. Live endpoints are never cached.

---

## Release 6 (shipped) - The Naval Arc

The last deferred feature. Deferred no more.

- **Getting a boat** - three coast-deck paths while you own none
  (`when !has sloop`): buy her from the shipwright, win her off a
  drunk captain at cards, or raise a sunken one with rope and a
  strong back. The sloop is an item (type `ship`); the deed rides
  in your pack.
- **Set sail** - on any coast region with a ship, a fifth travel
  option: two days on the open water (deck `sea`), landfall on a
  DIFFERENT random coast. The fastest way across a 30-region world,
  paid for in whatever the sea deals you.
- **The sea deck** - squalls, freebooters with receipts, a ghost
  ship crewed by a real dead figure from this world's Chronicle,
  a leviathan you can surf, doldrums where a god does the books,
  merfolk vendors who overpay for dry biscuits, and bottle-post
  carrying genuine chronicle rumors.
- **The horizon ending** - the chart's last page has a line drawn
  off its edge. Follow it (rations required) through horizon_call
  to beyond_the_chart: cross entirely (a `finish` ending to rank
  with the spaceship), come back changed, or anchor in the seam
  and wake knowing six words of the old tongue.

Steam packaging remains the only deferred item.

---

## Release 7 (shipped) - The Welcome Mat and the Whetstone

- **New starts** - the class list is fixed-index now (Heir always #5,
  shown locked until a life ends in this world) plus three earned
  starts: Captain (own sloop; sail beyond the chart once), Anointed
  (2 favor + incense; be saved by a miracle), Guildchild (agent badge,
  stipend, one contract of credit; take the guildmaster's chair).
  The profile tracks beasts slain, miracles, horizons, chairs taken,
  and vendettas settled.
- **New ambitions** - Beastslayer, Polyglot, Mariner, Peacemaker,
  Devout join the original six; the roll now draws from eleven.
- **Onboarding** - three how-to cards before your first-ever life
  (replayable from OPTIONS). Settings: text speed, volume, music
  toggle - all persisted in the profile.
- **Difficulty ratchet** - each life a world takes from you sharpens
  it: damage +1 per 3 generations, payouts up to match, extra live
  history each day. Announced on the travel screen from gen 2.
- **Weekly world** - a second shared world with a 7-day lifespan for
  the long endings; its leaderboard/ghosts/deeds live in a separate
  key namespace (7000000+week) so daily and weekly never mix.
- **Real-player balance** - public /__stats aggregates (deaths, avg
  days, completions per board, deadliest sites); deeper journey-mining
  queries documented in scripts/balance-queries.md.
- **Web mods** - ?mod=<url> fetches a community events file and merges
  it into the deck ("mod: +N events" on the title). Data only, never
  code. Documented in MODDING.md.

---

## Release 9 (shipped) - Player-Feedback Round One

- **The repeat fix** - drawn-but-ineligible events (failed when-gates or
  slots) were silently marked used, draining location pools 2-3x faster
  than players saw cards and forcing early recycling. Draws now return
  to the pool unless actually shown; a per-deal tried-set stops retry
  spins. Repeats now only occur when a location's pool is truly seen out.
- **+122 events** - blitz2_town (32 tavern/city), blitz2_deep (30
  dungeon/cave/crash/finale), blitz2_wilds (30 forest/road/swamp),
  blitz2_heights (30 mountains/coast/sea). 289 events total across 25
  files; validator green; bot reaches 283/289.
- **Chronicle tap-to-read** - every line in the chronicle browser opens
  the full entry, word-wrapped; tap to dismiss.
- **Seed scramble** - the boot seed was raw time %% 1e9, so every visitor
  saw a near-identical 78xxxxxxx number; it now rolls all nine digits.
- **Touch-hover fix** - on iPad the last tap's position kept a choice row
  highlighted gold (read as a hint). The pointer parks offscreen after
  each tap, deferred one frame so fast taps still land.

---

## Release 10 (shipped) - Depth Over Breadth

The audit's design program, executed:

- **The context pass** - 100 of the strongest events deepened in place by
  four parallel editors: 183 new conditional outcome variants (6 -> 190
  game-wide) keyed to war, plague, weather, seasons, fame, wanted status,
  curses, wealth, poverty, companions, artifacts, and carried items; 25
  new requires-gated choices, including the game's first four-choice
  cards. The same card now reads differently in a siege, in the snow,
  or with your name on a poster.
- **Best-in-kind equipment** - only your best weapon and best armor
  count toward a check; armor no longer stacks; trinkets still add up.
  Satchels grant +2 pack capacity. Full packs swap the cheapest item
  for better loot (never silently discarding), narrated on the card;
  artifacts always make room.
- **Site states surfaced** - travel signposts show [war] and [plague];
  war-sacked cities were already becoming ruins that deal the dungeon
  deck, and now you can see trouble coming.
- **Chronicle navigation** - era jump buttons (I-V and NOW), and every
  entry's detail view offers FOLLOW THIS THREAD: the whole book filters
  to that figure's or faction's story; ALL returns.
- **Companion devotion** - ten shared days makes a companion devoted:
  their check bonus sharpens and the travel screen says so.
- **Provenance-coherent quirks** - quirk text and mechanics are one
  record now (24 paired entries): "repels dogs" reads as a charisma
  problem, not a strength bonus.
- **Itch builds are online** - all six endpoints use the absolute
  random-rogue.com base, so the shared graveyard, leaderboards,
  replays, and deeds work from any host.
- **Worker hardening** - per-IP hourly write budgets (D1 ratelimit
  table, deployed) and control-character sanitization on all
  player-supplied strings before storage or MUD forwarding.

---

## Release 13 - Every Problem Has Neighbors

- **Six connected stories** - thirty new cards form five-stage arcs about a
  crooked inheritance, plague refugees, contagious trade, a false saint, a
  guild schism, and a town that walks away from its taxes.
- **NPC social webs** - living figures have deterministic family, rivalry,
  employment, mentorship, debt, and confidant ties. Events can strengthen or
  damage those links, and known ties appear in the Story Ledger.
- **Regional chain reactions** - danger, unrest, plague, and prosperity cross
  region borders each week. Refugees, trade booms, and spillover pressure can
  emerge without the player standing in the affected region.
- **Investigations with risk** - generated mysteries now track evidence,
  doubt, suspects, accusations, trials, and verdict accuracy. A convincing
  accusation can still name the wrong person.
- **Director diagnostics** - F8 or Options opens a readable explanation of the
  current card's score, family cooldown, context bonuses, and underused tags.
- **Private balance tuning** - optional browser telemetry is aggregated at
  ingestion in Cloudflare D1. It stores no name, seed, free text, address, or
  individual play record, and can be switched off in Options.

---

## Release 14 - Endings Leak

- **Consequence convergence** - the six connected stories now leave persistent
  echoes. Refugees become apprentices and merchants, prosperity attracts
  thieves, false faith reorganizes believers, guild reform changes vendors,
  the walking town leaves mail and migrants, and inheritance disputes move
  furniture, money, and relics through the wider game.
- **Arc pacing** - the Director recognizes active families and reserves extra
  priority for a due story beat. Multiple arcs can run together. The Ledger
  shows vague progress without spoiling the next card. Threads ignored for
  twelve days resolve on their own and enter the Chronicle.
- **Investigation Board** - suspects now have inspectable motives, alibis, and
  deterministic testimony reliability. Players can mark either suspect, see
  case strength, and appeal a failed verdict. Wrongly accused people remember,
  build grudges, recruit allies, and return later.
- **People and Connections** - the Story Ledger opens a dedicated browser for
  known NPCs, opinions, debts, grudges, and social ties. Unknown connections
  remain hidden until the player learns enough about that person.
- **Balance Dashboard** - local run diagnostics and privacy-safe D1 aggregate
  counters expose repetition gaps, death association, location pressure, arc
  backlog, and story-echo reach without storing individual play records.
- **Reliability and access** - autosaves keep a last-known-good backup and
  recover it when the newest save is damaged. Reader text has a larger mode,
  buttons have a high-contrast mode, music remains muted by default, and long
  readers remain scrollable on touch screens.

---

## Release 15 - Power Has a Memory

- **Living political economy** - every region now tracks supply, rent,
  pollution, and solidarity alongside danger, prosperity, and unrest. Tenant
  unions, company towns, public clinics, clean power, commons, and mutual aid
  alter prices and future events.
- **NPC agendas** - named historical figures organize, expose, liberate,
  displace, pollute, militarize, provide care, or profiteer without waiting for
  the player. Their work changes regions and enters the Chronicle.
- **Rumor ecology** - reports travel between neighboring regions, change in
  reliability, warn about scheduled events, and can be verified, amplified, or
  buried from the Story Journal.
- **Political choice language** - thirty new authored scenarios center labor,
  housing, anti-fascism, anti-war resistance, refugees, public care, ecology,
  common ownership, post-scarcity freedom, and the cost of kindness.
- **Collective progression** - mutual-aid contracts improve local supply and
  solidarity. The Common Cause ambition rewards victories achieved together.
- **Better history browsing** - Chronicle topics isolate player lives,
  conflict, and civic change while retaining full-entry and entity-thread views.
- **Authoring discipline** - a narrative constitution, content guide, scenario
  scaffolder, semantic coverage report, validator, and simulation gates keep
  future additions politically coherent and mechanically connected.
- **Access and atmosphere** - controller navigation, political audio stingers,
  a four-card onboarding sequence, and a refreshed presentation site explain
  the deeper world without changing the muted-by-default music policy.

---

## Release 16 - The Deck Remembers

- **Hard no-repeat dealing** - location decks never recycle during a life.
  Scheduled consequences and explicit story chains now pass through the same
  seen-card gate as ordinary draws.
- **World-scale cooldown** - each seed remembers its last 240 answered cards
  across generations. Death no longer immediately reshuffles the same tavern,
  road, or dungeon material back to the front.
- **A tested invariant** - regression tests exhaust a location and prove it
  stays exhausted. The 3,000-life playtest fails if an exact id appears twice
  in one life.
- **Wave A content** - 120 new scenarios add six cards for each of twenty
  political and human themes. The playable deck grows from 381 to 501.
- **The thousand-scenario matrix** - `SCENARIO_1000.md` defines exact location,
  theme, venue, dramatic-lens, arc, and quality budgets for 1,000 authored ids.
  A machine-readable target file makes the analyzer print every remaining gap.
- **Presentation update** - the homepage describes the no-repeat guarantee,
  the larger deck, and the public content plan without claiming unfinished
  cards are already playable.

---

## Release 17 - Solidarity Absurdism

- **Legacy narrative remaster** - all 351 foundation cards now have a primary
  political theme, a material coda, named choice approaches, and persistent
  regional consequences where the old card previously ended at money or hit
  points.
- **Twenty tone anchors** - one complete new scenario for every production
  theme establishes the remastered voice through labor trials, tenant plants,
  anti-fascist coalitions, war refusal, survivor testimony, public care,
  ecological sabotage, free ships, a monster union, a consenting cryptid, and
  a demon whose first humane visitor brings soup.
- **Solidarity absurdism** - ceremonial bureaucracy, cosmic inconvenience,
  humane monsters, pirate democracy, interior political contradiction, and
  radical humanism join the narrative constitution. Jokes target power,
  vanity, bad rules, and failing tools. Victims are never the punchline.
- **Mechanics carry politics** - legacy choices now identify strategies such
  as solidarity, mercy, greed, force, law, inquiry, caution, and wit. Their
  outcomes can alter rent, supply, pollution, pressure, unrest, and regional
  solidarity instead of disappearing after the result screen.
- **Machine-enforced direction** - every one of 521 cards must declare a valid
  primary deck and theme, carry that theme as a semantic tag, use non-empty
  choice approaches, and pass the project's no-em-dash authoring rule.
