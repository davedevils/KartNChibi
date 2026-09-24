-- active sessions last seen column GameServer updates on every login a fresh database was missing it

ALTER TABLE active_sessions ADD COLUMN IF NOT EXISTS last_seen TIMESTAMP NULL DEFAULT NULL;
