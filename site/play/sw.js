// Random Rogue service worker: play offline after the first visit (R5).
// Network-first so updates land immediately; cache answers when the road
// has no signal. Leaderboard/ghost calls (__*) are never cached.
const CACHE = 'random-rogue-v1';
const CORE = ['./', 'index.html', 'random_rogue.js', 'random_rogue.wasm',
              'random_rogue.data', 'manifest.webmanifest', 'icon-192.png',
              'icon-512.png'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CORE)).catch(() => {}));
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== 'GET') return;
  if (url.pathname.includes('/__')) return; // live endpoints stay live
  e.respondWith(
    fetch(e.request)
      .then((r) => {
        if (r.ok && url.origin === self.location.origin) {
          const copy = r.clone();
          caches.open(CACHE).then((c) => c.put(e.request, copy));
        }
        return r;
      })
      .catch(() => caches.match(e.request).then((m) => m || caches.match('index.html')))
  );
});
