-- S2C 0x119 is one pendant def per frame 0x11A one owned pendant equipped key rides the profile blob offset 0x22

CREATE TABLE IF NOT EXISTS pendant_def (
    pendant_key INT UNSIGNED NOT NULL PRIMARY KEY,
    icon_base   VARCHAR(32) NOT NULL COMMENT 'cstr, client dest is 33 bytes',
    title_key   VARCHAR(32) NOT NULL COMMENT 'cstr, client dest is 33 bytes',
    info_key    VARCHAR(33) NOT NULL COMMENT 'cstr, client dest is 34 bytes',
    visible     TINYINT UNSIGNED NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO pendant_def (pendant_key, icon_base, title_key, info_key) VALUES
    ( 1, 'pendant_01', 'PENDANT_01_TITLE', 'PENDANT_01_INFO'),
    ( 2, 'pendant_02', 'PENDANT_02_TITLE', 'PENDANT_02_INFO'),
    ( 3, 'pendant_03', 'PENDANT_03_TITLE', 'PENDANT_03_INFO'),
    ( 4, 'pendant_04', 'PENDANT_04_TITLE', 'PENDANT_04_INFO'),
    ( 5, 'pendant_05', 'PENDANT_05_TITLE', 'PENDANT_05_INFO'),
    ( 6, 'pendant_06', 'PENDANT_06_TITLE', 'PENDANT_06_INFO'),
    ( 7, 'pendant_07', 'PENDANT_07_TITLE', 'PENDANT_07_INFO'),
    ( 8, 'pendant_08', 'PENDANT_08_TITLE', 'PENDANT_08_INFO'),
    ( 9, 'pendant_09', 'PENDANT_09_TITLE', 'PENDANT_09_INFO'),
    (10, 'pendant_10', 'PENDANT_10_TITLE', 'PENDANT_10_INFO'),
    (11, 'pendant_11', 'PENDANT_11_TITLE', 'PENDANT_11_INFO'),
    (12, 'pendant_12', 'PENDANT_12_TITLE', 'PENDANT_12_INFO')
ON DUPLICATE KEY UPDATE icon_base = VALUES(icon_base), title_key = VALUES(title_key), info_key = VALUES(info_key);

-- chibikart sends a thirteenth hidden pendant same three string pattern but visible 0
INSERT INTO pendant_def (pendant_key, icon_base, title_key, info_key, visible)
VALUES (13, 'pendant_13', 'PENDANT_13_TITLE', 'PENDANT_13_INFO', 0)
ON DUPLICATE KEY UPDATE visible = VALUES(visible);

CREATE TABLE IF NOT EXISTS owned_pendant (
    character_id INT UNSIGNED NOT NULL,
    pendant_key  INT UNSIGNED NOT NULL,
    granted_at   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, pendant_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- a clan picks one emblem and every member carries it while in the clan
ALTER TABLE clans ADD COLUMN IF NOT EXISTS pendant_key INT UNSIGNED NOT NULL DEFAULT 0;

-- the first emblem is everyone's so the pendant screen has something to equip
INSERT IGNORE INTO owned_pendant (character_id, pendant_key)
SELECT id, 1 FROM characters;

-- gift rows learn whether the inbox has shown them C2S 0x9C marks one read
ALTER TABLE gift_log ADD COLUMN IF NOT EXISTS read_flag TINYINT UNSIGNED NOT NULL DEFAULT 0;
