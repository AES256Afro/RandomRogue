// random-rogue-site — the Cloudflare Worker serving random-rogue.com.
// Deployed on account c3975c2a... with a KV binding SITE
// (namespace "random-rogue-site", id 3a6ee10b605147fbaeff9c1f8cba0e3f)
// and zone routes random-rogue.com/* + www.random-rogue.com/*.
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
};
const CORS = { 'access-control-allow-origin': '*', 'access-control-allow-methods': 'GET,POST,OPTIONS', 'access-control-allow-headers': 'content-type' };
const ORIGIN = "https://aes256afro.github.io/RandomRogue/";

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    let p = url.pathname.replace(/^\/+/, "");
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
    
    // Daily leaderboard + shared graveyard (D1 binding DB,
    // database "random-rogue" id 6c93123a-db1e-497c-93c3-718a6b5a9f91).
    if (p === "__score" && req.method === "POST") {
      let body;
      try { body = await req.json(); } catch (e) { return new Response("bad", { status: 400, headers: CORS }); }
      const day = parseInt(body.day);
      const today = Math.floor(Date.now() / 86400000);
      if (!day || Math.abs(day - today) > 1) return new Response("stale", { status: 400, headers: CORS });
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
      await env.DB.prepare(
        "INSERT INTO deaths (day, name, meaning, days, epitaph, site, relics, finished) VALUES (?,?,?,?,?,?,?,?)"
      ).bind(day, name, meaning, days, epitaph, site, relics, finished).run();
      return new Response("ok", { headers: CORS });
    }
    if (p === "__scores") {
      const day = parseInt(url.searchParams.get("day")) || Math.floor(Date.now() / 86400000);
      const rs = await env.DB.prepare(
        "SELECT name, days, epitaph FROM deaths WHERE day = ? ORDER BY days DESC, ts ASC LIMIT 25"
      ).bind(day).all();
      return new Response(JSON.stringify(rs.results || []), { headers: Object.assign({ "content-type": "application/json" }, CORS) });
    }
    // Strangers fallen in this daily world: ghosts for other players.
    if (p === "__ghosts") {
      const day = parseInt(url.searchParams.get("day")) || Math.floor(Date.now() / 86400000);
      const n = Math.max(1, Math.min(5, parseInt(url.searchParams.get("n")) || 3));
      const not = String(url.searchParams.get("not") || "");
      const rs = await env.DB.prepare(
        "SELECT name, meaning, days, epitaph, site, relics FROM deaths WHERE day = ? AND name != ? ORDER BY RANDOM() LIMIT ?"
      ).bind(day, not, n).all();
      return new Response(JSON.stringify(rs.results || []), { headers: Object.assign({ "content-type": "application/json" }, CORS) });
    }
if (p === "") p = "index.html";
    if (p === "play" || p === "play/") p = "play/index.html";
    const type = FILES[p];
    if (!type) return Response.redirect(url.origin + "/", 302);
    const body = await env.SITE.get(p, { type: "arrayBuffer" });
    if (!body) return new Response("content not loaded yet", { status: 503 });
    return new Response(body, { headers: {
      "content-type": type,
      "cache-control": p.endsWith(".html") ? "public, max-age=300" : "public, max-age=86400"
    }});
  }
};
