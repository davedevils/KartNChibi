CREATE TABLE IF NOT EXISTS ghost_record (
    track_id    INT NOT NULL,
    char_id     INT UNSIGNED NOT NULL,
    name        VARCHAR(64) NOT NULL DEFAULT '',
    time_ms     INT NOT NULL DEFAULT 0,
    car_kind    INT UNSIGNED NOT NULL DEFAULT 3,
    frame_count INT UNSIGNED NOT NULL DEFAULT 0,
    char_block  VARBINARY(44) NULL,
    kart_block  VARBINARY(56) NULL,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (track_id, char_id),
    INDEX idx_track_time (track_id, time_ms)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ghost_replay_chunk (
    track_id    INT NOT NULL,
    char_id     INT UNSIGNED NOT NULL,
    chunk_index INT UNSIGNED NOT NULL,
    frame_count INT UNSIGNED NOT NULL,
    data        VARBINARY(4096) NOT NULL,
    PRIMARY KEY (track_id, char_id, chunk_index)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS quest_def (
    quest_index      INT UNSIGNED NOT NULL,
    theme_id         INT UNSIGNED NOT NULL,
    goal_or_reward_a INT UNSIGNED NOT NULL DEFAULT 0,
    goal_or_reward_b INT UNSIGNED NOT NULL DEFAULT 0,
    goal_count       INT UNSIGNED NOT NULL DEFAULT 0,
    str_key_title    VARCHAR(67) NOT NULL,
    is_enabled       TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (quest_index),
    INDEX idx_theme (theme_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS char_quest_state (
    char_id        INT UNSIGNED NOT NULL,
    quest_index    INT UNSIGNED NOT NULL,
    state          INT UNSIGNED NOT NULL DEFAULT 0,
    progress_count INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (char_id, quest_index),
    CONSTRAINT fk_char_quest_state_char FOREIGN KEY (char_id)
        REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO quest_def
    (quest_index, theme_id, goal_or_reward_a, goal_or_reward_b, goal_count, str_key_title) VALUES
    ( 0, 0, 1, 0, 1, 'Quest_01_TITLE'), ( 1, 0, 1, 0, 1, 'Quest_02_TITLE'),
    ( 2, 0, 1, 0, 1, 'Quest_03_TITLE'), ( 3, 0, 1, 0, 1, 'Quest_04_TITLE'),
    ( 4, 1, 1, 0, 1, 'Quest_05_TITLE'), ( 5, 1, 1, 0, 1, 'Quest_06_TITLE'),
    ( 6, 1, 1, 0, 1, 'Quest_07_TITLE'), ( 7, 1, 1, 0, 1, 'Quest_08_TITLE'),
    ( 8, 2, 1, 0, 1, 'Quest_09_TITLE'), ( 9, 2, 1, 0, 1, 'Quest_10_TITLE'),
    (10, 2, 1, 0, 1, 'Quest_11_TITLE'), (11, 2, 1, 0, 1, 'Quest_12_TITLE'),
    (12, 3, 1, 0, 1, 'Quest_13_TITLE'), (13, 3, 1, 0, 1, 'Quest_14_TITLE'),
    (14, 3, 1, 0, 1, 'Quest_15_TITLE'), (15, 3, 1, 0, 1, 'Quest_16_TITLE')
ON DUPLICATE KEY UPDATE theme_id = VALUES(theme_id), goal_count = VALUES(goal_count),
    str_key_title = VALUES(str_key_title);
