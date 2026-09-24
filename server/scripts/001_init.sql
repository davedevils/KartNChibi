
USE knc_emu;

CREATE TABLE IF NOT EXISTS accounts (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(32) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    is_banned TINYINT(1) DEFAULT 0,
    ban_reason VARCHAR(255),
    ban_expires_at TIMESTAMP NULL,
    INDEX idx_username (username),
    INDEX idx_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS characters (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    name VARCHAR(32) NOT NULL UNIQUE,
    level INT UNSIGNED DEFAULT 1,
    experience INT UNSIGNED DEFAULT 0,
    gold INT UNSIGNED DEFAULT 10000,
    cash INT UNSIGNED DEFAULT 0,
    wins INT UNSIGNED DEFAULT 0,
    losses INT UNSIGNED DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_played TIMESTAMP NULL,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    INDEX idx_account (account_id),
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS vehicles (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    vehicle_type_id INT UNSIGNED NOT NULL,
    durability INT UNSIGNED DEFAULT 100,
    max_durability INT UNSIGNED DEFAULT 100,
    stat_speed INT DEFAULT 0,
    stat_accel INT DEFAULT 0,
    stat_handling INT DEFAULT 0,
    stat_drift INT DEFAULT 0,
    stat_boost INT DEFAULT 0,
    stat_weight INT DEFAULT 0,
    stat_special INT DEFAULT 0,
    equipped TINYINT(1) DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_character (character_id),
    INDEX idx_equipped (character_id, equipped)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS items (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    item_type_id INT UNSIGNED NOT NULL,
    quantity INT UNSIGNED DEFAULT 1,
    slot INT DEFAULT -1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NULL,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_character (character_id),
    INDEX idx_slot (character_id, slot)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS accessories (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    accessory_type_id INT UNSIGNED NOT NULL,
    slot INT DEFAULT 0,
    bonus1 INT DEFAULT 0,
    bonus2 INT DEFAULT 0,
    bonus3 INT DEFAULT 0,
    equipped TINYINT(1) DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_character (character_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS bans (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED,
    ip_address VARCHAR(45),
    reason VARCHAR(255),
    banned_by VARCHAR(64),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NULL,
    is_permanent TINYINT(1) DEFAULT 0,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE SET NULL,
    INDEX idx_ip (ip_address),
    INDEX idx_account (account_id),
    INDEX idx_expires (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS game_logs (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED,
    event_type VARCHAR(32) NOT NULL,
    event_data JSON,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE SET NULL,
    INDEX idx_character (character_id),
    INDEX idx_event_type (event_type),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS active_sessions (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    character_id INT UNSIGNED NOT NULL,
    token VARCHAR(64) NOT NULL,
    client_ip VARCHAR(45) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_account (account_id),
    INDEX idx_ip (client_ip),
    INDEX idx_token (token),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- old sessions should be purged periodically by a cron job or the server

CREATE TABLE IF NOT EXISTS servers (
    id INT UNSIGNED PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    host VARCHAR(64) NOT NULL,
    port INT UNSIGNED NOT NULL,
    type VARCHAR(16) NOT NULL DEFAULT 'game',
    max_players INT UNSIGNED DEFAULT 200,
    current_players INT UNSIGNED DEFAULT 0,
    is_online TINYINT(1) DEFAULT 0,
    last_heartbeat TIMESTAMP NULL,
    INDEX idx_type_online (type, is_online)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO accounts (username, password_hash, email) VALUES 
    ('test', 'sha256:test', 'test@test.com')
ON DUPLICATE KEY UPDATE accounts.id = accounts.id;

INSERT INTO characters (account_id, name, gold, cash) 
SELECT id, 'TestPlayer', 50000, 1000 FROM accounts WHERE username = 'test'
ON DUPLICATE KEY UPDATE characters.id = characters.id;

-- Give test character a starter vehicle
INSERT INTO vehicles (character_id, vehicle_type_id, equipped)
SELECT id, 1, 1 FROM characters WHERE name = 'TestPlayer'
ON DUPLICATE KEY UPDATE vehicles.id = vehicles.id;

