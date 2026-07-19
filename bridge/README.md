# Chronicle Bridge Contract

`rr.chronicle.v1` is the append-only exchange format between Random Rogue's
Wide World, Cloudflare, and Random Rogue MUD.

## Stable identifier rules

- Events: `wide:evt:<world-kind>:<world-number>:<type>:<record-id>`
- Wide World characters: `wide:character:<world-kind>:<world-number>:<record-id>`
- Wide World regions: `wide:region:<stable-number>`
- Wide World sites: `wide:site:<stable-number>`
- MUD entities use the same shapes with the `mud:` prefix.
- Identifiers are opaque after creation. Names and descriptions may change;
  identifiers must not.

The Cloudflare service assigns IDs to client-originated records. A browser
cannot make an event authoritative by choosing an ID.

## Live routes

- `GET /__chronicle?id=<event-id>` reads a public canonical source event from
  Cloudflare D1.
- `POST /api/chronicle/v1/events` lets the Cloudflare bridge deliver a bounded
  Wide World death to the MUD.
- `GET /api/chronicle/v1/events/<event-id>` shows the MUD's stored source event
  and its projection.
- `POST /api/legacy/death` remains available as the compatibility path.

The public MUD route accepts only `wide_world.death`. It remains bounded,
sanitized, rate limited, and idempotent.

## Phase M2 projectors

The MUD application core now understands six typed Wide World events:

| Source event | Rebuildable MUD projection |
| --- | --- |
| `wide_world.death` | Source-linked ghost |
| `wide_world.deed` | Chronicle rumor |
| `wide_world.ending` | Chronicle rumor |
| `wide_world.institution_changed` | Institution summary |
| `wide_world.region_changed` | Regional notice |
| `wide_world.artifact_legacy` | Read-only artifact echo |

Players can read delivered projections with `chronicle` or `worldnews`. The
legacy death route and canonical route use the same application service. All
views can be rebuilt from the immutable event ledger.

Only deaths may cross the unauthenticated public route today. Authentication,
the transactional delivery outbox, and automatic retries are Phase M3 work.
Broader delivery must not be advertised as live before that boundary ships.
