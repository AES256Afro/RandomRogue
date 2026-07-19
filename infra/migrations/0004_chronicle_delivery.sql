CREATE TABLE IF NOT EXISTS chronicle_outbox (
  event_id TEXT PRIMARY KEY,
  status TEXT NOT NULL DEFAULT 'queued'
    CHECK (status IN ('queued', 'delivering', 'retrying', 'delivered', 'dead')),
  attempt_count INTEGER NOT NULL DEFAULT 0,
  next_attempt_at INTEGER NOT NULL,
  locked_at INTEGER,
  delivered_at INTEGER,
  last_http_status INTEGER,
  last_error TEXT,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  FOREIGN KEY (event_id) REFERENCES chronicle_events(event_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS chronicle_outbox_due
  ON chronicle_outbox(status, next_attempt_at);

CREATE TABLE IF NOT EXISTS chronicle_delivery_attempts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  event_id TEXT NOT NULL,
  attempted_at INTEGER NOT NULL,
  outcome TEXT NOT NULL,
  http_status INTEGER,
  error TEXT,
  duration_ms INTEGER NOT NULL,
  FOREIGN KEY (event_id) REFERENCES chronicle_events(event_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS chronicle_attempts_event_time
  ON chronicle_delivery_attempts(event_id, attempted_at DESC);

CREATE TABLE IF NOT EXISTS chronicle_dead_letters (
  event_id TEXT PRIMARY KEY,
  reason TEXT NOT NULL,
  raw_json TEXT NOT NULL,
  failed_at INTEGER NOT NULL,
  attempts INTEGER NOT NULL,
  FOREIGN KEY (event_id) REFERENCES chronicle_events(event_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS chronicle_delivery_policy (
  event_type TEXT PRIMARY KEY,
  enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
  updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS chronicle_daily_budget (
  day INTEGER PRIMARY KEY,
  accepted INTEGER NOT NULL DEFAULT 0,
  updated_at INTEGER NOT NULL
);

INSERT OR IGNORE INTO chronicle_delivery_policy(event_type, enabled, updated_at) VALUES
  ('wide_world.death', 1, unixepoch('now') * 1000),
  ('wide_world.deed', 1, unixepoch('now') * 1000),
  ('wide_world.ending', 1, unixepoch('now') * 1000),
  ('wide_world.institution_changed', 1, unixepoch('now') * 1000),
  ('wide_world.region_changed', 1, unixepoch('now') * 1000),
  ('wide_world.artifact_legacy', 1, unixepoch('now') * 1000);

INSERT OR IGNORE INTO chronicle_outbox(
  event_id, status, attempt_count, next_attempt_at, created_at, updated_at
)
SELECT event_id, 'queued', 0, unixepoch('now') * 1000,
       unixepoch('now') * 1000, unixepoch('now') * 1000
FROM chronicle_events;
