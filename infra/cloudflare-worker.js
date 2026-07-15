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
