import fs from 'node:fs';

const schema = JSON.parse(fs.readFileSync('bridge/rr.chronicle.v1.schema.json', 'utf8'));
const fixtureDir = 'bridge/fixtures';
const fixtureNames = fs.readdirSync(fixtureDir)
  .filter((name) => name.endsWith('.json'))
  .sort();

const expected = new Map([
  ['wide-world-death-v1.json', ['wide_world.death', 'character']],
  ['wide-world-deed-v1.json', ['wide_world.deed', 'character']],
  ['wide-world-ending-v1.json', ['wide_world.ending', 'character']],
  ['wide-world-institution-v1.json', ['wide_world.institution_changed', 'institution']],
  ['wide-world-region-v1.json', ['wide_world.region_changed', 'region']],
  ['wide-world-artifact-v1.json', ['wide_world.artifact_legacy', 'artifact']],
]);

function assert(condition, message) {
  if (!condition) throw new Error(`Chronicle contract: ${message}`);
}

function bounded(value, min, max) {
  return typeof value === 'string' && value.length >= min && value.length <= max;
}

function checkPayload(fixture, name) {
  const payload = fixture.payload;
  assert(payload && typeof payload === 'object' && !Array.isArray(payload), `${name} payload missing`);
  switch (fixture.event_type) {
    case 'wide_world.death':
      assert(payload.days === undefined || Number.isInteger(payload.days), `${name} days must be an integer`);
      assert(payload.days === undefined || (payload.days >= 0 && payload.days <= 9999), `${name} days out of range`);
      break;
    case 'wide_world.deed':
    case 'wide_world.ending':
      assert(bounded(payload.text, 2, 280), `${name} rumor text invalid`);
      assert(payload.title === undefined || bounded(payload.title, 2, 80), `${name} rumor title invalid`);
      break;
    case 'wide_world.institution_changed':
      assert(/^wide:institution:[A-Za-z0-9:_-]{1,100}$/.test(payload.institution_id ?? ''), `${name} institution id invalid`);
      assert(bounded(payload.name, 2, 80) && bounded(payload.summary, 2, 280), `${name} institution text invalid`);
      break;
    case 'wide_world.region_changed':
      assert(/^wide:region:[A-Za-z0-9:_-]{1,100}$/.test(payload.region_id ?? ''), `${name} region id invalid`);
      assert(bounded(payload.name, 2, 80) && bounded(payload.notice, 2, 280), `${name} region text invalid`);
      break;
    case 'wide_world.artifact_legacy':
      assert(/^wide:artifact:[A-Za-z0-9:_-]{1,100}$/.test(payload.artifact_id ?? ''), `${name} artifact id invalid`);
      assert(bounded(payload.name, 2, 80), `${name} artifact name invalid`);
      assert(bounded(payload.description, 2, 280), `${name} artifact description invalid`);
      assert(bounded(payload.provenance, 2, 280), `${name} artifact provenance invalid`);
      break;
    default:
      assert(false, `${name} has unsupported event type`);
  }
}

assert(schema.$id === 'https://random-rogue.com/schemas/rr.chronicle.v1.json', 'schema id changed');
assert(fixtureNames.length === expected.size, 'fixture count changed without updating the contract check');

for (const name of fixtureNames) {
  const fixture = JSON.parse(fs.readFileSync(`${fixtureDir}/${name}`, 'utf8'));
  const pair = expected.get(name);
  assert(pair, `unexpected fixture ${name}`);
  assert(fixture.schema === 'rr.chronicle.v1', `${name} schema mismatch`);
  assert(fixture.event_type === pair[0], `${name} event type mismatch`);
  assert(fixture.subject?.kind === pair[1], `${name} subject kind mismatch`);
  assert(/^wide:evt:[A-Za-z0-9:_-]{8,120}$/.test(fixture.event_id), `${name} event id invalid`);
  assert(/^(daily|weekly|persistent):[A-Za-z0-9:_-]{1,80}$/.test(fixture.world_key), `${name} world key invalid`);
  assert(fixture.source === 'wide_world', `${name} source mismatch`);
  assert(fixture.visibility === 'public', `${name} visibility mismatch`);
  assert(/^wide:[A-Za-z0-9:_-]{3,120}$/.test(fixture.subject?.id ?? ''), `${name} subject id invalid`);
  assert(bounded(fixture.subject?.name, 2, 80), `${name} subject name invalid`);
  assert(Number.isFinite(Date.parse(fixture.occurred_at)), `${name} occurrence time invalid`);
  assert(!/[\u2013\u2014]/.test(JSON.stringify(fixture)), `${name} contains a forbidden dash`);
  checkPayload(fixture, name);
}

console.log(`Chronicle contract OK: ${fixtureNames.length} typed fixtures`);
