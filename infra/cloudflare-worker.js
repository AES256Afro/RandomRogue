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
const CHRONICLE_DESTINATION = "https://mud.random-rogue.com/api/chronicle/v1/events";
const CHRONICLE_TYPES = new Set([
  "wide_world.death",
  "wide_world.deed",
  "wide_world.ending",
  "wide_world.institution_changed",
  "wide_world.region_changed",
  "wide_world.artifact_legacy",
]);
const RETRY_DELAYS_MS = [15000, 60000, 300000, 900000, 3600000, 21600000, 43200000, 86400000];
const MAX_DELIVERY_ATTEMPTS = RETRY_DELAYS_MS.length;
const MAX_CHRONICLE_EVENTS_PER_DAY = 5000;

// Board keys (R7): plain day numbers for daily worlds; 7000000+week for
// weekly worlds, so the two leaderboards never collide.
function validKey(day) {
  if (!day) return false;
  const now = Date.now();
  if (day >= 7000000) return Math.abs((day - 7000000) - Math.floor(now / (86400000 * 7))) <= 1;
  return Math.abs(day - Math.floor(now / 86400000)) <= 1;
}

function worldIdentity(day) {
  if (day >= 7000000) {
    const number = day - 7000000;
    return { kind: "weekly", number, key: "weekly:" + number };
  }
  return { kind: "daily", number: day, key: "daily:" + day };
}

function publicChronicleEvent(event) {
  return {
    schema: event.schema,
    event_id: event.event_id,
    event_type: event.event_type,
    source: event.source,
    world_key: event.world_key,
    occurred_at: event.occurred_at,
    subject: event.subject,
    ...(event.place ? { place: event.place } : {}),
    tags: [...(event.tags || [])].sort(),
    effects: event.effects || {},
    payload: event.payload || {},
    visibility: event.visibility,
  };
}

async function storeChronicleEvent(env, deathId, event, extraStatements = []) {
  const canonical = publicChronicleEvent(event);
  const raw = JSON.stringify(canonical);
  const now = Date.now();
  await env.DB.batch([
    env.DB.prepare(
    "INSERT OR IGNORE INTO chronicle_events " +
    "(event_id,death_id,schema_name,event_type,source,world_key,occurred_at,subject_json,place_json,tags_json,effects_json,payload_json,visibility,raw_json) " +
    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
    ).bind(
    canonical.event_id,
    deathId,
    canonical.schema,
    canonical.event_type,
    canonical.source,
    canonical.world_key,
    canonical.occurred_at,
    JSON.stringify(canonical.subject),
    JSON.stringify(canonical.place || {}),
    JSON.stringify(canonical.tags),
    JSON.stringify(canonical.effects),
    JSON.stringify(canonical.payload),
    canonical.visibility,
    raw
    ),
    env.DB.prepare(
      "INSERT OR IGNORE INTO chronicle_outbox " +
      "(event_id,status,attempt_count,next_attempt_at,created_at,updated_at) VALUES (?,'queued',0,?,?,?)"
    ).bind(canonical.event_id, now, now, now),
    ...extraStatements,
  ]);
  return canonical;
}

function bridgeError(value) {
  return clean(value instanceof Error ? value.message : String(value)).slice(0, 300);
}

function base64Bytes(value) {
  const binary = atob(value);
  return Uint8Array.from(binary, (character) => character.charCodeAt(0));
}

