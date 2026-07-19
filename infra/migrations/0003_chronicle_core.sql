CREATE TABLE IF NOT EXISTS chronicle_events (
  event_id TEXT PRIMARY KEY,
  death_id INTEGER UNIQUE,
  schema_name TEXT NOT NULL,
  event_type TEXT NOT NULL,
  source TEXT NOT NULL,
  world_key TEXT NOT NULL,
  occurred_at TEXT NOT NULL,
  subject_json TEXT NOT NULL,
  place_json TEXT NOT NULL DEFAULT '{}',
  tags_json TEXT NOT NULL DEFAULT '[]',
  effects_json TEXT NOT NULL DEFAULT '{}',
  payload_json TEXT NOT NULL DEFAULT '{}',
  visibility TEXT NOT NULL,
  raw_json TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (death_id) REFERENCES deaths(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS chronicle_events_type_time
  ON chronicle_events(event_type, created_at DESC);
CREATE INDEX IF NOT EXISTS chronicle_events_world_time
  ON chronicle_events(world_key, created_at DESC);
