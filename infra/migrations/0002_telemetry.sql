CREATE TABLE IF NOT EXISTS telemetry_rollup (
  event TEXT NOT NULL,
  choice INTEGER NOT NULL,
  deck TEXT NOT NULL,
  day_band INTEGER NOT NULL,
  n INTEGER NOT NULL DEFAULT 0,
  score_total INTEGER NOT NULL DEFAULT 0,
  gap_total INTEGER NOT NULL DEFAULT 0,
  run_ends INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (event, choice, deck, day_band)
);

CREATE INDEX IF NOT EXISTS telemetry_event_n
  ON telemetry_rollup(event, n DESC);