function bytesBase64(value) {
  const bytes = new Uint8Array(value);
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

async function chronicleSignature(privateKeyBase64, timestamp, eventId, rawBody) {
  if (!privateKeyBase64) throw new Error("Chronicle bridge private key is not configured");
  const encoder = new TextEncoder();
  const key = await crypto.subtle.importKey(
    "pkcs8", base64Bytes(privateKeyBase64), { name: "Ed25519" }, false, ["sign"]
  );
  const signature = await crypto.subtle.sign(
    "Ed25519", key, encoder.encode(timestamp + "." + eventId + "." + rawBody)
  );
  return "v1=" + bytesBase64(signature);
}

async function recordDeadLetter(env, row, attempt, reason, status, duration) {
  const now = Date.now();
  await env.DB.batch([
    env.DB.prepare(
      "INSERT INTO chronicle_delivery_attempts " +
      "(event_id,attempted_at,outcome,http_status,error,duration_ms) VALUES (?,?,'dead',?,?,?)"
    ).bind(row.event_id, now, status, reason, duration),
    env.DB.prepare(
      "UPDATE chronicle_outbox SET status='dead',last_http_status=?,last_error=?,locked_at=NULL,updated_at=? WHERE event_id=?"
    ).bind(status, reason, now, row.event_id),
    env.DB.prepare(
      "INSERT INTO chronicle_dead_letters(event_id,reason,raw_json,failed_at,attempts) VALUES (?,?,?,?,?) " +
      "ON CONFLICT(event_id) DO UPDATE SET reason=excluded.reason,raw_json=excluded.raw_json," +
      "failed_at=excluded.failed_at,attempts=excluded.attempts"
    ).bind(row.event_id, reason, row.raw_json, now, attempt),
  ]);
}

async function deliverOneChronicleEvent(env, row) {
  const claimTime = Date.now();
  const claim = await env.DB.prepare(
    "UPDATE chronicle_outbox SET status='delivering',attempt_count=attempt_count+1,locked_at=?,updated_at=? " +
    "WHERE event_id=? AND ((status IN ('queued','retrying') AND next_attempt_at<=?) " +
    "OR (status='delivering' AND locked_at<=?))"
  ).bind(claimTime, claimTime, row.event_id, claimTime, claimTime - 300000).run();
  if (!claim.meta || Number(claim.meta.changes || 0) !== 1) return false;

  const attempt = Number(row.attempt_count || 0) + 1;
  const started = Date.now();
  let response;
  let failure = "";
  try {
    const timestamp = String(Math.floor(started / 1000));
    const signature = await chronicleSignature(
      env.CHRONICLE_BRIDGE_PRIVATE_KEY, timestamp, row.event_id, row.raw_json
    );
    response = await fetch(CHRONICLE_DESTINATION, {
      method: "POST",
      headers: {
        "content-type": "application/json",
        "x-rr-timestamp": timestamp,
        "x-rr-event-id": row.event_id,
        "x-rr-signature": signature,
      },
      body: row.raw_json,
      signal: AbortSignal.timeout(10000),
    });
    if (!response.ok) failure = "MUD returned HTTP " + response.status;
  } catch (error) {
    failure = bridgeError(error);
  }
  const duration = Date.now() - started;
  const status = response ? response.status : null;
  if (response && response.ok) {
    const now = Date.now();
    await env.DB.batch([
      env.DB.prepare(
        "INSERT INTO chronicle_delivery_attempts " +
        "(event_id,attempted_at,outcome,http_status,error,duration_ms) VALUES (?,?,'delivered',?,'',?)"
      ).bind(row.event_id, now, status, duration),
      env.DB.prepare(
        "UPDATE chronicle_outbox SET status='delivered',delivered_at=?,last_http_status=?," +
        "last_error=NULL,locked_at=NULL,updated_at=? WHERE event_id=?"
      ).bind(now, status, now, row.event_id),
      env.DB.prepare("DELETE FROM chronicle_dead_letters WHERE event_id=?").bind(row.event_id),
    ]);
    return true;
  }

  const retryable = !response || status === 408 || status === 429 || status >= 500;
  if (!retryable || attempt >= MAX_DELIVERY_ATTEMPTS) {
    await recordDeadLetter(env, row, attempt, failure || "permanent delivery failure", status, duration);
    return false;
  }
  const now = Date.now();
  const nextAttempt = now + RETRY_DELAYS_MS[Math.min(attempt - 1, RETRY_DELAYS_MS.length - 1)];
  await env.DB.batch([
    env.DB.prepare(
      "INSERT INTO chronicle_delivery_attempts " +
      "(event_id,attempted_at,outcome,http_status,error,duration_ms) VALUES (?,?,'retry',?,?,?)"
    ).bind(row.event_id, now, status, failure, duration),
    env.DB.prepare(
      "UPDATE chronicle_outbox SET status='retrying',next_attempt_at=?,last_http_status=?," +
      "last_error=?,locked_at=NULL,updated_at=? WHERE event_id=?"
    ).bind(nextAttempt, status, failure, now, row.event_id),
  ]);
  return false;
}

async function deliverChronicleOutbox(env, limit = 10, prune = false) {
  const now = Date.now();
  const due = await env.DB.prepare(
    "SELECT o.event_id,o.attempt_count,e.raw_json FROM chronicle_outbox o " +
    "JOIN chronicle_events e ON e.event_id=o.event_id " +
    "JOIN chronicle_delivery_policy p ON p.event_type=e.event_type AND p.enabled=1 " +
    "WHERE ((o.status IN ('queued','retrying') AND o.next_attempt_at<=?) " +
    "OR (o.status='delivering' AND o.locked_at<=?)) ORDER BY o.next_attempt_at LIMIT ?"
  ).bind(now, now - 300000, Math.max(1, Math.min(50, limit))).all();
  let delivered = 0;
  for (const row of due.results || []) {
    if (await deliverOneChronicleEvent(env, row)) delivered++;
  }
  if (prune) {
    await env.DB.prepare(
      "DELETE FROM chronicle_delivery_attempts WHERE attempted_at < ?"
    ).bind(now - 30 * 86400000).run();
    await env.DB.prepare(
      "DELETE FROM ratelimit WHERE win < ?"
    ).bind(Math.floor(now / 3600000) - 48).run();
  }
  return { due: (due.results || []).length, delivered };
}

function queueChronicleDelivery(ctx, env) {
  if (ctx) ctx.waitUntil(deliverChronicleOutbox(env, 5).catch((error) => {
    console.error(JSON.stringify({ message: "Chronicle delivery failed", error: bridgeError(error) }));
  }));
}

function slug(value, fallback) {
  const out = clean(value || "").toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "").slice(0, 60);
  return out || fallback;
}

