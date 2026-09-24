
USE knc_emu;

CREATE TABLE IF NOT EXISTS maps (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    display_name VARCHAR(64) NOT NULL,
    category ENUM('normal', 'item', 'tutorial', 'scenario', 'bonus') DEFAULT 'normal',
    difficulty INT UNSIGNED DEFAULT 1 COMMENT '1-5 stars',
    min_players INT UNSIGNED DEFAULT 2,
    max_players INT UNSIGNED DEFAULT 8,
    lap_options VARCHAR(32) DEFAULT '3,5,7' COMMENT 'Comma-separated lap counts',
    weather_options VARCHAR(32) DEFAULT '0,1,2' COMMENT '0=Sun, 1=Night, 2=Rain',
    is_enabled TINYINT(1) DEFAULT 1,
    required_level INT UNSIGNED DEFAULT 1,
    unlock_price_gold INT UNSIGNED DEFAULT 0,
    INDEX idx_category (category),
    INDEX idx_enabled (is_enabled)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- default map ids must match the client
INSERT INTO maps (id, name, display_name, category, difficulty, min_players, max_players) VALUES
    (1, 'city_circuit', 'City Circuit', 'normal', 1, 2, 8),
    (2, 'forest_run', 'Forest Run', 'normal', 2, 2, 8),
    (3, 'beach_paradise', 'Beach Paradise', 'normal', 2, 2, 8),
    (4, 'mountain_pass', 'Mountain Pass', 'normal', 3, 2, 8),
    (5, 'desert_storm', 'Desert Storm', 'normal', 3, 2, 8),
    (6, 'ice_palace', 'Ice Palace', 'normal', 4, 2, 8),
    (7, 'volcano_fury', 'Volcano Fury', 'normal', 4, 2, 8),
    (8, 'space_station', 'Space Station', 'normal', 5, 2, 8),
    (11, 'tutorial_basic', 'Tutorial: Basic Controls', 'tutorial', 1, 1, 1),
    (12, 'tutorial_drift', 'Tutorial: Drifting', 'tutorial', 1, 1, 1),
    (13, 'tutorial_items', 'Tutorial: Items', 'tutorial', 1, 1, 1)
ON DUPLICATE KEY UPDATE display_name = VALUES(display_name);

CREATE TABLE IF NOT EXISTS missions (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    category ENUM('daily', 'weekly', 'story', 'achievement') DEFAULT 'daily',
    mission_type ENUM('race', 'win', 'item', 'collect', 'time', 'tutorial') NOT NULL,
    target_count INT UNSIGNED DEFAULT 1,
    map_id INT UNSIGNED NULL COMMENT 'Specific map required, NULL = any',
    reward_gold INT UNSIGNED DEFAULT 0,
    reward_cash INT UNSIGNED DEFAULT 0,
    reward_xp INT UNSIGNED DEFAULT 0,
    reward_item_id INT UNSIGNED NULL,
    is_repeatable TINYINT(1) DEFAULT 0,
    required_level INT UNSIGNED DEFAULT 1,
    is_enabled TINYINT(1) DEFAULT 1,
    INDEX idx_category (category),
    INDEX idx_enabled (is_enabled)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Insert default missions
INSERT INTO missions (id, name, description, category, mission_type, target_count, reward_gold, reward_xp) VALUES
    (1, 'First Race', 'Complete your first race', 'story', 'race', 1, 500, 100),
    (2, 'Beginner Winner', 'Win 3 races', 'story', 'win', 3, 1000, 200),
    (3, 'Speed Demon', 'Finish a race in under 2 minutes', 'achievement', 'time', 1, 2000, 500),
    (4, 'Item Master', 'Use 10 items in races', 'story', 'item', 10, 500, 150),
    (5, 'Daily Race', 'Complete 3 races today', 'daily', 'race', 3, 300, 50),
    (6, 'Daily Win', 'Win 1 race today', 'daily', 'win', 1, 500, 100),
    (7, 'Weekly Champion', 'Win 10 races this week', 'weekly', 'win', 10, 3000, 1000)
ON DUPLICATE KEY UPDATE name = VALUES(name);

CREATE TABLE IF NOT EXISTS mission_progress (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    mission_id INT UNSIGNED NOT NULL,
    current_progress INT UNSIGNED DEFAULT 0,
    is_completed TINYINT(1) DEFAULT 0,
    completed_at TIMESTAMP NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_char_mission (character_id, mission_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (mission_id) REFERENCES missions(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS license_progress (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    license_level INT UNSIGNED DEFAULT 0 COMMENT '0=None, 1=D, 2=C, 3=B, 4=A, 5=S',
    tutorial_1_completed TINYINT(1) DEFAULT 0,
    tutorial_2_completed TINYINT(1) DEFAULT 0,
    tutorial_3_completed TINYINT(1) DEFAULT 0,
    best_time_tutorial_1 INT UNSIGNED NULL COMMENT 'Best time in ms',
    best_time_tutorial_2 INT UNSIGNED NULL,
    best_time_tutorial_3 INT UNSIGNED NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_character (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS drivers (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(32) NOT NULL,
    display_name VARCHAR(64) NOT NULL,
    category ENUM('default', 'premium', 'event', 'special') DEFAULT 'default',
    price_gold INT UNSIGNED DEFAULT 0,
    price_cash INT UNSIGNED DEFAULT 0,
    required_level INT UNSIGNED DEFAULT 1,
    is_default TINYINT(1) DEFAULT 0 COMMENT 'Given to all players',
    stat_bonus_speed INT DEFAULT 0,
    stat_bonus_accel INT DEFAULT 0,
    stat_bonus_handling INT DEFAULT 0,
    is_enabled TINYINT(1) DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Insert default drivers
INSERT INTO drivers (id, name, display_name, category, is_default, price_gold) VALUES
    (1, 'racer_basic', 'Rookie Racer', 'default', 1, 0),
    (2, 'racer_pro', 'Pro Driver', 'default', 0, 5000),
    (3, 'racer_speed', 'Speed Star', 'premium', 0, 0),
    (4, 'racer_drift', 'Drift Master', 'premium', 0, 0),
    (5, 'racer_mummy', 'Mummy', 'special', 0, 10000)
ON DUPLICATE KEY UPDATE display_name = VALUES(display_name);

CREATE TABLE IF NOT EXISTS player_drivers (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    driver_id INT UNSIGNED NOT NULL,
    is_equipped TINYINT(1) DEFAULT 0,
    obtained_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_char_driver (character_id, driver_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (driver_id) REFERENCES drivers(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Give default drivers to all existing players
INSERT INTO player_drivers (character_id, driver_id, is_equipped)
SELECT c.id, d.id, (d.id = 1) 
FROM characters c
CROSS JOIN drivers d
WHERE d.is_default = 1
ON DUPLICATE KEY UPDATE is_equipped = is_equipped;

CREATE TABLE IF NOT EXISTS pets (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(32) NOT NULL,
    display_name VARCHAR(64) NOT NULL,
    rarity ENUM('common', 'rare', 'epic', 'legendary') DEFAULT 'common',
    price_gold INT UNSIGNED DEFAULT 0,
    price_cash INT UNSIGNED DEFAULT 0,
    effect_type ENUM('speed', 'item', 'defense', 'luck') DEFAULT 'luck',
    effect_value INT DEFAULT 0,
    is_enabled TINYINT(1) DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Insert default pets
INSERT INTO pets (id, name, display_name, rarity, price_gold, effect_type, effect_value) VALUES
    (1, 'cat_basic', 'Lucky Cat', 'common', 2000, 'luck', 5),
    (2, 'dog_speed', 'Speed Pup', 'common', 3000, 'speed', 3),
    (3, 'dragon_rare', 'Baby Dragon', 'rare', 10000, 'defense', 10),
    (4, 'phoenix_epic', 'Phoenix', 'epic', 0, 'speed', 8)
ON DUPLICATE KEY UPDATE display_name = VALUES(display_name);

CREATE TABLE IF NOT EXISTS player_pets (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    pet_id INT UNSIGNED NOT NULL,
    level INT UNSIGNED DEFAULT 1,
    experience INT UNSIGNED DEFAULT 0,
    is_equipped TINYINT(1) DEFAULT 0,
    obtained_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_char_pet (character_id, pet_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (pet_id) REFERENCES pets(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS anticheat_logs (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED,
    account_id INT UNSIGNED,
    ip_address VARCHAR(45) NOT NULL,
    violation_type ENUM('speedhack', 'teleport', 'item_exploit', 'packet_flood', 'invalid_data', 'memory_edit') NOT NULL,
    severity ENUM('low', 'medium', 'high', 'critical') DEFAULT 'medium',
    details TEXT,
    action_taken ENUM('none', 'kick', 'temp_ban', 'perm_ban') DEFAULT 'none',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_character (character_id),
    INDEX idx_ip (ip_address),
    INDEX idx_violation (violation_type),
    INDEX idx_severity (severity),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS speed_records (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    map_id INT UNSIGNED NOT NULL,
    lap_time INT UNSIGNED NOT NULL COMMENT 'Time in ms',
    total_time INT UNSIGNED NOT NULL COMMENT 'Total race time in ms',
    max_speed FLOAT NOT NULL COMMENT 'Maximum speed recorded',
    avg_speed FLOAT NOT NULL COMMENT 'Average speed',
    is_valid TINYINT(1) DEFAULT 1 COMMENT 'Set to 0 if flagged by anti-cheat',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_character (character_id),
    INDEX idx_map (map_id),
    INDEX idx_time (total_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS item_templates (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    category ENUM('vehicle', 'item', 'accessory', 'driver', 'pet', 'consumable') NOT NULL,
    description TEXT,
    rarity ENUM('common', 'uncommon', 'rare', 'epic', 'legendary') DEFAULT 'common',
    base_price INT UNSIGNED DEFAULT 0,
    is_tradeable TINYINT(1) DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Default item templates
INSERT IGNORE INTO item_templates (id, name, category, rarity, base_price) VALUES
-- Vehicles
(1001, 'Starter Kart', 'vehicle', 'common', 0),
(1002, 'Speed Racer', 'vehicle', 'uncommon', 5000),
(1003, 'Turbo Machine', 'vehicle', 'rare', 15000),
(1004, 'Lightning', 'vehicle', 'epic', 50000),
(1005, 'Golden Dragon', 'vehicle', 'legendary', 200000),
-- items are boosts
(2001, 'Speed Boost', 'item', 'common', 100),
(2002, 'Shield', 'item', 'common', 150),
(2003, 'Rocket', 'item', 'uncommon', 300),
(2004, 'Mine', 'item', 'uncommon', 250),
(2005, 'Magnet', 'item', 'rare', 500),
-- Accessories
(3001, 'Racing Helmet', 'accessory', 'common', 200),
(3002, 'Cool Glasses', 'accessory', 'uncommon', 500),
(3003, 'Fire Wings', 'accessory', 'rare', 2000),
(3004, 'Angel Halo', 'accessory', 'epic', 10000),
-- Drivers
(4001, 'Default Driver', 'driver', 'common', 0),
(4002, 'Ninja', 'driver', 'uncommon', 3000),
(4003, 'Robot', 'driver', 'rare', 8000),
(4004, 'Alien', 'driver', 'epic', 25000);

CREATE TABLE IF NOT EXISTS gacha_banners (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    cost_gold INT UNSIGNED DEFAULT 0,
    cost_cash INT UNSIGNED DEFAULT 100,
    is_active TINYINT(1) DEFAULT 1,
    start_date TIMESTAMP NULL,
    end_date TIMESTAMP NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS gacha_items (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    gacha_id INT UNSIGNED NOT NULL,
    template_id INT UNSIGNED NOT NULL,
    drop_rate FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Percentage chance',
    rarity ENUM('common', 'uncommon', 'rare', 'epic', 'legendary') DEFAULT 'common',
    FOREIGN KEY (gacha_id) REFERENCES gacha_banners(id) ON DELETE CASCADE,
    FOREIGN KEY (template_id) REFERENCES item_templates(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS gacha_history (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    gacha_id INT UNSIGNED NOT NULL,
    item_template_id INT UNSIGNED NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_character (character_id),
    INDEX idx_gacha (gacha_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Default gacha banners
INSERT IGNORE INTO gacha_banners (id, name, description, cost_gold, cost_cash) VALUES
(1, 'Standard Gacha', 'Contains common and uncommon items', 1000, 50),
(2, 'Premium Gacha', 'Higher chance for rare items!', 0, 150),
(3, 'Vehicle Gacha', 'Exclusive vehicle collection', 0, 300);

-- Standard gacha items
INSERT IGNORE INTO gacha_items (gacha_id, template_id, drop_rate, rarity) VALUES
(1, 2001, 25.0, 'common'),
(1, 2002, 25.0, 'common'),
(1, 3001, 20.0, 'common'),
(1, 2003, 15.0, 'uncommon'),
(1, 3002, 10.0, 'uncommon'),
(1, 2005, 5.0, 'rare');

CREATE TABLE IF NOT EXISTS transaction_logs (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    type ENUM('ADD', 'REMOVE', 'PURCHASE', 'SELL', 'TRADE', 'REWARD') NOT NULL,
    amount INT NOT NULL,
    currency ENUM('gold', 'cash') NOT NULL,
    reason VARCHAR(255),
    admin_id INT UNSIGNED DEFAULT NULL COMMENT 'If modified by admin',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_character (character_id),
    INDEX idx_type (type),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE accounts ADD COLUMN IF NOT EXISTS gm_level TINYINT UNSIGNED DEFAULT 0;

CREATE TABLE IF NOT EXISTS game_logs (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED DEFAULT 0,
    event_type VARCHAR(50) NOT NULL,
    event_data TEXT,
    ip_address VARCHAR(45),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_character (character_id),
    INDEX idx_event_type (event_type),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS trade_history (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    initiator_char_id INT UNSIGNED NOT NULL,
    target_char_id INT UNSIGNED NOT NULL,
    initiator_gold INT UNSIGNED DEFAULT 0,
    target_gold INT UNSIGNED DEFAULT 0,
    initiator_items JSON,
    target_items JSON,
    status ENUM('completed', 'cancelled', 'expired') DEFAULT 'completed',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_initiator (initiator_char_id),
    INDEX idx_target (target_char_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS scenario_stages (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    chapter TINYINT UNSIGNED NOT NULL,
    stage TINYINT UNSIGNED NOT NULL,
    map_id INT UNSIGNED NOT NULL,
    difficulty TINYINT UNSIGNED NOT NULL DEFAULT 1,
    required_stars INT UNSIGNED NOT NULL DEFAULT 0,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    UNIQUE KEY uk_chapter_stage (chapter, stage)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS scenario_progress (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    total_stars INT UNSIGNED NOT NULL DEFAULT 0,
    completed TINYINT(1) DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_character (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS scenario_stage_progress (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    stage_id INT UNSIGNED NOT NULL,
    stars TINYINT UNSIGNED NOT NULL DEFAULT 0,
    best_time INT UNSIGNED NOT NULL DEFAULT 0,
    completed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_char_stage (character_id, stage_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (stage_id) REFERENCES scenario_stages(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- default scenario stages for chapter 1
INSERT IGNORE INTO scenario_stages (chapter, stage, map_id, difficulty, required_stars, name, description) VALUES
(1, 1, 1, 1, 0, 'First Steps', 'Learn the basics of racing'),
(1, 2, 2, 1, 0, 'Speed Demon', 'Reach the finish line first'),
(1, 3, 3, 2, 3, 'Drift Master', 'Master the art of drifting'),
(1, 4, 4, 2, 6, 'Item Frenzy', 'Use items to win'),
(1, 5, 5, 3, 9, 'Chapter Boss', 'Defeat the chapter boss'),
-- Chapter 2
(2, 1, 6, 2, 15, 'Desert Storm', 'Race through the desert'),
(2, 2, 7, 2, 18, 'Oasis Rush', 'Find the oasis'),
(2, 3, 8, 3, 21, 'Sandstorm', 'Survive the sandstorm'),
(2, 4, 9, 3, 24, 'Pyramid Race', 'Race around the pyramids'),
(2, 5, 10, 4, 27, 'Desert King', 'Defeat the desert king'),
-- Chapter 3
(3, 1, 11, 3, 30, 'Ice Track', 'Race on ice'),
(3, 2, 12, 3, 33, 'Frozen Lake', 'Cross the frozen lake'),
(3, 3, 13, 4, 36, 'Blizzard', 'Race through the blizzard'),
(3, 4, 14, 4, 39, 'Glacier', 'Climb the glacier'),
(3, 5, 15, 5, 42, 'Ice Queen', 'Defeat the ice queen');

CREATE TABLE IF NOT EXISTS ghost_records (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    map_id INT UNSIGNED NOT NULL,
    time INT UNSIGNED NOT NULL COMMENT 'Time in milliseconds',
    vehicle_id INT UNSIGNED NOT NULL,
    driver_id INT UNSIGNED NOT NULL,
    replay_data MEDIUMBLOB COMMENT 'Replay data for ghost (max 16MB)',
    is_valid TINYINT(1) DEFAULT 1 COMMENT 'Set to 0 if flagged by anti-cheat',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_map_time (map_id, time),
    INDEX idx_character (character_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friends (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    friend_id INT UNSIGNED NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_friendship (character_id, friend_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS blocked_players (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    blocked_id INT UNSIGNED NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_block (character_id, blocked_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (blocked_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

SELECT 'Migration v3: Game Systems complete!' AS Status;

