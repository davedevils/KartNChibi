-- adds content missing from the 003 seed and the vehicle customization table

-- drivers 6 to 10 are cosmetic characters missing from the 003 seed
INSERT INTO drivers (id, name, display_name, category, required_level, is_default, price_gold, price_cash) VALUES
    (6,  'cosmo',     'Cosmo',     'premium', 1,  0, 8000, 0),
    (7,  'moriko',    'Moriko',    'premium', 1,  0, 8000, 0),
    (8,  'prince',    'Prince',    'premium', 5,  0, 15000, 0),
    (9,  'princess',  'Princess',  'premium', 5,  0, 15000, 0),
    (10, 'pumpkin',   'Pumpkin',   'premium', 1,  0, 0, 100),
    (11, 'witch',     'Witch',     'premium', 10, 0, 0, 150),
    (12, 'wolf',      'Wolf',      'premium', 10, 0, 0, 150),
    (13, 'monster',   'Monster',   'premium', 15, 0, 0, 200),
    (14, 'yuk',       'Yuk',       'premium', 15, 0, 0, 200)
ON DUPLICATE KEY UPDATE display_name = VALUES(display_name);

-- stores visual only garage parts per vehicle mirroring the FactoryCar folder categories
CREATE TABLE IF NOT EXISTS vehicle_customization (
    vehicle_id INT UNSIGNED NOT NULL PRIMARY KEY,
    body_color INT UNSIGNED DEFAULT 0,
    booster INT UNSIGNED DEFAULT 0,
    bumper INT UNSIGNED DEFAULT 0,
    chassis INT UNSIGNED DEFAULT 0,
    cover INT UNSIGNED DEFAULT 0,
    front_fender INT UNSIGNED DEFAULT 0,
    rear_fender INT UNSIGNED DEFAULT 0,
    tires INT UNSIGNED DEFAULT 0,
    wing INT UNSIGNED DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (vehicle_id) REFERENCES vehicles(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- race history keeps per race records for leaderboards and ghost playback
CREATE TABLE IF NOT EXISTS race_history (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    room_id INT UNSIGNED DEFAULT 0,
    map_id INT UNSIGNED DEFAULT 0,
    vehicle_id INT UNSIGNED DEFAULT 0,
    game_mode TINYINT UNSIGNED DEFAULT 0,
    finish_time_ms INT UNSIGNED DEFAULT 0,
    rank_in_race TINYINT UNSIGNED DEFAULT 0,
    player_count TINYINT UNSIGNED DEFAULT 0,
    gold_reward INT DEFAULT 0,
    xp_reward INT DEFAULT 0,
    finished_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_char (character_id),
    INDEX idx_map_time (map_id, finish_time_ms),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
