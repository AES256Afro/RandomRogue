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

## Phase M1 routes

- `GET /__chronicle?id=<event-id>` reads a public canonical source event from
  Cloudflare D1.
- `POST /api/chronicle/v1/events` lets the Cloudflare bridge deliver a bounded
  Wide World death to the MUD.
- `GET /api/chronicle/v1/events/<event-id>` shows the MUD's stored source event
  and its projection.
- `POST /api/legacy/death` remains available as the compatibility path.

M1 accepts only `wide_world.death` in the MUD. The schema lists reserved future
types so later releases can extend projectors without renaming the envelope.
Authentication, the transactional delivery outbox, and automatic retries are
Phase M3 work. The M1 route remains bounded, sanitized, rate limited, and
idempotent while that transport work is built.
