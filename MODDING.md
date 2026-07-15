# Modding Random Rogue

Everything in Random Rogue is data. Every event, item, quirk, name syllable,
prose template, trait, and companion lives in JSON under `assets/data/`. Edit
a file, restart the game, and your content is in — validated by the same
tools that gate the official content.

## Quick start

1. Create `assets/data/events/my_events.json` (an array of events — schema
   below).
2. Add `"my_events.json"` to the `files` list in
   `assets/data/events/manifest.json`.
3. Run the validator: `build/windows/validate.exe assets` (or the wasm build
   under node). Fix anything it flags.
4. (Optional) Balance-check: `playtest 3000 assets` runs three thousand bot
   runs and reports if your event murders everyone.
5. Play. On desktop, edit the files next to the exe. For the web build,
   rebuild the bundle (assets are baked in at link time).

## Event schema

```json
{
  "id": "tavern_example",           // unique everywhere
  "locations": ["tavern", "city"],  // decks: city tavern dungeon cave forest
                                    // road crash dungeon_finale; [] = only
                                    // reachable via goto
  "weight": 10,                     // draw weight within its deck (default 10)
  "when": ["trait cursed"],         // optional gate: all must hold to deal
  "slots": { "npc": "figure_alive" }, // optional chronicle bindings, see below
  "text": "Card text. {npc} and {site} interpolate. Grammar keys too.",
  "choices": [                      // 1-4 choices
    {
      "text": "Do the thing",
      "requires": { "money": 5 },   // gates: money, credits, stat+gte,
                                    // item, trait, nottrait
      "outcomes": [                 // weighted pool (no dice check)
        { "weight": 70, "text": "It worked.", "effects": ["money -5", "hp +2"] },
        { "weight": 30, "when": ["trait famous"],   // conditional outcomes
          "text": "It worked BETTER because you're famous.",
          "effects": ["money +5"] }
      ]
    },
    {
      "text": "Try the risky way",
      "check": { "stat": "dex", "dc": 12 },  // d20 + mod + item bonuses vs DC
      "success": [ { "weight": 100, "text": "...", "effects": [] } ],
      "fail":    [ { "weight": 100, "text": "...", "effects": ["hp -3"] } ]
    }
  ]
}
```

Conditional outcomes (`when` on an outcome) OVERRIDE the default pool when
they match — special cases beat generic ones. Every pool must keep at least
one unconditional outcome.

## Effects

```
hp ±N          maxhp ±N        money ±N        credits ±N
stat <s> ±N    trait +id       trait -id       item <template_id>
loot common    loot fine       removeitem <id> rep ±N
shop           contract        goto <event_id> take_artifact
companion <id> companion_leave companion_dies  npc_mark <tag>
die <epitaph…> finish <epitaph…>   (finish = end the run ALIVE, in gold)
```

## Conditions (`when`, on events or outcomes)

```
trait X / !trait X      has <item> / !has <item>   carrying_artifact
money|credits|hp|day|rep  <op>  N     stat <s> <op> N
companion / !companion  vehicle / !vehicle
war_here  plague_here   raining  snowing   season <name>   npc <tag>
```

## Slots (chronicle bindings)

| query | provides |
|---|---|
| `chronicle_random` | `{name}` = a rendered history entry |
| `chronicle_news`   | `{name}` = a rendered LIVE entry (event skipped if no news) |
| `figure_alive`     | `{name}`, `{name}_trait`, `{name}_prof`, `{name}_faction` |
| `figure_dead`      | `{name}`, `{name}_trait`, `{name}_year` |
| `artifact_here`    | `{name}`, `{name}_material` — binds for `take_artifact` |
| `god`              | `{name}`, `{name}_domain`, `{name}_mood` |

If a slot can't be satisfied where the card is dealt, the event is skipped —
so slot events are automatically situational.

## Other moddable data

- `assets/data/items.json` — item templates (types: weapon, armor, food,
  drink, med, book, bag, misc, vehicle; `use` effects; `passive` bonuses)
- `assets/data/quirks.json` — quirk texts + hidden passive pools
- `assets/data/traits.json` — trait id → display name
- `assets/data/companions.json` — companion kinds
- `assets/data/recipes/prose.json` — every grammar template, including the
  chronicle renderers (`chron_*`), name patterns, god domains
- `assets/data/recipes/language.json` — per-culture syllables + the lexicon

Share your mod as a zip of changed files. Determinism note: content changes
worlds — two players need matching data for a shared seed to match.

## Loading mods in the browser (R7)

Host an events file anywhere with CORS (a GitHub gist's raw URL works) and
append `?mod=<url>` to the play address:

```
https://random-rogue.com/play/?mod=https://gist.githubusercontent.com/you/abc/raw/myevents.json
```

The file is fetched at load, parsed as a standard events array (same schema
as `assets/data/events/*.json`), and merged into the deck. The title screen
shows `mod: +N events` when it lands. Data only — mods are event JSON, never
code. Duplicate event ids shadow nothing; both copies enter the deck, so
namespace your ids (`mymod_goblin_tea_party`).

Validate your file before sharing: drop it in `assets/data/events/`, add it
to `manifest.json`, and run the `validate` tool.
