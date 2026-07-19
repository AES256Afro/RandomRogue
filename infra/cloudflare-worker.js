// random-rogue-site — the Cloudflare Worker serving random-rogue.com.
// Deployed on account c3975c2a... with bindings:
//   SITE — KV namespace "random-rogue-site" (3a6ee10b605147fbaeff9c1f8cba0e3f)
//   DB   — D1 database  "random-rogue"      (6c93123a-db1e-497c-93c3-718a6b5a9f91)
// and zone routes random-rogue.com/* + www.random-rogue.com/*.
//
// D1 schema:
//   deaths(id, day, name, meaning, days, epitaph, site, relics, finished,
//          journey, ts)   — daily-world deaths + replay logs
//   deeds(id, day, text, ts) — the live deeds feed
//
// /__load re-syncs KV from the GitHub staging deploy. LOAD_KEY is a Wrangler
// secret binding and must never be committed or placed in a URL.

const FILES = {
  "index.html": "text/html; charset=utf-8",
  "favicon.png": "image/png",
  "favicon.ico": "image/x-icon",
  "play/index.html": "text/html; charset=utf-8",
  "play/random_rogue.js": "application/javascript",
  "play/random_rogue.wasm": "application/wasm",
  "play/random_rogue.data": "application/octet-stream",
  "play/manifest.webmanifest": "application/manifest+json",
  "play/sw.js": "application/javascript",
  "play/icon-192.png": "image/png",
  "play/icon-512.png": "image/png",
};
// Sync origin: the CI-published site-dist branch. (GitHub Pages 301s the
// github.io URL to the custom domain — i.e. back to this worker — so it
// can't be the origin.)
const ORIGIN = "https://raw.githubusercontent.com/AES256Afro/RandomRogue/site-dist/";
const CORS = { "access-control-allow-origin": "*", "access-control-allow-methods": "GET,POST,OPTIONS", "access-control-allow-headers": "content-type" };

// Board keys (R7): plain day numbers for daily worlds; 7000000+week for
// weekly worlds, so the two leaderboards never collide.
function validKey(day) {
  if (!day) return false;
  const now = Date.now();
  if (day >= 7000000) return Math.abs((day - 7000000) - Math.floor(now / (86400000 * 7))) <= 1;
  return Math.abs(day - Math.floor(now / 86400000)) <= 1;
}

// Strip control characters from player-supplied strings (R10).
function clean(s) { return String(s).replace(/[\u0000-\u001f\u007f]/g, " "); }
async function validSecret(provided, expected) {
  const enc = new TextEncoder();
  const [a, b] = await Promise.all([
    crypto.subtle.digest("SHA-256", enc.encode(provided || "")),
    crypto.subtle.digest("SHA-256", enc.encode(expected || "")),
  ]);
  return Boolean(expected) && crypto.subtle.timingSafeEqual(a, b);
}

async function readJson(req) {
  const declared = Number(req.headers.get("content-length") || 0);
  if (declared > 100000) throw new Error("body too large");
  return req.json();
}
// Per-IP hourly write budget via D1 (table ratelimit(ip, win, n));
// generous for humans, dull for scripts.
async function overLimit(env, req) {
  try {
    const ip = req.headers.get("cf-connecting-ip") || "0";
    const win = Math.floor(Date.now() / 3600000);
    await env.DB.prepare("INSERT INTO ratelimit (ip, win, n) VALUES (?,?,1) ON CONFLICT(ip, win) DO UPDATE SET n = n + 1").bind(ip, win).run();
    const row = await env.DB.prepare("SELECT n FROM ratelimit WHERE ip = ? AND win = ?").bind(ip, win).first();
    return row && row.n > 40;
  } catch (e) { return false; }
}

