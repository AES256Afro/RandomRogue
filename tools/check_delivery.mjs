import fs from 'node:fs';
import { generateKeyPairSync, verify } from 'node:crypto';
import { chronicleSignature, sharedEventFromClient } from '../infra/cloudflare-worker.js';

function assert(condition, message) {
  if (!condition) throw new Error(`Chronicle delivery: ${message}`);
}

const keys = generateKeyPairSync('ed25519');
const privateKey = keys.privateKey.export({ type: 'pkcs8', format: 'der' }).toString('base64');
const timestamp = '1784494800';
const eventId = 'wide:evt:daily:20653:deed:contracttest';
const body = '{"test":"body bytes matter"}';
const signature = await chronicleSignature(privateKey, timestamp, eventId, body);
assert(signature.startsWith('v1='), 'signature version is missing');
assert(verify(
  null,
  Buffer.from(`${timestamp}.${eventId}.${body}`, 'utf8'),
  keys.publicKey,
  Buffer.from(signature.slice(3), 'base64'),
), 'Worker Ed25519 signature does not match the shared contract');

const base = {
  day: Math.floor(Date.now() / 86400000),
  name: 'Ada Common',
  title: 'A bridge opened',
  text: 'Ada recorded a public fact and left maintenance notes.',
};
for (const type of ['deed', 'ending', 'institution', 'region', 'artifact']) {
  const event = sharedEventFromClient({
    ...base,
    type,
    entity_key: `${type}_contract`,
    entity_name: `Contract ${type}`,
    description: 'A bounded artifact description.',
    provenance: 'Recovered during an automated contract check.',
  }, base.day);
  assert(event, `${type} event was not constructed`);
  assert(event.schema === 'rr.chronicle.v1', `${type} schema drifted`);
  assert(event.event_type.startsWith('wide_world.'), `${type} type drifted`);
  assert(event.world_key.startsWith('daily:'), `${type} world key drifted`);
  assert(event.visibility === 'public', `${type} visibility drifted`);
}

const migration = fs.readFileSync('infra/migrations/0004_chronicle_delivery.sql', 'utf8');
for (const table of [
  'chronicle_outbox',
  'chronicle_delivery_attempts',
  'chronicle_dead_letters',
  'chronicle_delivery_policy',
  'chronicle_daily_budget',
]) {
  assert(migration.includes(`CREATE TABLE IF NOT EXISTS ${table}`), `${table} migration is missing`);
}
for (const status of ['queued', 'delivering', 'retrying', 'delivered', 'dead']) {
  assert(migration.includes(`'${status}'`), `${status} outbox state is missing`);
}

const config = fs.readFileSync('infra/wrangler.jsonc', 'utf8');
assert(config.includes('"* * * * *"'), 'minute recovery trigger is missing');
const worker = fs.readFileSync('infra/cloudflare-worker.js', 'utf8');
assert(!worker.includes('api/legacy/death'), 'Worker still bypasses the canonical outbox');
assert(worker.includes('CHRONICLE_BRIDGE_PRIVATE_KEY'), 'bridge private key binding is missing');
assert(worker.includes('MAX_CHRONICLE_EVENTS_PER_DAY = 5000'), 'daily cost guard is missing');

console.log('Chronicle delivery contract OK: Ed25519, five client types, outbox, recovery, and policy');
