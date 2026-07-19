# The Thousand Scenario Plan

This is the production map for a deck of exactly 1,000 authored scenario ids.
It is not a promise that one premise will be copied into several biomes. Every
id must have its own opening, conflict, choice structure, outcomes, and reason
to exist in the Chronicle.

The current deck contains 521 cards. All of them have a primary theme and deck,
and 351 foundation cards have completed the Release 17 narrative remaster. New
releases fill this matrix until the validator reports 1,000 playable ids.
Scenario count is only one defense
against repetition. The engine also deals without replacement for an entire
life, cools repeated themes, and preserves seen ids in saves.

## Primary location budget

Every event receives one primary deck for quota accounting. It may list extra
locations when the premise genuinely travels.

| Primary deck | Target | Narrative work |
| --- | ---: | --- |
| city | 150 | housing, councils, clinics, courts, factories, finance |
| tavern | 130 | labor gossip, travelers, recruiters, scams, relationships |
| road | 120 | borders, migration, logistics, war, itinerant institutions |
| forest | 90 | commons, extraction, rangers, ecology, hidden communities |
| dungeon | 90 | buried ownership, prisons, ruins, monsters, stolen history |
| cave | 70 | mines, geology, shelter, deep labor, nonhuman life |
| swamp | 65 | poisoned water, memory, adaptation, wetland communities |
| mountains | 65 | mines, isolation, observatories, borders, clean power |
| coast | 70 | ports, fisheries, erosion, refugees, maritime trade |
| sea | 60 | crews, piracy, naval war, migration, free ships |
| crash | 35 | post-scarcity relics, automation, cosmic bureaucracy |
| dungeon finale | 35 | irreversible decisions and institutional reckonings |
| locationless consequence | 20 | delayed arc beats that find the player anywhere |
| **Total** | **1,000** | |

## Twenty political and human themes

Each theme owns 50 scenarios. Those 50 are split across ten venue bands and
five dramatic lenses. The full Cartesian product is the 1,000-scenario
backlog: 20 themes x 10 venue bands x 5 lenses.

1. Workplace democracy and ownership
2. Housing, landlords, eviction, and land
3. Anti-fascist organization and defense
4. War refusal, desertion, veterans, and civilian survival
5. Refuge, genocide prevention, testimony, and return
6. Pollution, climate, repair, and environmental justice
7. Public health, disability, care work, and grief
8. Food, hunger, kitchens, farms, and the commons
9. Colonial borders, stolen land, and cultural survival
10. Debt, private equity, insurance, and financial extraction
11. Automation, abundance, post-scarcity, and artificial scarcity
12. Procedure, liberal compromise, and institutional cowardice
13. Police, prisons, surveillance, and state violence
14. Union democracy, strikes, scabs, and internal corruption
15. Revolution, defeat, memory, restoration, and the next attempt
16. Piracy, naval labor, free movement, and maritime law
17. Science, exploration, education, and a better humanity
18. Faith, liberation theology, cult authority, and moral courage
19. Media, propaganda, rumor, archives, and manufactured consent
20. Kindness, friendship, mutual aid, and ordinary solidarity

## Ten venue bands

Each theme appears once in each band under each of the five lenses. The exact
deck is selected from the band according to the location budget above.

1. Civic: city
2. Social: tavern
3. Transit: road
4. Wild commons: forest or swamp
5. Buried systems: dungeon or cave
6. High country: mountains
7. Shore: coast
8. Open water: sea
9. Future ruin: crash
10. Reckoning: dungeon finale or locationless consequence

## Five dramatic lenses

1. Immediate need: someone needs food, shelter, medicine, escape, or safety.
2. Organized response: people try to change who owns or decides something.
3. Compromise: a partial rescue strengthens a harmful institution or delays a
   larger fight.
4. Backlash: power retaliates through law, violence, scarcity, propaganda, or
   division.
5. Legacy: an old defeat or kindness returns through a person, relic, rumor,
   institution, or place.

## Scenario contract

Every one of the 1,000 entries must satisfy all of these rules:

- Three or four choices use materially different strategies. Renamed versions
  of fight, talk, and leave do not count as different.
- At least one outcome changes a relationship, region, rumor, agenda, delayed
  consequence, artifact, or Chronicle entry. A card cannot exist only to move
  hit points and money.
- The text identifies who benefits and who bears the cost.
- Solidarity can succeed, fail, or survive as memory, but it is never a free
  morality button.
- Selfish or cautious choices can be tactically useful. Their downstream cost
  remains visible.
- Fascism and genocide are treated as structures of violence, not edgy decor
  or neutral debate topics.
- Jokes target power, bureaucracy, cosmic absurdity, pride, and material
  contradictions. Victims are not the punchline.
- No copied lines, characters, catchphrases, or scene structures from another
  work.
- No em dash.

## Arc architecture

Two hundred of the 1,000 scenarios form 50 four-beat arcs:

1. Contact introduces a person, contradiction, or institution.
2. Pressure reveals who profits and asks for a commitment.
3. Consequence returns after time and reflects the earlier choice.
4. Reckoning changes the region or Chronicle without pretending history ends.

Another 200 are two-beat echoes. The remaining 600 are standalone cards with
enough persistent effects to become ingredients for later stories.

No arc repeats an id. Recurring situations use a new beat with changed facts.

## Production waves

| Wave | Deck total after wave | Focus |
| --- | ---: | --- |
| Foundation | 381 | Existing world, relationship, mystery, naval, and political cards |
| Wave A | 501 | Labor, housing, care, anti-fascism, and repetition repair |
| Narrative remaster | 521 | Reframe every legacy card and establish twenty tone anchors |
| Wave B | 625 | War refusal, refuge, borders, testimony, and propaganda |
| Wave C | 750 | Ecology, food, land, pollution, and public infrastructure |
| Wave D | 875 | Debt, automation, post-scarcity, science, and cosmic bureaucracy |
| Wave E | 1,000 | Revolution, legacy, kindness, arc reckonings, and quota repair |

## Automated gates

- An exact event id may appear at most once per life.
- Save and continue preserve the complete seen-id set.
- Every primary deck meets its quota at 1,000.
- Every one of the 20 themes reaches 50 tagged scenarios.
- No seven-word opening is duplicated.
- Choice structures, effects, and approaches are checked for suspicious clones.
- Monte Carlo play must reach at least 95 percent of standalone cards and every
  arc opener across the test suite.
- Native and browser runs from the same seed remain byte-identical.

## Naming convention

New ids use `r16_<theme>_<venue>_<lens>_<number>`. For example:

`r16_housing_civic_backlash_01`

The id is production metadata, not player-facing prose. Arc beats append
`_contact`, `_pressure`, `_consequence`, or `_reckoning` and each remains a
separate id.
