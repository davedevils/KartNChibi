-- the creation popup offers drivers whose row carries both head flags originally just the pumpkin and the princess idempotent

ALTER TABLE drivers ADD COLUMN IF NOT EXISTS creation_pick TINYINT(1) NOT NULL DEFAULT 0;
UPDATE drivers SET creation_pick = 1 WHERE name IN ('princess', 'pumpkin');
