CREATE TABLE IF NOT EXISTS project_device (
  session_id TEXT PRIMARY KEY REFERENCES session(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  label TEXT NOT NULL DEFAULT 'Hollowwake Xbox companion',
  created_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS project_device_user ON project_device(user_id);
CREATE TABLE IF NOT EXISTS request_limit (
  bucket TEXT PRIMARY KEY,
  started_at INTEGER NOT NULL,
  attempts INTEGER NOT NULL
);
