# Random Rogue and MUD World Bridge Plan

Status: Living handoff document

Single-player baseline: Release 19, commit `cbd3890`

Last updated: 2026-07-19
Owners: Random Rogue project and Random Rogue MUD project

## Implementation status

- Phase M0: complete. Runtime, persistence, network, moderation, deployment,
  backup, and restore boundaries are audited in the MUD repository. The legacy
  death crossing remains covered by compatibility tests.
- Phase M1: complete. `rr.chronicle.v1`, stable identifiers, Cloudflare D1
  canonical event storage, MUD imported-event storage, atomic death projection,
  duplicate protection, source links, HTTP inspection, and admin inspection are
  implemented and acceptance-tested.
- Phase M2: complete. Canonical and compatibility death ingestion now share one
  application service. Typed, rebuildable projectors exist for ghosts, rumors,
  institutions, regions, and artifact echoes, with player and admin views.
- Phase M3: next. Add signed Worker delivery, the transactional outbox, bounded
  retries, dead letters, and broader one-way event delivery.

## 1. Purpose

This is the evolving build plan for connecting Random Rogue's single-player
Wide World to the shared Random Rogue MUD.

The two projects should feel like different views of one setting:

- Single-player creates personal history through procedural choices.
- The MUD turns selected history into shared places, people, rumors, ghosts,
  institutions, artifacts, and public consequences.
- The Chronicle is the stable contract between them.

This document must be updated with every major Random Rogue release. A release
is not considered fully documented until its new world state, Chronicle events,
and possible MUD projections have been recorded here.

## 2. Verified connection today

The current live bridge is intentionally narrow.

### Live now

1. Daily and weekly single-player runs submit deaths, epitaphs, relics, final
   journeys, and notable deeds to the Cloudflare service.
2. Cloudflare D1 stores shared deaths and deeds.
3. Other single-player runs can encounter shared graves, ghosts, relics, deeds,
   leaderboards, and replayable final journeys.
4. Unfinished single-player deaths are forwarded to
   `https://mud.random-rogue.com/api/legacy/death` as MUD ghosts.
5. The MUD can present those ghosts and list them on its Wall of the Fallen.

### Not live yet

- General single-player deeds do not yet become MUD rumors or room events.
- Single-player institution changes do not yet change MUD factions.
- Artifacts do not yet move safely between games.
- MUD actions do not yet return consequences to the single-player Chronicle.
- The stable identifier contract exists, but broad signed delivery is not live.

The website must continue to label these as planned until acceptance tests pass.

## 3. Shared design rules

1. One Chronicle, two projections. Neither game directly owns the other's
   runtime state.
2. History is append-only. Corrections create later entries instead of silently
   rewriting earlier events.
3. Every shared event is idempotent. Receiving the same event twice must not
   duplicate a ghost, item, rumor, or world change.
4. The source game remains authoritative for what happened there.
5. The receiving game decides how an event becomes playable in its own rules.
6. Personal data is not required. Player display names and free text remain
   optional, bounded, sanitized, and removable.
7. Shared consequences must be legible. Players should be able to inspect why a
   rumor, ghost, shortage, monument, or institutional change exists.
8. Political consequences remain material. The bridge should transfer changes
   in labor, rent, pollution, militarization, food, solidarity, legitimacy,
   mutual aid, and ownership rather than a hidden good or evil score.
9. The bridge must fail safely. A MUD outage cannot block or corrupt a
   single-player run.
10. No event is trusted merely because it came from a client.

## 4. Target architecture

```text
Single-player client
        |
        | bounded Chronicle event
        v
Cloudflare Worker API
        |
        +--> D1 canonical event log
        |
        +--> transactional outbox
                  |
                  | signed delivery with retries
                  v
             MUD ingest API
                  |
                  +--> idempotency ledger
                  +--> Chronicle projector
                  +--> MUD rooms, rumors, ghosts, factions, artifacts

MUD Chronicle events follow the same path in reverse after Phase 5.
```

