import fs from 'node:fs';

const file = process.argv[2] || 'site/index.html';
const html = fs.readFileSync(file, 'utf8');
const failures = [];

function requireText(text, label = text) {
  if (!html.includes(text)) failures.push(`missing ${label}`);
}

const ids = [...html.matchAll(/\sid="([^"]+)"/g)].map(match => match[1]);
const uniqueIds = new Set(ids);
if (ids.length !== uniqueIds.size) failures.push('duplicate HTML id');

for (const id of ['top', 'worlds', 'updates', 'features', 'random-card', 'patch-notes', 'play']) {
  if (!uniqueIds.has(id)) failures.push(`missing #${id}`);
}

for (const match of html.matchAll(/href="#([^"]+)"/g)) {
  if (!uniqueIds.has(match[1])) failures.push(`broken anchor #${match[1]}`);
}

for (const [text, label] of [
  ['https://github.com/AES256Afro/RandomRogue', 'GitHub link'],
  ['https://mud.random-rogue.com', 'MUD link'],
  ['https://theencryptedafro.itch.io/random-rogue', 'itch.io link'],
  ['LIVE NOW', 'live bridge label'],
  ['NEXT BRIDGE', 'planned bridge label'],
  ['PATCH NOTES', 'patch notes navigation'],
  ['DEAL ANOTHER CARD', 'random card control'],
]) requireText(text, label);

const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match => match[1]);
if (scripts.length !== 1) failures.push(`expected one inline script, found ${scripts.length}`);
else {
  try { new Function(scripts[0]); }
  catch (error) { failures.push(`inline script syntax: ${error.message}`); }
}

const randomExamples = (scripts[0]?.match(/meta:\s*'/g) || []).length;
if (randomExamples < 6) failures.push(`random example pool too small: ${randomExamples}`);

if (html.includes('—')) failures.push('em dash found');

if (failures.length) {
  console.error(`site check failed: ${failures.join('; ')}`);
  process.exit(1);
}

console.log(`site check: ${uniqueIds.size} ids, ${randomExamples} random cards, anchors and bridge labels pass`);
