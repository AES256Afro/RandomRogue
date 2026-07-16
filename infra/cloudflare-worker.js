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
// /__load?key=<LOAD_KEY> re-syncs KV from the GitHub Pages staging deploy.
// The real key is NOT committed; the deployed copy has it inlined.
const LOAD_KEY = "REDACTED-see-deployed-worker";

const FILES = {
  "index.html": "text/html; charset=utf-8",
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

export default {
  async fetch(req, env, ctx) {
    const url = new URL(req.url);
    let p = url.pathname.replace(/^\/+/, "");
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    if (p === "__load") {
      if (url.searchParams.get("key") !== LOAD_KEY) return new Response("no", { status: 403 });
      const out = {};
      for (const f of Object.keys(FILES)) {
        const r = await fetch(ORIGIN + f, { redirect: "follow" });
        if (!r.ok) { out[f] = "fetch " + r.status; continue; }
        const buf = await r.arrayBuffer();
        await env.SITE.put(f, buf);
        out[f] = buf.byteLength;
      }
      return Response.json(out);
    }
    // A daily-world death: leaderboard row, shared-graveyard ghost, and
    // replay journey, all in one insert.
    if (p === "__score" && req.method === "POST") {
      let body;
      try { body = await req.json(); } catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const day = parseInt(body.day);
      if (!validKey(day)) return new Response("stale", { status: 400, headers: CORS });
      const name = String(body.name || "anonymous").slice(0, 48);
      const meaning = String(body.meaning || "").slice(0, 64);
      const days = Math.max(0, Math.min(999, parseInt(body.days) || 0));
      const epitaph = String(body.epitaph || "").slice(0, 120);
      const site = String(body.site || "").slice(0, 64);
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
      let body;
      try { body = await req.json(); } catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const day = parseInt(body.day);
      if (!validKey(day)) return new Response("stale", { status: 400, headers: CORS });
      const text = String(body.text || "").slice(0, 140);
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
    if (p === "") p = "index.html";
    if (p === "play" || p === "play/") p = "play/index.html";
    const type = FILES[p];
    if (!type) return Response.redirect(url.origin + "/", 302);
    const body = await env.SITE.get(p, { type: "arrayBuffer" });
    if (!body) return new Response("content not loaded yet", { status: 503 });
    return new Response(body, { headers: {
      "content-type": type,
      "cache-control": p.endsWith(".html") || p.endsWith("sw.js") ? "public, max-age=300" : "public, max-age=86400"
    }});
  }
};