export default {
  async fetch(req, env, ctx) {
    const url = new URL(req.url);
    let p = url.pathname.replace(/^\/+/, "");
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    if (p === "__load") {
      const auth = req.headers.get("authorization") || "";
      const provided = auth.startsWith("Bearer ") ? auth.slice(7) : "";
      if (!(await validSecret(provided, env.LOAD_KEY)))
        return new Response("no", { status: 403 });
      // Fetch every file before changing anything. A release becomes visible
      // only after all versioned keys exist and the one pointer is flipped.
      try {
        const fetched = await Promise.all(Object.keys(FILES).map(async (f) => {
          const r = await fetch(ORIGIN + f, { redirect: "follow" });
          if (!r.ok) throw new Error("fetch " + f + ": " + r.status);
          return [f, await r.arrayBuffer()];
        }));
        const release = Date.now().toString(36) + "-" + crypto.randomUUID().slice(0, 8);
        await Promise.all(fetched.map(([f, buf]) =>
          env.SITE.put("release/" + release + "/" + f, buf)));
        await env.SITE.put("__release", release);
        const out = { release };
        for (const [f, buf] of fetched) out[f] = buf.byteLength;
        return Response.json(out);
      } catch (error) {
        console.error(JSON.stringify({
          message: "site sync failed",
          error: error instanceof Error ? error.message : String(error),
        }));
        return Response.json({ error: "site sync failed" }, { status: 502 });
      }
    }
    // A daily-world death: leaderboard row, shared-graveyard ghost, and
    // replay journey, all in one insert.
    if (p === "__score" && req.method === "POST") {
      if (await overLimit(env, req)) return new Response("slow down", { status: 429, headers: CORS });
      let body;
      try { body = await readJson(req); } catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const day = parseInt(body.day);
      if (!validKey(day)) return new Response("stale", { status: 400, headers: CORS });
      const name = clean(body.name || "anonymous").slice(0, 48);
      const meaning = clean(body.meaning || "").slice(0, 64);
      const days = Math.max(0, Math.min(999, parseInt(body.days) || 0));
      const epitaph = clean(body.epitaph || "").slice(0, 120);
      const site = clean(body.site || "").slice(0, 64);
      const finished = body.finished ? 1 : 0;
      let relics = "[]";
      try {
        const r = Array.isArray(body.relics) ? body.relics.slice(0, 2) : [];
        relics = JSON.stringify(r.map(x => ({ name: String(x.name || "").slice(0, 48), quirk: String(x.quirk || "").slice(0, 120) })));
      } catch (e) {}
      let journey = "[]";
      try {
        const j = Array.isArray(body.journey) ? body.journey.slice(0, 240) : [];
        journey = JSON.stringify(j.map(s => ({ d: parseInt(s.d) || 0, s: String(s.s || "").slice(0, 40), c: String(s.c || "").slice(0, 70), o: String(s.o || "").slice(0, 110) })));
        if (journey.length > 80000) journey = "[]";
      } catch (e) {}
      await env.DB.prepare(
        "INSERT INTO deaths (day, name, meaning, days, epitaph, site, relics, finished, journey) VALUES (?,?,?,?,?,?,?,?,?)"
      ).bind(day, name, meaning, days, epitaph, site, relics, finished, journey).run();
      // The Confluence: fallen runs cross over to the MUD as ghosts.
      // Fire and forget; the afterlife does not do retries.
      if (!finished && ctx) {
        ctx.waitUntil(fetch("https://mud.random-rogue.com/api/legacy/death", {
          method: "POST",
          headers: { "content-type": "application/json" },
          body: JSON.stringify({ name, meaning, epitaph, days, cause: site ? "fell at " + site : "fell somewhere in the Wide World" }),
        }).catch(() => {}));
      }
      return new Response("ok", { headers: CORS });
    }
    if (p === "__scores") {
      const day = parseInt(url.searchParams.get("day")) || Math.floor(Date.now() / 86400000);
      const rs = await env.DB.prepare(
        "SELECT id, name, days, epitaph FROM deaths WHERE day = ? ORDER BY days DESC, ts ASC LIMIT 25"
      ).bind(day).all();
      return new Response(JSON.stringify(rs.results || []), { headers: { "content-type": "application/json", ...CORS } });
    }
    // Watch how someone else's run actually went.
    if (p === "__replay") {
      const id = parseInt(url.searchParams.get("id")) || -1;
      const row = await env.DB.prepare("SELECT journey FROM deaths WHERE id = ?").bind(id).first();
      return new Response((row && row.journey) || "[]", { headers: { "content-type": "application/json", ...CORS } });
    }
    // Strangers fallen in this daily world: ghosts for other players.
    if (p === "__ghosts") {
      const day = parseInt(url.searchParams.get("day")) || Math.floor(Date.now() / 86400000);
      const n = Math.max(1, Math.min(5, parseInt(url.searchParams.get("n")) || 3));
      const not = String(url.searchParams.get("not") || "");
      const rs = await env.DB.prepare(
        "SELECT name, meaning, days, epitaph, site, relics FROM deaths WHERE day = ? AND name != ? ORDER BY RANDOM() LIMIT ?"
      ).bind(day, not, n).all();
      return new Response(JSON.stringify(rs.results || []), { headers: { "content-type": "application/json", ...CORS } });
    }
    // The live deeds feed: notable acts, broadcast to the same daily world.
    if (p === "__deed" && req.method === "POST") {
      if (await overLimit(env, req)) return new Response("slow down", { status: 429, headers: CORS });
      let body;
      try { body = await readJson(req); } catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const day = parseInt(body.day);
      if (!validKey(day)) return new Response("stale", { status: 400, headers: CORS });
      const text = clean(body.text || "").slice(0, 140);
      if (!text) return new Response("bad", { status: 400, headers: CORS });
      await env.DB.prepare("INSERT INTO deeds (day, text) VALUES (?,?)").bind(day, text).run();
      return new Response("ok", { headers: CORS });
    }
    if (p === "__deeds") {
      const day = parseInt(url.searchParams.get("day")) || Math.floor(Date.now() / 86400000);
      const rs = await env.DB.prepare(
        "SELECT text FROM deeds WHERE day = ? ORDER BY ts DESC LIMIT 12"
      ).bind(day).all();
      return new Response(JSON.stringify(rs.results || []), { headers: { "content-type": "application/json", ...CORS } });
    }
    // Public aggregate stats: how the fallen are actually falling (R7).
    if (p === "__stats") {
      const rs = await env.DB.prepare(
        "SELECT day, COUNT(*) AS deaths, ROUND(AVG(days),1) AS avg_days, MAX(days) AS best, SUM(finished) AS completed FROM deaths GROUP BY day ORDER BY day DESC LIMIT 14"
      ).all();
      const sites = await env.DB.prepare(
        "SELECT site, COUNT(*) AS n FROM deaths WHERE site != '' GROUP BY site ORDER BY n DESC LIMIT 10"
      ).all();
      return new Response(JSON.stringify({ boards: rs.results || [], deadliest_sites: sites.results || [] }),
        { headers: Object.assign({ "content-type": "application/json" }, CORS) });
    }
    // Privacy-safe balance telemetry. The Worker immediately aggregates each
    // batch. No player name, seed, free text, address, or individual row is
    // stored. Players can disable this from the game options.
    if (p === "__telemetry" && req.method === "POST") {
      let body;
      try { body = await readJson(req); }
      catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const incoming = Array.isArray(body.events) ? body.events.slice(0, 32) : [];
      const statements = [];
      for (const value of incoming) {
        if (!value || value.v !== 1) continue;
        const event = String(value.event || "").replace(/[^a-zA-Z0-9_:-]/g, "").slice(0, 64);
        const deck = String(value.deck || "unknown").replace(/[^a-zA-Z0-9_:-]/g, "").slice(0, 24);
        const choice = Math.max(-1, Math.min(8, parseInt(value.choice) || 0));
        const day = Math.max(0, Math.min(999, parseInt(value.day) || 0));
        const dayBand = Math.floor(day / 10) * 10;
        const score = Math.max(0, Math.min(300, parseInt(value.score) || 0));
        const gap = Math.max(0, Math.min(999, parseInt(value.gap) || 0));
        const end = value.end ? 1 : 0;
        if (!event) continue;
        statements.push(env.DB.prepare(
          "INSERT INTO telemetry_rollup (event,choice,deck,day_band,n,score_total,gap_total,run_ends) VALUES (?,?,?,?,1,?,?,?) " +
          "ON CONFLICT(event,choice,deck,day_band) DO UPDATE SET n=n+1,score_total=score_total+excluded.score_total,gap_total=gap_total+excluded.gap_total,run_ends=run_ends+excluded.run_ends"
        ).bind(event, choice, deck, dayBand, score, gap, end));
      }
      if (statements.length) await env.DB.batch(statements);
      return new Response("ok", { headers: CORS });
    }
    if (p === "__telemetry_stats") {
      const rs = await env.DB.prepare(
        "SELECT event, choice, deck, SUM(n) AS n, ROUND(1.0*SUM(score_total)/SUM(n),1) AS avg_score, ROUND(1.0*SUM(gap_total)/SUM(n),1) AS avg_gap, SUM(run_ends) AS run_ends FROM telemetry_rollup GROUP BY event,choice,deck ORDER BY n DESC LIMIT 250"
      ).all();
      return new Response(JSON.stringify(rs.results || []),
        { headers: Object.assign({ "content-type": "application/json" }, CORS) });
    }
    if (p === "") p = "index.html";
    if (p === "play" || p === "play/") p = "play/index.html";
    const type = FILES[p];
    if (!type) return Response.redirect(url.origin + "/", 302);
    const release = await env.SITE.get("__release");
    const key = release ? "release/" + release + "/" + p : p;
    const body = await env.SITE.get(key, { type: "arrayBuffer" });
    if (!body) return new Response("content not loaded yet", { status: 503 });
    return new Response(body, { headers: {
      "content-type": type,
      // Public URLs are stable while the release pointer changes. Revalidate
      // each request so a browser never pairs files from different releases.
      "cache-control": "no-cache, must-revalidate"
    }});
  }
};
