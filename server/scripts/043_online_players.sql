-- one row per online account refreshed by heartbeat a row past ninety seconds is stale and lets them back in

CREATE TABLE IF NOT EXISTS online_players (
    account_id   INT UNSIGNED NOT NULL PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL DEFAULT 0,
    game_session INT UNSIGNED NOT NULL DEFAULT 0,
    since        TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_seen    TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
