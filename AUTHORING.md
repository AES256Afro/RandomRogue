# Scenario Authoring

Read [NARRATIVE.md](NARRATIVE.md) before adding story content. Random Rogue's
politics should emerge through material conditions, institutions, consequences,
and choices. A villain giving a speech is less interesting than a rent increase
that reveals who owns the street.

## Start a scenario

Build the `scaffold` target, then print a valid event skeleton:

```powershell
build/windows/scaffold.exe r15_example tavern labor
```

Arguments are event id, location deck, and story family. Redirect the output or
copy the object into an event file listed by `assets/data/events/manifest.json`.

Release 16 scenarios also declare `primary` and `theme`. `primary` is the one
deck charged against the 1,000-card location quota even when the event can
appear in several places. `theme` is one of the twenty keys in
`assets/data/scenario_targets.json`. The analyzer reports every remaining gap.

## Required pass

1. Give every choice a different political or personal strategy.
2. Name who gains, who pays, and what changes after the card.
3. Prefer regional state, rumors, agendas, relationships, and delayed effects
   over isolated rewards.
4. Let solidarity cost time, safety, comfort, or certainty. It should remain
   worthwhile without becoming a magic button.
5. Keep jokes original. Borrow comic structures, never recognizable lines.
6. Do not use an em dash.
7. A recurring arc beat must receive a new event id. Exact ids are once per
   life, including scheduled consequences and explicit `goto` chains.

Run these gates before committing:

```powershell
build/windows/validate.exe assets
build/windows/analyze.exe assets
build/windows/regression.exe assets
build/windows/playtest.exe assets 3000
```

The validator checks ids, decks, effects, conditions, slot queries, and delayed
targets. The analyzer reports thematic coverage and suspiciously similar event
structures. The regression tool checks deterministic mechanics. The playtest bot
checks reachability and survival pressure.