The Cloudflare event log is the exchange layer. It is not a second game server
and should not simulate either game.

## 5. Canonical event envelope

All new bridge messages should use a versioned envelope.

```json
{
  "schema": "rr.chronicle.v1",
  "event_id": "01J...",
  "event_type": "wide_world.deed",
  "source": "wide_world",
  "world_key": "daily:20653",
  "occurred_at": "2026-07-19T18:30:00Z",
  "subject": {
    "kind": "character",
    "id": "wide:run-hash:character",
    "name": "bounded display name"
  },
  "place": {
    "region_id": "wide:region:17",
    "site_id": "wide:site:44"
  },
  "tags": ["labor", "solidarity", "city"],
  "effects": {
    "worker_power": 1,
    "solidarity": 2,
    "rent_burden": -1
  },
  "payload": {},
  "visibility": "public"
}
```

Required fields are `schema`, `event_id`, `event_type`, `source`, `world_key`,
`occurred_at`, and `visibility`. The Worker creates or verifies `event_id`.
Client-provided identifiers must not be accepted as proof of authenticity.

## 6. Initial shared event types

| Event type | Source | First MUD projection |
| --- | --- | --- |
| `wide_world.death` | Single-player | Ghost, Wall entry, epitaph rumor |
| `wide_world.deed` | Single-player | Tavern rumor or town crier report |
| `wide_world.ending` | Single-player | Era rumor, memorial, faction reaction |
| `wide_world.institution_changed` | Single-player | Faction mood, organizer NPC, meeting event |
| `wide_world.artifact_legacy` | Single-player | Read-only artifact echo before transferable items |
| `wide_world.region_changed` | Single-player | Room text, prices, hazards, public notices |
| `mud.death` | MUD | Shared Chronicle entry and later grave encounter |
| `mud.deed` | MUD | Single-player news or rumor |
| `mud.institution_changed` | MUD | Single-player institution pressure |
| `mud.artifact_changed` | MUD | Chronicle provenance update |

## 7. MUD restructuring target

The MUD should be reorganized around clear layers before broad two-way sync.

### Layer A: Domain

- Chronicle event types and stable world identifiers
- Characters, ghosts, regions, sites, institutions, factions, and artifacts
- Material conditions and artifact provenance

This layer contains no browser, socket, database, or command parsing code.

### Layer B: Application services

- Ingest Chronicle event
- Project death into ghost
- Project deed into rumor
- Project institution change into faction state
- Project regional change into rooms and vendors
- Export eligible MUD event
- Rebuild projections from event history

### Layer C: Adapters

- Cloudflare HTTP client
- MUD command handlers
- Persistence adapter
- Scheduler and retry worker
- Web client and telnet presentation

### Layer D: Presentation

- Room descriptions and Chronicle browser
- Wall of the Fallen and ghost examination
- Rumor boards and institution status
- Artifact provenance and admin diagnostics

The MUD must be able to rebuild imported projections from its idempotency ledger
without replaying ordinary player commands.

## 8. Build phases

### Phase M0: Audit and safety boundary [COMPLETE]

Goal: Understand the MUD before restructuring it.

- Inventory runtime, persistence, commands, rooms, NPCs, factions, items, web
  client, telnet gateway, deployment, tests, and backups.
- Record the current `/api/legacy/death` request and response behavior.
- Add a fixture test for the existing death crossing before moving code.
- Establish database backup and restore instructions.
- Identify all player-authored text and moderation boundaries.

Acceptance:

- Existing ghost crossing has a reproducible integration test.
- A clean MUD restore can be performed from backup.
- No restructuring begins before the current behavior is captured.

### Phase M1: Canonical identities and Chronicle core [COMPLETE]

Goal: Give shared history stable names.

- Add the `rr.chronicle.v1` envelope.
- Add stable prefixes for every shared world entity.
- Add MUD tables for imported events and projection versions.
- Reject duplicate `event_id` values without error.
- Add an admin command to inspect the raw event behind a projection.

