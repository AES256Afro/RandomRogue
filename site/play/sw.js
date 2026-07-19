// Random Rogue service worker: play offline after the first visit (R5).
// Network-first so updates land immediately; cache answers when the road
// has no signal. Leaderboard/ghost calls (__*) are never cached.
const CACHE_PREFIX = 'random-rogue-';
const CACHE = CACHE_PREFIX + 'v18';
const CORE = ['./', 'index.html', 'random_rogue.js', 'random_rogue.wasm',
              'random_rogue.data', 'manifest.webmanifest', 'icon-192.png',
              'icon-512.png'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CORE)).catch(() => {}));
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil((async () => {
    const names = await caches.keys();
    await Promise.all(names
      .filter((name) => name.startsWith(CACHE_PREFIX) && name !== CACHE)
      .map((name) => caches.delete(name)));
    await self.clients.claim();
  })());
});

self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== 'GET') return;
  if (url.pathname.includes('/__')) return; // live endpoints stay live
  e.respondWith(
    fetch(e.request, { cache: 'no-store' })
      .then((r) => {
        if (r.ok && url.origin === self.location.origin) {
          const copy = r.clone();
          e.waitUntil(caches.open(CACHE).then((c) => c.put(e.request, copy)));
        }
        return r;
      })
      .catch(async () => {
        const cached = await caches.match(e.request);
        if (cached) return cached;
        // Only navigations may fall back to the app shell. Returning HTML for
        // a missing wasm or data file produces a misleading compile failure.
        if (e.request.mode === 'navigate') {
          const shell = await caches.match('index.html');
          if (shell) return shell;
        }
        return new Response('Offline asset unavailable', {
          status: 503,
          headers: { 'content-type': 'text/plain; charset=utf-8' },
        });
      })
  );
});
