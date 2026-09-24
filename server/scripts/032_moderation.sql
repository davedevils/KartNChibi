-- bans already exist and are enforced this adds mutes and gm actions since the client has no admin screen

ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS muted_until TIMESTAMP NULL DEFAULT NULL;

CREATE INDEX IF NOT EXISTS idx_characters_muted ON characters (muted_until);

-- who did what to whom needed the first time a decision is questioned
CREATE TABLE IF NOT EXISTS gm_log (
    id         BIGINT AUTO_INCREMENT PRIMARY KEY,
    gm_id      INT          NOT NULL,
    gm_name    VARCHAR(32)  NOT NULL,
    action     VARCHAR(16)  NOT NULL,
    target     VARCHAR(32)  NOT NULL DEFAULT '',
    detail     VARCHAR(255) NOT NULL DEFAULT '',
    at         TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_gm_log_at (at),
    KEY idx_gm_log_gm (gm_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