Acceptance:

- Replaying the same fixture 100 times creates one projection.
- Every imported ghost can identify its source event.

### Phase M2: MUD domain restructure [COMPLETE]

Goal: Separate game rules from transport and presentation.

- Move ghost creation out of the HTTP route into an application service.
- Introduce projectors for ghosts, rumors, institutions, regions, and artifacts.
- Put command output behind presentation functions.
- Add unit tests for projectors without starting the network server.
- Preserve all current commands through compatibility adapters.

Acceptance:

- Existing rooms, accounts, inventory, and commands continue working.
- The legacy death endpoint calls the same tested projector as the new bridge.

### Phase M3: Reliable one-way bridge

Goal: Bring more Wide World history into the MUD.

- Add D1 tables for canonical events, outbox delivery, delivery attempts, and
  dead letters.
- Sign Worker-to-MUD messages with timestamped HMAC authentication.
- Retry transient failures with bounded exponential backoff.
- Deliver deeds, endings, institution changes, and regional changes.
- Add rate limits and payload size limits at both ends.
- Add a MUD admin view for pending, delivered, rejected, and dead-letter events.

Acceptance:

- A temporary MUD outage loses no accepted event.
- Invalid signatures and stale timestamps are rejected.
- The single-player game remains playable during an outage.

### Phase M4: Chronicle projections become gameplay

Goal: Make shared history useful rather than decorative.

- Deeds become regional rumors with source attribution.
- Institution changes alter faction dialogue, meetings, and local pressure.
- Pollution, rent, war, food, and solidarity affect selected rooms, vendors,
  hazards, and public notices.
- Endings create memorials, dissenting interpretations, or organizing tasks.
- Artifact legacies appear first as echoes, replicas, or provenance records.

Acceptance:

- Every projection has an inspectable cause.
- Imported effects are capped and decay where appropriate.
- One single-player run cannot dominate the shared MUD.

### Phase M5: Two-way Chronicle

Goal: Let the MUD write history back.

- Add MUD outbox events for deaths, deeds, institutions, regions, and artifacts.
- Add Worker ingestion with MUD authentication.
- Expose a bounded shared-news endpoint to single-player.
- Turn MUD events into news, graves, rumors, Chronicle entries, and delayed
  scenario conditions.
- Keep external events out of deterministic seeded world generation. Inject
  them only after the deterministic base world exists.

Acceptance:

- The same seed still creates the same base world on native and browser builds.
- Shared history is additive and clearly marked as player-caused.
- Offline single-player remains fully functional.

### Phase M6: Shared artifacts and institutions

Goal: Connect the rarest high-value state safely.

- Define artifact identity, custody, provenance, echo, transfer, duplication,
  loss, and recovery rules.
- Begin with read-only echoes. Do not permit cross-game custody until replay,
  rollback, duplication, and moderation tests pass.
- Map Wide World institutions to MUD factions through tendencies and material
  conditions, not one-to-one command authority.
- Add seasonal convergence events where both games affect a capped shared goal.

Acceptance:

- Artifact duplication and rollback tests pass.
- Shared institutions cannot be captured permanently by one player or one game.

### Phase M7: Operations, privacy, and moderation

Goal: Make the bridge supportable.

- Add retention rules for raw events, delivery logs, and player-visible text.
- Add deletion and tombstone handling for removable player display data.
- Add structured metrics without storing IP addresses in Chronicle records.
- Add circuit breakers, alerting, dead-letter replay, and version rollback.
- Document schema migration and backward compatibility policy.

Acceptance:

- Operators can disable any event type without redeploying both games.
- A bad projector version can be rolled back and rebuilt.
- Player-facing text has reporting and moderation paths.

## 9. Cloudflare data work

Proposed new tables:

- `chronicle_events`
- `chronicle_outbox`
- `chronicle_delivery_attempts`
- `chronicle_dead_letters`
- `chronicle_consumers`
- `chronicle_tombstones`

Recommended event status flow:

