-- Retain only a keyed fingerprint of a redeemed HUMAN code until its original expiry.
-- Never retain an extra copy of the secret device code or resulting credentials.
CREATE TABLE IF NOT EXISTS consumed_link_code (
  code_fingerprint TEXT PRIMARY KEY,
  expires_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS consumed_link_expiry ON consumed_link_code(expires_at);
