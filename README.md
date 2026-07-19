# Random Rogue

A lofi-pixel, Reigns-style roguelike where every world is generated from a
seed: geography, factions, 1,000 years of simulated history across five eras,
and every rumor, book, artifact, and item quirk ties back to that history.
Regions have political economies. Named people pursue agendas. Solidarity,
rent, supply, pollution, and power continue moving without the player.

- **Design docs:** [PLAN.md](PLAN.md) - [WORLDGEN.md](WORLDGEN.md) - [NARRATIVE.md](NARRATIVE.md) - [SCENARIO_1000.md](SCENARIO_1000.md)
- **Content guide:** [AUTHORING.md](AUTHORING.md)
- **Targets:** Windows, Linux, macOS, Browser (desktop + iPad)

## Play

- **1-9 / tap**: choose
- **Tab / PACK button**: inventory
- **Enter / tap**: continue
- **S**: enter a seed - **M**: mute
- Same seed = same world, same history, on every platform.

## Build (desktop)

Needs CMake 3.24+, Ninja, and a C++17 compiler. Raylib and nlohmann/json are
fetched automatically.

```
cmake --preset windows-gcc      # or: cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build --preset windows-gcc
build/windows/random_rogue.exe
```

On Windows, `scripts/env.ps1` puts the local toolchain
(`~/tools/{cmake,ninja,mingw64}`, `~/emsdk`) on PATH.

## Build (browser)

```
emcmake cmake -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web
cmake --build build/web
npx http-server build/web -p 8087   # open random_rogue.html
```

## Modding

Everything is JSON. See [MODDING.md](MODDING.md) for the full schema -
events, items, quirks, traits, companions, grammars, and languages.

## Dev tools

```
build/windows/chronicle_dump.exe <seed> assets
```

Prints a world's full simulated history as readable text. If the dump is fun
to read, the game is working. Native and WASM dumps are byte-identical for
the same seed. That is load-bearing; see the determinism note in
src/language.cpp before touching RNG call sites.

The playable deck currently contains 501 authored scenarios. Exact event ids
deal without replacement for an entire life, saves preserve that seen set, and
each world carries a 240-card cooldown across generations. `SCENARIO_1000.md`
and `assets/data/scenario_targets.json` define and measure the path to 1,000.

## Hosting (random-rogue.com)

The domain is served by a Cloudflare Worker (`random-rogue-site`) that reads
the landing page + game from a KV namespace, independent of GitHub, so repo
visibility never affects the site. Zone routes: `random-rogue.com/*` and
`www.random-rogue.com/*`.

To publish a site/game update: push to `main` (CI deploys GitHub Pages at
aes256afro.github.io/RandomRogue), then hit the worker's `/__load` endpoint
(URL with key kept privately) to sync the new files into KV.

## Releasing

- **CI:** pushing to GitHub runs `.github/workflows/build.yml`, producing
  Windows / Linux / macOS binaries and a web bundle as artifacts.
- **itch.io:** upload the `random-rogue-web` artifact zip (it contains
  `index.html`), mark it "This file will be played in the browser", and
  enable mobile-friendly. The page already blocks iPad zoom/scroll and
  supports touch for every interaction.
- Desktop zips: pair the binary with its `assets/` folder.
