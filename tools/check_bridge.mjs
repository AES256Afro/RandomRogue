import fs from 'node:fs';

const schema = JSON.parse(fs.readFileSync('bridge/rr.chronicle.v1.schema.json', 'utf8'));
const fixture = JSON.parse(fs.readFileSync('bridge/fixtures/wide-world-death-v1.json', 'utf8'));

function assert(condition, message) {
  if (!condition) throw new Error(`Chronicle contract: ${message}`);
}

assert(schema.$id === 'https://random-rogue.com/schemas/rr.chronicle.v1.json', 'schema id changed');
assert(fixture.schema === 'rr.chronicle.v1', 'fixture schema mismatch');
assert(/^wide:evt:[A-Za-z0-9:_-]{8,120}$/.test(fixture.event_id), 'invalid event id');
assert(/^daily:[0-9]{1,12}$/.test(fixture.world_key), 'invalid world key');
assert(fixture.event_type === 'wide_world.death', 'fixture must be a death');
assert(fixture.source === 'wide_world', 'fixture source mismatch');
assert(fixture.visibility === 'public', 'fixture visibility mismatch');
assert(fixture.subject?.kind === 'character', 'fixture subject mismatch');
assert(/^wide:[A-Za-z0-9:_-]{3,120}$/.test(fixture.subject?.id ?? ''), 'invalid subject id');
assert(typeof fixture.subject?.name === 'string' && fixture.subject.name.length <= 40, 'invalid subject name');
assert(Number.isFinite(Date.parse(fixture.occurred_at)), 'invalid occurrence time');
assert(JSON.stringify(fixture).includes('\u2014') === false, 'em dash is not allowed');

console.log(`Chronicle contract OK: ${fixture.event_id}`);
