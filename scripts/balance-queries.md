# Balance from real players (R7)

The D1 database `random-rogue` (id `6c93123a-db1e-497c-93c3-718a6b5a9f91`)
records every daily/weekly death: name, days survived, epitaph, death site,
whether the life was *finished* (ascended/retired/sailed), and the full
journey log (every choice and outcome). That is ground truth the Monte-Carlo
bot can't give you — the bot clicks at random; players don't.

Quick look without any tooling: **https://random-rogue.com/__stats**
(public, aggregate only) — deaths/avg/best/completions per board, plus the
ten deadliest sites.

Run the deeper queries with wrangler (`npx wrangler d1 execute random-rogue
--remote --command "<sql>"`) or via the Cloudflare dashboard's D1 console.

## Survival curve (per board)
```sql
SELECT day, COUNT(*) AS deaths, ROUND(AVG(days),1) AS avg_days,
       MAX(days) AS best, SUM(finished) AS completed
FROM deaths GROUP BY day ORDER BY day DESC LIMIT 30;
```
Healthy target: avg 10–18 days, completions 2–8%. If avg drops under 8,
players are dying to something new — check the sites query next.

## Deadliest places
```sql
SELECT site, COUNT(*) AS n, ROUND(AVG(days),1) AS avg_day_of_death
FROM deaths WHERE site != '' GROUP BY site ORDER BY n DESC LIMIT 20;
```
A site with many deaths at LOW avg_day is an early-game killer (tune down).
Many deaths at high avg_day is a proper endgame wall (probably fine).

## What players actually choose (journey mining)
Journeys are JSON arrays of {d, s, c, o}. SQLite's json_each unpacks them:
```sql
-- The last choice of every fallen run: what actually killed them.
SELECT json_extract(j.value, '$.c') AS final_choice, COUNT(*) AS n
FROM deaths, json_each(deaths.journey) j
WHERE j.key = json_array_length(deaths.journey) - 1
GROUP BY final_choice ORDER BY n DESC LIMIT 20;
```
```sql
-- Most-taken choices overall (popularity = perceived best strategy).
SELECT json_extract(j.value, '$.c') AS choice, COUNT(*) AS n
FROM deaths, json_each(deaths.journey) j
GROUP BY choice ORDER BY n DESC LIMIT 30;
```
If one choice dominates its event, the other options need better payoffs —
the "goblin toll spreadsheet" problem this game exists to avoid.

## Run length distribution
```sql
SELECT days, COUNT(*) AS n FROM deaths GROUP BY days ORDER BY days;
```

## Housekeeping
Rows accumulate forever (D1 free tier: 5 GB — years of headroom). To prune
boards older than 60 days:
```sql
DELETE FROM deaths WHERE day < CAST(strftime('%s','now')/86400 AS INT) - 60
  AND day < 7000000;
DELETE FROM deeds WHERE day < CAST(strftime('%s','now')/86400 AS INT) - 60
  AND day < 7000000;
```
