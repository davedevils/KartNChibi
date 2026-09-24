-- 067 the client has no guild so the invented clan table and its two character columns go

-- apply only with the image of 2026-09-23 or later an older game server still joins the clans table at login
ALTER TABLE characters DROP INDEX IF EXISTS idx_characters_clan;
ALTER TABLE characters DROP COLUMN IF EXISTS clan_id, DROP COLUMN IF EXISTS clan_rank;
DROP TABLE IF EXISTS clans;
