-- scenario raw wire for S2C 0xF3 0xF5 0xF8 0xF9 keyed against FUN 00452E30's cached menu table

CREATE TABLE IF NOT EXISTS scenario_def (
    scenario_key        INT UNSIGNED NOT NULL PRIMARY KEY,
    -- offset 0x10 and offset 0x14 of the 156 byte S2C 0x00F3 record catalog mapping unproven so these default to 0
    character_def_key   INT UNSIGNED NOT NULL DEFAULT 0,
    kart_def_key         INT UNSIGNED NOT NULL DEFAULT 0,
    -- offset 0x28 selects one of eight tail shapes on the kind 3 reward never used by this server
    reward_category      INT UNSIGNED NOT NULL DEFAULT 0,
    -- server side award policy for C2S 0x00F8 result reports not on the wire
    gold_reward           INT UNSIGNED NOT NULL DEFAULT 0,
    exp_reward             INT UNSIGNED NOT NULL DEFAULT 0,
    created_at             TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- no seed rows since C2S 0x00F5 is shared with quest ghost start an invented key could shadow a real quest

-- scenario key wire keyed the same way the client keys its own cache
CREATE TABLE IF NOT EXISTS scenario_key_progress (
    character_id   INT UNSIGNED NOT NULL,
    scenario_key   INT UNSIGNED NOT NULL,
    completed      TINYINT(1) NOT NULL DEFAULT 0,
    last_result    INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, scenario_key),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
