# Chronicle Bridge Cost Estimate

Last reviewed: 2026-07-19

## Current footprint

The live D1 database measured 92 KB before M3, with one Chronicle event, one
death, one deed, and twelve telemetry rows. Random Rogue MUD already runs on
the existing roughly $6 per month server. M3 does not require another server.

## Expected monthly change

- If Cloudflare remains on the Free plan, expected bridge increase: **$0 per
  month**.
- If the account must move to Workers Paid, expected increase: **$5 per
  month**, making the known hosting total roughly **$11 per month** before any
  unrelated email or optional AI usage.
- If Workers Paid is already active for the account, expected bridge increase:
  **$0 per month at current and moderate traffic**.

The Cloudflare account's billing plan is not exposed by this repository or the
Wrangler project configuration, so the only unresolved part of the estimate is
whether the existing account already pays the $5 Workers minimum.

## Included limits used by this estimate

Official pricing pages:

- Workers: https://developers.cloudflare.com/workers/platform/pricing/
- D1: https://developers.cloudflare.com/d1/platform/pricing/
- KV: https://developers.cloudflare.com/kv/platform/pricing/

As reviewed on 2026-07-19:

- Workers Free includes 100,000 requests per day.
- Workers Paid starts at $5 per month and includes 10 million requests and 30
  million CPU milliseconds per month.
- D1 Free includes 5 million rows read per day, 100,000 rows written per day,
  and 5 GB of storage.
- D1 on Workers Paid includes 25 billion rows read, 50 million rows written,
  and 5 GB of storage per month.
- KV Free includes 100,000 reads per day, 1,000 writes per day, and 1 GB.

The existing website also consumes Worker requests and KV reads. Those uses
count toward the same Cloudflare account limits and are more likely to reach a
Free-plan request limit than the bridge itself.

## Bridge load model

One normally delivered Chronicle event is budgeted at about 12 to 14 D1 row
writes. That conservative count includes the canonical event, indexes, outbox,
claim, delivery attempt, completion, and daily budget record. Retries add more
attempt and state writes.

The minute recovery scheduler adds about 43,200 Worker invocations per month.
It reads only due jobs, so an empty queue has very little D1 work.

| Accepted events | Events per month | Approximate D1 writes | Expected bridge cost |
| --- | ---: | ---: | ---: |
| 100 per day | 3,000 | 42,000 | $0 incremental |
| 1,000 per day | 30,000 | 420,000 | $0 incremental |
| 5,000 per day | 150,000 | 2.1 million | $0 incremental on Free if other usage remains within limits |
| 10,000 per day | 300,000 | 4.2 million | Likely requires Workers Paid because daily D1 writes can exceed Free limits |
| 100,000 per day | 3 million | 42 million | About $5 total Workers charge, still inside paid D1 included writes |

The deployed bridge hard-caps accepted Chronicle events at 5,000 per UTC day.
This keeps ordinary bridge writes below the Free D1 daily write allowance even
under hostile traffic. Per-IP write limits, bounded payloads, per-type kill
switches, retry ceilings, and dead letters add further cost controls.

## What can change the estimate

- Large growth in website page and game asset traffic
- A long MUD outage that causes many retry attempts
- Raising or removing the 5,000-event daily bridge cap
- Moving static assets away from KV or onto Workers Static Assets, which could
  reduce current KV and Worker pressure
- Optional companion AI calls in the MUD, which are separate from this bridge

## Budget recommendation

Plan for **$6 per month now**, assuming Cloudflare Free plus the existing MUD
server. Keep **$11 per month** as the conservative near-term ceiling if Workers
Paid becomes necessary. Review Cloudflare usage at 50 percent and 80 percent of
the daily request or D1 write allowances before changing the bridge cap.