function sharedEventFromClient(body, day) {
  const world = worldIdentity(day);
  const kind = String(body.type || "deed").toLowerCase();
  const eventTypes = {
    deed: "wide_world.deed",
    ending: "wide_world.ending",
    institution: "wide_world.institution_changed",
    region: "wide_world.region_changed",
    artifact: "wide_world.artifact_legacy",
  };
  const eventType = eventTypes[kind];
  if (!eventType || !CHRONICLE_TYPES.has(eventType)) return null;
  const token = crypto.randomUUID().replaceAll("-", "");
  const text = clean(body.text || "").replace(/\s+/g, " ").trim().slice(0, 280);
  const title = clean(body.title || "").replace(/\s+/g, " ").trim().slice(0, 80);
  const playerName = clean(body.name || "An unnamed traveler").replace(/\s+/g, " ").trim().slice(0, 80);
  const entityName = clean(body.entity_name || body.name || "An unnamed public thing").replace(/\s+/g, " ").trim().slice(0, 80);
  const entityKey = slug(body.entity_key || entityName, kind);
  let subject = { kind: "character", id: "wide:character:" + world.kind + ":" + world.number + ":" + token, name: playerName };
  let payload;
  if (kind === "deed" || kind === "ending") {
    if (text.length < 2) return null;
    payload = { text, ...(title.length >= 2 ? { title } : {}) };
  } else if (kind === "institution") {
    if (text.length < 2) return null;
    const id = "wide:institution:" + world.kind + ":" + world.number + ":" + entityKey;
    subject = { kind: "institution", id, name: entityName };
    payload = { institution_id: id, name: entityName, summary: text };
  } else if (kind === "region") {
    if (text.length < 2) return null;
    const id = "wide:region:" + world.kind + ":" + world.number + ":" + entityKey;
    subject = { kind: "region", id, name: entityName };
    payload = { region_id: id, name: entityName, notice: text };
  } else {
    const description = clean(body.description || text).replace(/\s+/g, " ").trim().slice(0, 280);
    const provenance = clean(body.provenance || text).replace(/\s+/g, " ").trim().slice(0, 280);
    if (description.length < 2 || provenance.length < 2) return null;
    const id = "wide:artifact:" + world.kind + ":" + world.number + ":" + entityKey;
    subject = { kind: "artifact", id, name: entityName };
    payload = { artifact_id: id, name: entityName, description, provenance };
  }
  const site = clean(body.site || "").replace(/\s+/g, " ").trim().slice(0, 80);
  return publicChronicleEvent({
    schema: "rr.chronicle.v1",
    event_id: "wide:evt:" + world.kind + ":" + world.number + ":" + kind + ":" + token,
    event_type: eventType,
    source: "wide_world",
    world_key: world.key,
    occurred_at: new Date().toISOString(),
    subject,
    ...(site ? { place: { name: site } } : {}),
    tags: [kind, world.kind].sort(),
    effects: {},
    payload,
    visibility: "public",
  });
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

async function adminAuthorized(req, env) {
  const auth = req.headers.get("authorization") || "";
  const provided = auth.startsWith("Bearer ") ? auth.slice(7) : "";
  return validSecret(provided, env.LOAD_KEY);
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

async function claimChronicleBudget(env) {
  const day = Math.floor(Date.now() / 86400000);
  const now = Date.now();
  await env.DB.prepare(
    "INSERT OR IGNORE INTO chronicle_daily_budget(day,accepted,updated_at) VALUES (?,0,?)"
  ).bind(day, now).run();
  const result = await env.DB.prepare(
    "UPDATE chronicle_daily_budget SET accepted=accepted+1,updated_at=? WHERE day=? AND accepted<?"
  ).bind(now, day, MAX_CHRONICLE_EVENTS_PER_DAY).run();
  return Boolean(result.meta && Number(result.meta.changes || 0) === 1);
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
    if (p === "__chronicle_admin" && req.method === "GET") {
      if (!(await adminAuthorized(req, env))) return new Response("no", { status: 403 });
      const statuses = await env.DB.prepare(
        "SELECT status,COUNT(*) AS n FROM chronicle_outbox GROUP BY status ORDER BY status"
      ).all();
      const dead = await env.DB.prepare(
        "SELECT event_id,reason,failed_at,attempts FROM chronicle_dead_letters ORDER BY failed_at DESC LIMIT 20"
      ).all();
      const attempts = await env.DB.prepare(
        "SELECT event_id,attempted_at,outcome,http_status,error,duration_ms " +
        "FROM chronicle_delivery_attempts ORDER BY id DESC LIMIT 30"
      ).all();
      const policy = await env.DB.prepare(
        "SELECT event_type,enabled,updated_at FROM chronicle_delivery_policy ORDER BY event_type"
      ).all();
      const budget = await env.DB.prepare(
        "SELECT day,accepted,updated_at FROM chronicle_daily_budget ORDER BY day DESC LIMIT 7"
      ).all();
      return Response.json({
        statuses: statuses.results || [],
        dead_letters: dead.results || [],
        attempts: attempts.results || [],
        policy: policy.results || [],
        daily_budget: budget.results || [],
        daily_limit: MAX_CHRONICLE_EVENTS_PER_DAY,
      });
    }
    if (p === "__chronicle_replay" && req.method === "POST") {
      if (!(await adminAuthorized(req, env))) return new Response("no", { status: 403 });
      let body;
      try { body = await readJson(req); } catch (error) { return Response.json({ error: "bad request" }, { status: 400 }); }
      const eventId = String(body.event_id || "");
      if (!/^wide:evt:[A-Za-z0-9:_-]{8,120}$/.test(eventId)) {
        return Response.json({ error: "invalid event id" }, { status: 400 });
      }
      const now = Date.now();
      const result = await env.DB.prepare(
        "UPDATE chronicle_outbox SET status='queued',attempt_count=0,next_attempt_at=?,locked_at=NULL," +
        "delivered_at=NULL,last_http_status=NULL,last_error=NULL,updated_at=? WHERE event_id=?"
      ).bind(now, now, eventId).run();
      await env.DB.prepare("DELETE FROM chronicle_dead_letters WHERE event_id=?").bind(eventId).run();
      if (!result.meta || Number(result.meta.changes || 0) !== 1) {
        return Response.json({ error: "event not found" }, { status: 404 });
      }
      queueChronicleDelivery(ctx, env);
      return Response.json({ ok: true, event_id: eventId });
    }
    if (p === "__chronicle_policy") {
      if (!(await adminAuthorized(req, env))) return new Response("no", { status: 403 });
      if (req.method === "GET") {
        const policy = await env.DB.prepare(
          "SELECT event_type,enabled,updated_at FROM chronicle_delivery_policy ORDER BY event_type"
        ).all();
        return Response.json(policy.results || []);
      }
      if (req.method === "POST") {
        let body;
        try { body = await readJson(req); } catch (error) { return Response.json({ error: "bad request" }, { status: 400 }); }
        const eventType = String(body.event_type || "");
        if (!CHRONICLE_TYPES.has(eventType) || typeof body.enabled !== "boolean") {
          return Response.json({ error: "invalid policy" }, { status: 400 });
        }
        await env.DB.prepare(
          "INSERT INTO chronicle_delivery_policy(event_type,enabled,updated_at) VALUES (?,?,?) " +
          "ON CONFLICT(event_type) DO UPDATE SET enabled=excluded.enabled,updated_at=excluded.updated_at"
        ).bind(eventType, body.enabled ? 1 : 0, Date.now()).run();
        if (body.enabled) queueChronicleDelivery(ctx, env);
        return Response.json({ ok: true, event_type: eventType, enabled: body.enabled });
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
      const inserted = await env.DB.prepare(
        "INSERT INTO deaths (day, name, meaning, days, epitaph, site, relics, finished, journey) VALUES (?,?,?,?,?,?,?,?,?)"
      ).bind(day, name, meaning, days, epitaph, site, relics, finished, journey).run();
      // The Confluence: fallen runs cross over to the MUD as ghosts. The
      // canonical fact and its delivery job are committed before any POST.
      if (!finished && await claimChronicleBudget(env)) {
        const deathId = Number(inserted.meta && inserted.meta.last_row_id);
        if (deathId > 0) {
          const world = worldIdentity(day);
          const cause = site ? "fell at " + site : "fell somewhere in the Wide World";
          await storeChronicleEvent(env, deathId, {
            schema: "rr.chronicle.v1",
            event_id: "wide:evt:" + world.kind + ":" + world.number + ":death:" + deathId,
            event_type: "wide_world.death",
            source: "wide_world",
            world_key: world.key,
            occurred_at: new Date().toISOString(),
            subject: {
              kind: "character",
              id: "wide:character:" + world.kind + ":" + world.number + ":" + deathId,
              name,
            },
            ...(site ? { place: { name: site } } : {}),
            tags: ["death", world.kind],
            effects: {},
            payload: { meaning, epitaph, cause, days },
            visibility: "public",
          });
          queueChronicleDelivery(ctx, env);
        }
      }
      return new Response("ok", { headers: CORS });
    }
    if (p === "__chronicle" && req.method === "GET") {
      const eventId = String(url.searchParams.get("id") || "").slice(0, 160);
      if (!/^(wide|mud):evt:[A-Za-z0-9:_-]{8,120}$/.test(eventId)) {
        return Response.json({ error: "invalid event id" }, { status: 400, headers: CORS });
      }
      const row = await env.DB.prepare(
        "SELECT raw_json, created_at FROM chronicle_events WHERE event_id = ? AND visibility = 'public'"
      ).bind(eventId).first();
      if (!row) return Response.json({ error: "chronicle event not found" }, { status: 404, headers: CORS });
      return new Response(row.raw_json, {
        headers: { "content-type": "application/json", "cache-control": "public, max-age=60", ...CORS },
      });
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
      const event = sharedEventFromClient(body, day);
      if (!event) return new Response("bad", { status: 400, headers: CORS });
      if (!(await claimChronicleBudget(env))) {
        return new Response("Chronicle daily budget reached", { status: 503, headers: CORS });
      }
      const feedText = event.event_type === "wide_world.artifact_legacy"
        ? event.payload.name + ": " + event.payload.description
        : event.event_type === "wide_world.institution_changed"
          ? event.payload.name + ": " + event.payload.summary
          : event.event_type === "wide_world.region_changed"
            ? event.payload.name + ": " + event.payload.notice
            : event.payload.text;
      await storeChronicleEvent(env, null, event, [
        env.DB.prepare("INSERT INTO deeds (day, text) VALUES (?,?)").bind(day, feedText.slice(0, 280)),
      ]);
      queueChronicleDelivery(ctx, env);
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
  },
  async scheduled(controller, env, ctx) {
    ctx.waitUntil(deliverChronicleOutbox(env, 25, true));
  },
};

export { chronicleSignature, sharedEventFromClient };
