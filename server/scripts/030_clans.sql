-- no clan system exists in the client so the clan tag rides inside the display name instead

CREATE TABLE IF NOT EXISTS clans (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    tag         VARCHAR(3)   NOT NULL,
    name        VARCHAR(32)  NOT NULL,
    leader_id   INT          NOT NULL,
    notice      VARCHAR(255) NOT NULL DEFAULT '',
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_clans_tag  (tag),
    UNIQUE KEY uq_clans_name (name),
    KEY idx_clans_leader (leader_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- one clan per character dropping the clan clears the column not the row
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS clan_id INT NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS clan_rank TINYINT NOT NULL DEFAULT 0;

CREATE INDEX IF NOT EXISTS idx_characters_clan ON characters (clan_id);