```text
accepted -> stored -> queued -> delivered
                         |
                         +-> retrying -> dead_letter
```

D1 remains the durable exchange record. If event volume later exceeds D1's
operational fit, delivery can move to Cloudflare Queues while D1 retains event
identity and audit state.

## 10. Security contract

- MUD delivery uses a dedicated secret, never the site deployment secret.
- Sign timestamp, event ID, and raw body.
- Reject timestamps outside a short replay window.
- Store processed event IDs before projecting effects.
- Limit accepted event types per authenticated source.
- Bound arrays, strings, nested objects, and total body size.
- Sanitize presentation text independently in each game.
- Never trust client-submitted effect magnitudes.
- Compute public effects on the Worker from allowlisted event facts.
- Provide kill switches by source and event type.

## 11. Release synchronization checklist

Every major Random Rogue release must update this document with:

1. New Chronicle event candidates.
2. Changed identifiers or data shapes.
3. New material conditions.
4. New institution types or tendencies.
5. New artifact behavior.
6. New endings or epilogues.
7. New player-authored text.
8. MUD projection opportunities.
9. Compatibility risks.
10. Required MUD acceptance tests.

Use this delta template:

```text
Release:
Single-player commit:
Schema impact:
New event types:
Changed event types:
New projections:
Security impact:
Migration required:
MUD tests required:
Website claims now allowed:
```

## 12. Release 19 bridge delta

Release: 19, The World Answers Back

Single-player commit: `e788474`

New source systems worth projecting:

- Remembered campaign choices and witnesses
- Public world responses
- Institution membership, standing, leadership, tension, and tendency
- Material condition changes
- Seven ideological ending horizons
- Movement epilogues and cross-life causes

Recommended first projections:

1. Ending epilogue becomes a MUD broadsheet or memorial argument.
2. A public world response becomes a regional rumor.
3. A major institution change creates a meeting, organizer, dissident, or
   bureaucratic obstruction.
4. A cross-life cause gives a ghost one additional inspectable statement.

Compatibility note:

The existing `/api/legacy/death` payload does not include event ID, world key,
movement legacy, causes, institution membership, or relic provenance. M2 now
adapts it internally to `rr.chronicle.v1` while preserving its public shape.

## 13. Hard stopping point and handoff package

This plan is ready to hand to the MUD project when the following package exists:

- This document updated through the current Random Rogue release
- Example `rr.chronicle.v1` fixtures for every enabled event type
- JSON Schema or equivalent validation code
- Cloudflare migration files and Worker route documentation
- Authentication setup guide with no committed secrets
- MUD projector acceptance tests
- Backfill and rollback procedure
- Live-feature matrix separating shipped, testing, and planned behavior
- One end-to-end test proving a single-player event appears in the MUD exactly
  once and remains inspectable

The practical M2 handoff is complete. The first broad public feature handoff
target remains the end of Phase M3.

## 14. Change log

### 2026-07-19

- Created the living bridge plan against Random Rogue Release 19.
- Recorded the verified death-to-ghost connection.
- Separated live shared deeds from planned MUD deed projection.
- Defined the event envelope, MUD restructure, build phases, release
  synchronization checklist, and hard stopping point.
- Completed M0 with a system inventory, legacy fixture boundary, guarded restore
  procedure, and full MUD regression run.
- Completed M1 with the canonical JSON Schema and fixture, stable event and
  entity IDs, D1 source-event storage, MUD idempotency ledger, atomic ghost
  projector, public source inspection, and admin inspection.
- Verified 100 deliveries of one fixture produce one source event and one ghost.
- Completed M2 with a shared ingestion service, typed projector registry,
  rebuildable rumor, institution, region, and artifact views, player Chronicle
  digest, admin rebuild tooling, and six cross-repository fixtures.
- Added stale-state protection so delayed older events remain auditable without
  replacing newer institution, region, or artifact projections.
- Kept non-death HTTP delivery closed until the signed Phase M3 transport.
