
USE knc_emu;

ALTER TABLE characters 
    ADD COLUMN IF NOT EXISTS total_races INT UNSIGNED DEFAULT 0,
    ADD COLUMN IF NOT EXISTS playtime_minutes INT UNSIGNED DEFAULT 0,
    ADD COLUMN IF NOT EXISTS license_class INT UNSIGNED DEFAULT 0 COMMENT '0=Rookie, 1=Amateur, 2=Pro, 3=Master',
    ADD COLUMN IF NOT EXISTS rank_points INT UNSIGNED DEFAULT 0,
    ADD COLUMN IF NOT EXISTS equipped_driver_id INT UNSIGNED DEFAULT 1,
    ADD COLUMN IF NOT EXISTS tutorial_completed TINYINT(1) DEFAULT 0,
    ADD COLUMN IF NOT EXISTS is_gm TINYINT(1) DEFAULT 0;

-- renames stat armor to stat weight and stat item to stat special to match the reverse after checking if needed
SET @has_armor = (SELECT COUNT(*) FROM information_schema.COLUMNS 
                  WHERE TABLE_SCHEMA='knc_emu' AND TABLE_NAME='vehicles' AND COLUMN_NAME='stat_armor');

-- renames stat armor to stat weight when the old column still exists
SET @sql = IF(@has_armor > 0, 
    'ALTER TABLE vehicles CHANGE COLUMN stat_armor stat_weight INT DEFAULT 50',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_item = (SELECT COUNT(*) FROM information_schema.COLUMNS 
                 WHERE TABLE_SCHEMA='knc_emu' AND TABLE_NAME='vehicles' AND COLUMN_NAME='stat_item');

SET @sql = IF(@has_item > 0, 
    'ALTER TABLE vehicles CHANGE COLUMN stat_item stat_special INT DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- Set default values for vehicle stats if they're 0
UPDATE vehicles SET 
    stat_speed = 50 WHERE stat_speed = 0;
UPDATE vehicles SET 
    stat_accel = 50 WHERE stat_accel = 0;
UPDATE vehicles SET 
    stat_handling = 50 WHERE stat_handling = 0;
UPDATE vehicles SET 
    stat_drift = 40 WHERE stat_drift = 0;
UPDATE vehicles SET 
    stat_boost = 30 WHERE stat_boost = 0;

ALTER TABLE items 
    ADD COLUMN IF NOT EXISTS equipped TINYINT(1) DEFAULT 0;

-- renames expires at to expire at when it still exists for consistency
SET @has_expires = (SELECT COUNT(*) FROM information_schema.COLUMNS 
                    WHERE TABLE_SCHEMA='knc_emu' AND TABLE_NAME='items' AND COLUMN_NAME='expires_at');

-- adds expire at column when missing
ALTER TABLE items 
    ADD COLUMN IF NOT EXISTS expire_at TIMESTAMP NULL,
    ADD COLUMN IF NOT EXISTS enhancement_level INT UNSIGNED DEFAULT 0;

CREATE TABLE IF NOT EXISTS shop_items (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    template_id INT UNSIGNED NOT NULL COMMENT 'ID from client Define/*.txt',
    category ENUM('vehicle', 'item', 'accessory', 'special') NOT NULL,
    name VARCHAR(64) NOT NULL,
    price_gold INT UNSIGNED DEFAULT 0,
    price_cash INT UNSIGNED DEFAULT 0,
    required_level INT UNSIGNED DEFAULT 1,
    stock INT DEFAULT -1 COMMENT '-1 = unlimited',
    is_limited TINYINT(1) DEFAULT 0,
    discount_percent INT UNSIGNED DEFAULT 0,
    available_from TIMESTAMP NULL,
    available_until TIMESTAMP NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_template (template_id),
    INDEX idx_category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS purchase_log (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    shop_item_id INT UNSIGNED,
    item_template_id INT UNSIGNED NOT NULL,
    quantity INT UNSIGNED DEFAULT 1,
    total_gold INT UNSIGNED DEFAULT 0,
    total_cash INT UNSIGNED DEFAULT 0,
    ip_address VARCHAR(45),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_character (character_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO shop_items (template_id, category, name, price_gold, price_cash, required_level) VALUES
    -- starter vehicle template ids must match the client define files
    (1001, 'vehicle', 'Basic Kart', 0, 0, 1),
    (1002, 'vehicle', 'Speed Racer', 5000, 0, 5),
    (1003, 'vehicle', 'Drift King', 8000, 0, 10),
    (1004, 'vehicle', 'Turbo Special', 15000, 0, 15),
    (1005, 'vehicle', 'Premium Rider', 0, 100, 1),
    
    -- items are boosts shields and similar
    (2001, 'item', 'Speed Boost', 100, 0, 1),
    (2002, 'item', 'Shield', 150, 0, 1),
    (2003, 'item', 'Missile', 200, 0, 5),
    (2004, 'item', 'Banana', 50, 0, 1),
    (2005, 'item', 'Oil Slick', 75, 0, 3),
    
    -- Accessories
    (3001, 'accessory', 'Speed +5', 1000, 0, 5),
    (3002, 'accessory', 'Accel +5', 1000, 0, 5),
    (3003, 'accessory', 'Handling +5', 1000, 0, 5),
    (3004, 'accessory', 'Lucky Star', 0, 50, 1)
ON DUPLICATE KEY UPDATE name = VALUES(name);

UPDATE characters SET 
    wins = 10,
    losses = 5,
    total_races = 15,
    playtime_minutes = 120,
    license_class = 1,
    rank_points = 500,
    tutorial_completed = 1
WHERE name = 'TestPlayer';

-- Update test vehicle with proper stats
UPDATE vehicles v
JOIN characters c ON v.character_id = c.id
SET 
    v.stat_speed = 55,
    v.stat_accel = 50,
    v.stat_handling = 60,
    v.stat_drift = 45,
    v.stat_boost = 40,
    v.stat_weight = 50,
    v.stat_special = 0
WHERE c.name = 'TestPlayer' AND v.equipped = 1;

-- Give test player some items
INSERT INTO items (character_id, item_type_id, quantity, slot, equipped)
SELECT c.id, 2001, 5, 0, 0 FROM characters c WHERE c.name = 'TestPlayer'
ON DUPLICATE KEY UPDATE quantity = 5;

INSERT INTO items (character_id, item_type_id, quantity, slot, equipped)
SELECT c.id, 2002, 3, 1, 0 FROM characters c WHERE c.name = 'TestPlayer'
ON DUPLICATE KEY UPDATE quantity = 3;

-- Give test player an accessory
INSERT INTO accessories (character_id, accessory_type_id, slot, bonus1, bonus2, bonus3, equipped)
SELECT c.id, 3001, 0, 5, 0, 0, 1 FROM characters c WHERE c.name = 'TestPlayer'
ON DUPLICATE KEY UPDATE equipped = 1;

SELECT 'Migration v2 complete!' AS Status;

