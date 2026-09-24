
CREATE TABLE IF NOT EXISTS progression_grant (
    id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    exp_delta    INT NOT NULL DEFAULT 0,
    gold_delta   INT NOT NULL DEFAULT 0,
    astro_delta  INT NOT NULL DEFAULT 0 COMMENT 'characters.cash, the premium wallet',
    source       VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'free text, who queued it',
    state        ENUM('pending','claimed','applied') NOT NULL DEFAULT 'pending'
                 COMMENT 'claimed is the crash window, it is never paid twice',
    created_at   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    applied_at   TIMESTAMP NULL DEFAULT NULL,
    KEY idx_progression_grant_drain (character_id, state, id),
    CONSTRAINT fk_progression_grant_char FOREIGN KEY (character_id)
        REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE level_curve COMMENT =
  'SYNTHETIC thresholds, shape 100n^2+400n, cap 55. The client owns NO ladder: exp_floor 0x01A20B24 has 1 xref and exp_next 0x01A20B28 has 2, all READ, all inside FUN_00429990. The only define files KnC.exe ever opens are def_emotion def_quest_message def_quest_index def_taboo def_title def_trans_message def_trans_index, none carries exp. Only the 55 row count is shipped backed, Data/Public/Image/Icon/Lv_icon_001..055.png. Replace after a retail capture.';

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 0*1000000 + d.id, 0, 0,
       IF(d.price_cash > 0, d.price_cash, d.price_gold), 0,
       IF(d.price_cash > 0, 1, 0)
FROM drivers d WHERE d.is_enabled = 1;

INSERT IGNORE INTO shop_definition
    (category, base_key, shop_visible_flag, badge, subtype, level_req,
     unlock_condition_key, str1_name, str2, str3_desc)
SELECT 0, d.id, 1, 0, 0, d.required_level, 0,
       LEFT(d.name, 32), LEFT(d.display_name, 32), LEFT(d.display_name, 33)
FROM drivers d WHERE d.is_enabled = 1;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 0, d.id, 0, 0*1000000 + d.id FROM drivers d WHERE d.is_enabled = 1;

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 1*1000000 + v.id, 0, 0,
       IF(v.price_cash > 0, v.price_cash, v.price_gold), 0,
       IF(v.price_cash > 0, 1, 0)
FROM vehicle_templates v WHERE v.is_enabled = 1;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 1, v.id, 0, 1*1000000 + v.id FROM vehicle_templates v WHERE v.is_enabled = 1;

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 2*1000000 + t.id, 0, 0, t.base_price, 0, 0
FROM item_templates t WHERE t.category = 'item';

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 2, t.id, 0, 2*1000000 + t.id FROM item_templates t WHERE t.category = 'item';

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 4*1000000 + p.id, 0, 0,
       IF(p.price_cash > 0, p.price_cash, p.price_gold), 0,
       IF(p.price_cash > 0, 1, 0)
FROM pets p WHERE p.is_enabled = 1;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 4, p.id, 0, 4*1000000 + p.id FROM pets p WHERE p.is_enabled = 1;

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT DISTINCT p.currency_key, p.period_type, p.period_value, 0, 0, 0
FROM room_object_price p WHERE p.currency_key > 0;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 5, p.object_key, p.slot_index, p.currency_key
FROM room_object_price p WHERE p.currency_key > 0 AND p.slot_index < 4;

INSERT IGNORE INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT DISTINCT c.price_table_key, c.period_type, c.period_value, 0, 0, 0
FROM carcraft_part_price c WHERE c.price_table_key > 0;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 6, c.part_key, c.row_index, c.price_table_key
FROM carcraft_part_price c WHERE c.price_table_key > 0 AND c.row_index < 4;

INSERT IGNORE INTO banned_words (word, scope) VALUES ('anal','both'), ('anus','both'), ('fist-fucking','both'), ('fistfucking','both'), ('fuck','both'), ('fucker','both'), ('fucking','both'), ('fudgepacking','both'), ('fuhrer','both'), ('hitler','both'), ('Ku_Klux_Klan','both'), ('Ku-Klux-Klan','both'), ('KKK','both'), ('nazi','both'), ('nazis','both'), ('peepshow','both'), ('peepshows','both'), ('peep-show','both'), ('peep-shows','both'), ('porn','both'), ('porno','both'), ('pr2n','both'), ('sex','both'), ('warez','both'), ('3l337','both'), ('3l33t','both'), ('amphetamine','both'), ('arse','both'), ('ass','both'), ('aurelianus','both'), ('biatch','both'), ('bitch','both'), ('blair','both'), ('blow','both'), ('boobs','both'), ('breasts','both'), ('bugger','both'), ('butt','both'), ('caca','both'), ('clit','both'), ('cocain','both'), ('cocaine','both'), ('cock','both'), ('condom','both'), ('crap','both'), ('cum','both'), ('cunniling','both'), ('cunnilingus','both'), ('cunt','both'), ('damn','both'), ('dildo','both'), ('douche','both'), ('dyke','both'), ('excrement','both'), ('fag','both'), ('fart','both'), ('fascist','both'), ('felch','both'), ('fellatio','both'), ('fetisch','both'), ('fetish','both'), ('frigg','both'), ('ganja','both'), ('genital','both'), ('gonad','both'), ('h4xx0r','both'), ('hasch','both'), ('hash','both'), ('haxx0r','both'), ('haxxor','both'), ('heroin','both'), ('hooker','both'), ('horny','both'), ('hurle','both'), ('jesus','both'), ('JesusChrist','both'), ('junkie','both'), ('klit','both'), ('kock','both'), ('kum','both'), ('kunniling','both'), ('kunt','both'), ('l337','both'), ('l33t','both'), ('marijuana','both'), ('masturba','both'), ('negro','both'), ('nigger','both'), ('nonce','both'), ('nude','both'), ('nudity','both'), ('oral','both'), ('orgasm','both'), ('pedophil','both'), ('penis','both'), ('phag','both'), ('phart','both'), ('phuck','both'), ('phuque','both'), ('piss','both'), ('ponce','both'), ('pr0n','both'), ('puke','both'), ('punnani','both'), ('puss','both'), ('putain','both'), ('queer','both'), ('rape','both'), ('ringhorn','both'), ('satan','both'), ('scat','both'), ('semen','both'), ('shit','both'), ('shlong','both'), ('slut','both'), ('spanking','both'), ('sperm','both'), ('sphincter','both'), ('suck','both'), ('suXX0r','both'), ('suXXor','both'), ('terd','both'), ('testic','both'), ('titties','both'), ('turd','both'), ('vagina','both'), ('viagra','both'), ('vomit','both'), ('vulva','both'), ('whore','both');

CREATE TABLE IF NOT EXISTS ghost_quest_replay (
    quest_index INT UNSIGNED NOT NULL,
    track_id    INT NOT NULL,
    char_id     INT UNSIGNED NOT NULL DEFAULT 0,
    is_enabled  TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (quest_index),
    INDEX idx_track (track_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO ghost_quest_replay (quest_index, track_id, char_id) VALUES
    (0, 90, 0), (1, 90, 0), (2, 91, 0), (3, 91, 0)
ON DUPLICATE KEY UPDATE track_id = VALUES(track_id), char_id = VALUES(char_id);

CREATE TABLE IF NOT EXISTS theme_catalog (
    theme_id       INT UNSIGNED NOT NULL COMMENT 'S2C 0xC4 +0x04, lookup key of sub_452FB0',
    theme_folder   VARCHAR(32)  NOT NULL COMMENT 'S2C 0xC4 +0x08 char[33], first World path part',
    display_name   VARCHAR(34)  NOT NULL DEFAULT '' COMMENT 'S2C 0xC4 +0x29 char[35], LOCALISATION KEY not text',
    record_field_0 INT NOT NULL DEFAULT 1 COMMENT 'S2C 0xC4 +0x00 enabled flag, the ghost strip skips 0',
    PRIMARY KEY (theme_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO theme_catalog (theme_id, theme_folder, display_name, record_field_0) VALUES
    (10,       'Forest',  'THEME_FOREST_INFO', 1),
    (20,       'Cookie',  'THEME_COOKIE_INFO', 1),
    (30,       'Desert',  'THEME_DESERT_INFO', 1),
    (40,       'Toy',     'THEME_TOY_INFO',    1),
    (50,       'Devil',   'THEME_DEVIL_INFO',  1),
    (60,       'Snow',    'THEME_SNOW_INFO',   1),
    (70,       'Palace',  'THEME_PALACE_INFO', 1),
    (80,       'Swamp',   'THEME_SWAMP_INFO',  1),
    (90,       'Race',    'THEME_RACE_INFO',   1),
    (10000000, 'Random',  'THEME_RANDOM_INFO', 1),
    (20000000, 'Mission', '',                  0),
    -- 20000000 rally and 30000000 battle are dev era themes with no world folder so rows are removed on purpose
    (0, '', '', 0)
ON DUPLICATE KEY UPDATE
    theme_folder   = VALUES(theme_folder),
    display_name   = VALUES(display_name),
    record_field_0 = VALUES(record_field_0);

CREATE TABLE IF NOT EXISTS track_catalog (
    track_id            INT UNSIGNED NOT NULL,
    map_id              INT UNSIGNED NULL COMMENT 'maps.id, the key track_spawn rows use',
    theme_id            INT UNSIGNED NOT NULL,
    folder_name         VARCHAR(35) NOT NULL,
    tail_string         VARCHAR(35) NOT NULL DEFAULT '',
    record_field_0      INT   NOT NULL DEFAULT 1,
    light_fog_0         FLOAT NOT NULL DEFAULT 0.4,
    light_fog_1         FLOAT NOT NULL DEFAULT 0.6,
    light_fog_2         FLOAT NOT NULL DEFAULT 90,
    unknown_60          INT   NOT NULL DEFAULT 0,
    fall_off_timeout_ms INT   NOT NULL DEFAULT 500,
    unknown_68          INT   NOT NULL DEFAULT 0,
    required_level      INT   NOT NULL DEFAULT 0,
    unknown_76          INT   NOT NULL DEFAULT 0,
    lap_count           INT   NOT NULL DEFAULT 3,
    unknown_84          INT   NOT NULL DEFAULT 0,
    unknown_88          INT   NOT NULL DEFAULT 0,
    unknown_92          INT   NOT NULL DEFAULT 0,
    unknown_96          INT   NOT NULL DEFAULT 0,
    unknown_100         INT   NOT NULL DEFAULT 0,
    checkpoint_count    INT   NOT NULL DEFAULT 0 COMMENT 'countA of track.COL, server side only',
    start_row_count     INT   NOT NULL DEFAULT 0 COMMENT 'start.ini row count, server side only',
    PRIMARY KEY (track_id),
    KEY idx_track_catalog_map (map_id),
    KEY idx_track_catalog_theme (theme_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO track_catalog
    (track_id, map_id, theme_id, folder_name, tail_string, record_field_0, checkpoint_count, start_row_count) VALUES
    (10, 1, 10, 'Forest_01', 'TRACK_FOREST_01_INFO', 1, 14, 16),
    (11, 2, 10, 'Forest_02', 'TRACK_FOREST_02_INFO', 1, 27, 16),
    (12, 3, 10, 'Forest_03', 'TRACK_FOREST_03_INFO', 1, 29, 16),
    (13, 4, 10, 'Forest_04', 'TRACK_FOREST_04_INFO', 1, 25, 16),
    (20, 40, 20, 'Cookie_01', 'TRACK_COOKIE_01_INFO', 1, 17, 16),
    (21, 41, 20, 'Cookie_02', 'TRACK_COOKIE_02_INFO', 1, 27, 16),
    (22, 42, 20, 'Cookie_03', 'TRACK_COOKIE_03_INFO', 1, 37, 16),
    (23, 43, 20, 'Cookie_04', 'TRACK_COOKIE_04_INFO', 1, 39, 16),
    (30, 10, 30, 'Desert_01', 'TRACK_DESERT_01_INFO', 1, 21, 16),
    (31, 11, 30, 'Desert_02', 'TRACK_DESERT_02_INFO', 1, 15, 16),
    (32, 12, 30, 'Desert_03', 'TRACK_DESERT_03_INFO', 1, 29, 16),
    (33, 13, 30, 'Desert_04', 'TRACK_DESERT_04_INFO', 1, 31, 16),
    (40, 50, 40, 'Toy_01', 'TRACK_TOY_01_INFO', 1, 33, 16),
    (41, 51, 40, 'Toy_02', 'TRACK_TOY_02_INFO', 1, 33, 16),
    (42, 52, 40, 'Toy_03', 'TRACK_TOY_03_INFO', 1, 36, 16),
    (43, 53, 40, 'Toy_04', 'TRACK_TOY_04_INFO', 1, 43, 16),
    (50, 60, 50, 'Devil_01', 'TRACK_DEVIL_01_INFO', 1, 23, 16),
    (51, 61, 50, 'Devil_02', 'TRACK_DEVIL_02_INFO', 1, 32, 16),
    (52, 62, 50, 'Devil_03', 'TRACK_DEVIL_03_INFO', 1, 33, 16),
    (53, 63, 50, 'Devil_04', 'TRACK_DEVIL_04_INFO', 1, 19, 16),
    (56, 67, 50, 'Devil_07', '', 1, 32, 16),
    (60, 20, 60, 'Snow_01', 'TRACK_SNOW_01_INFO', 1, 36, 16),
    (61, 21, 60, 'Snow_02', 'TRACK_SNOW_02_INFO', 1, 29, 16),
    (62, 22, 60, 'Snow_03', 'TRACK_SNOW_03_INFO', 1, 25, 16),
    (63, 23, 60, 'Snow_04', 'TRACK_SNOW_04_INFO', 1, 31, 16),
    (70, 30, 70, 'Palace_01', 'TRACK_PALACE_01_INFO', 1, 17, 16),
    (71, 31, 70, 'Palace_02', 'TRACK_PALACE_02_INFO', 1, 25, 16),
    (72, 32, 70, 'Palace_03', 'TRACK_PALACE_03_INFO', 1, 24, 16),
    (73, 33, 70, 'Palace_04', '', 1, 26, 16),
    (74, 34, 70, 'Palace_05', 'TRACK_PALACE_05_INFO', 1, 36, 16),
    (80, 70, 80, 'Swamp_01', 'TRACK_SWAMP_01_INFO', 1, 18, 16),
    (81, 71, 80, 'Swamp_02', 'TRACK_SWAMP_02_INFO', 1, 32, 16),
    (82, 72, 80, 'Swamp_03', 'TRACK_SWAMP_03_INFO', 1, 36, 16),
    (90, 80, 90, 'Race_01', 'TRACK_RACE_01_INFO', 1, 16, 16),
    (91, 81, 90, 'Race_02', 'TRACK_RACE_02_INFO', 1, 29, 16),
    (20000001, 100, 20000000, 'Mission_01', '', 0, 1, 1),
    (20000002, 101, 20000000, 'Mission_02', '', 0, 32, 1),
    (20000003, 102, 20000000, 'Mission_03', '', 0, 22, 8),
    (20000004, 103, 20000000, 'Mission_04', '', 0, 29, 9),
    (20000005, 104, 20000000, 'Mission_05', '', 0, 33, 9)
ON DUPLICATE KEY UPDATE
    map_id           = VALUES(map_id),
    theme_id         = VALUES(theme_id),
    folder_name      = VALUES(folder_name),
    tail_string      = VALUES(tail_string),
    record_field_0   = VALUES(record_field_0),
    checkpoint_count = VALUES(checkpoint_count),
    start_row_count  = VALUES(start_row_count);

UPDATE track_catalog tc
  JOIN maps m ON m.id = tc.map_id
   SET tc.unknown_68 = GREATEST(0, LEAST(5, CAST(m.difficulty AS SIGNED) - 1))
 WHERE tc.map_id IS NOT NULL;

INSERT IGNORE INTO mission_def
  (mission_id, mission_kind, goal_count, time_limit_ms,
   reward_extra, reward_currency_a, reward_currency_b,
   reward_item_type, reward_item_key,
   world_name, str_key_title, str_key_sub, str_key_desc) VALUES
  (0, 0,  0, 120000,  0,  500,  200, 0,     0, 'Mission_01', 'MISSION_01_TITLE', 'MISSION_01_INFO', 'MISSION_01_STORY'),
  (1, 1, 50, 150000,  0,  800,  300, 0,     0, 'Mission_02', 'MISSION_02_TITLE', 'MISSION_02_INFO', 'MISSION_02_STORY'),
  (2, 0,  0, 150000, 10, 1000,  400, 7,     1, 'Mission_03', 'MISSION_03_TITLE', 'MISSION_03_INFO', 'MISSION_03_STORY'),
  (3, 1, 50, 180000,  0, 1500,  600, 0,     5, 'Mission_04', 'MISSION_04_TITLE', 'MISSION_04_INFO', 'MISSION_04_STORY'),
  (4, 0,  0, 180000, 20, 2500, 1000, 1, 10015, 'Mission_05', 'MISSION_05_TITLE', 'MISSION_05_INFO', 'MISSION_05_STORY')
ON DUPLICATE KEY UPDATE
  mission_kind = VALUES(mission_kind), goal_count = VALUES(goal_count),
  time_limit_ms = VALUES(time_limit_ms), reward_extra = VALUES(reward_extra),
  reward_currency_a = VALUES(reward_currency_a), reward_currency_b = VALUES(reward_currency_b),
  reward_item_type = VALUES(reward_item_type), reward_item_key = VALUES(reward_item_key),
  world_name = VALUES(world_name), str_key_title = VALUES(str_key_title),
  str_key_sub = VALUES(str_key_sub), str_key_desc = VALUES(str_key_desc);

UPDATE vehicle_templates SET is_factory_car = 1
 WHERE name IN ('Firedragon','Striper','Circler','Quatzalcuatl','Squarer');

UPDATE vehicle_templates SET is_factory_car = 0
 WHERE name NOT IN ('Firedragon','Striper','Circler','Quatzalcuatl','Squarer');

INSERT IGNORE INTO carcraft_part_def
    (part_key, enabled, category, model_dir_name, display_name_key,
     description_key, stat_block_hex, wheel_attach_0, wheel_attach_1, wheel_attach_2)
VALUES
    (1000, 1, 0, 'Firedragon', 'COVER_1000_TITLE', 'COVER_1000_INFO', '', 0, 0, 0),
    (1001, 1, 0, 'Striper', 'COVER_1001_TITLE', 'COVER_1001_INFO', '', 0, 0, 0),
    (1002, 1, 0, 'Circler', 'COVER_1002_TITLE', 'COVER_1002_INFO', '', 0, 0, 0),
    (1003, 1, 0, 'Quatzalcuatl', 'COVER_1003_TITLE', 'COVER_1003_INFO', '', 0, 0, 0),
    (1004, 1, 0, 'Squarer', 'COVER_1004_TITLE', 'COVER_1004_INFO', '', 0, 0, 0),
    (2000, 1, 1, 'Firedragon', 'BOOSTER_2000_TITLE', 'BOOSTER_2000_INFO', '', 0, 0, 0),
    (2001, 1, 1, 'Striper', 'BOOSTER_2001_TITLE', 'BOOSTER_2001_INFO', '', 0, 0, 0),
    (2002, 1, 1, 'Circler', 'BOOSTER_2002_TITLE', 'BOOSTER_2002_INFO', '', 0, 0, 0),
    (2003, 1, 1, 'Quatzalcuatl', 'BOOSTER_2003_TITLE', 'BOOSTER_2003_INFO', '', 0, 0, 0),
    (2004, 1, 1, 'Squarer', 'BOOSTER_2004_TITLE', 'BOOSTER_2004_INFO', '', 0, 0, 0),
    (3000, 1, 2, 'Firedragon', 'TIRES_3000_TITLE', 'TIRES_3000_INFO', '', 0, 0, 0),
    (3001, 1, 2, 'Striper', 'TIRES_3001_TITLE', 'TIRES_3001_INFO', '', 0, 0, 0),
    (3002, 1, 2, 'Circler', 'TIRES_3002_TITLE', 'TIRES_3002_INFO', '', 0, 0, 0),
    (3003, 1, 2, 'Quatzalcuatl', 'TIRES_3003_TITLE', 'TIRES_3003_INFO', '', 0, 0, 0),
    (3004, 1, 2, 'Squarer', 'TIRES_3004_TITLE', 'TIRES_3004_INFO', '', 0, 0, 0),
    (4000, 1, 3, 'Firedragon', 'F_FENDER_4000_TITLE', 'F_FENDER_4000_INFO', '', 0, 0, 0),
    (4001, 1, 3, 'Striper', 'F_FENDER_4001_TITLE', 'F_FENDER_4001_INFO', '', 0, 0, 0),
    (4002, 1, 3, 'Circler', 'F_FENDER_4002_TITLE', 'F_FENDER_4002_INFO', '', 0, 0, 0),
    (4003, 1, 3, 'Quatzalcuatl', 'F_FENDER_4003_TITLE', 'F_FENDER_4003_INFO', '', 0, 0, 0),
    (4004, 1, 3, 'Squarer', 'F_FENDER_4004_TITLE', 'F_FENDER_4004_INFO', '', 0, 0, 0),
    (5000, 1, 4, 'Firedragon', 'R_FENDER_5000_TITLE', 'R_FENDER_5000_INFO', '', 0, 0, 0),
    (5001, 1, 4, 'Striper', 'R_FENDER_5001_TITLE', 'R_FENDER_5001_INFO', '', 0, 0, 0),
    (5002, 1, 4, 'Circler', 'R_FENDER_5002_TITLE', 'R_FENDER_5002_INFO', '', 0, 0, 0),
    (5003, 1, 4, 'Quatzalcuatl', 'R_FENDER_5003_TITLE', 'R_FENDER_5003_INFO', '', 0, 0, 0),
    (5004, 1, 4, 'Squarer', 'R_FENDER_5004_TITLE', 'R_FENDER_5004_INFO', '', 0, 0, 0),
    (6000, 1, 5, 'Firedragon', 'BUMPER_6000_TITLE', 'BUMPER_6000_INFO', '', 0, 0, 0),
    (6001, 1, 5, 'Striper', 'BUMPER_6001_TITLE', 'BUMPER_6001_INFO', '', 0, 0, 0),
    (6002, 1, 5, 'Circler', 'BUMPER_6002_TITLE', 'BUMPER_6002_INFO', '', 0, 0, 0),
    (6003, 1, 5, 'Quatzalcuatl', 'BUMPER_6003_TITLE', 'BUMPER_6003_INFO', '', 0, 0, 0),
    (6004, 1, 5, 'Squarer', 'BUMPER_6004_TITLE', 'BUMPER_6004_INFO', '', 0, 0, 0),
    (7000, 1, 6, 'Firedragon', 'WING_7000_TITLE', 'WING_7000_INFO', '', 0, 0, 0),
    (7001, 1, 6, 'Striper', 'WING_7001_TITLE', 'WING_7001_INFO', '', 0, 0, 0),
    (7002, 1, 6, 'Circler', 'WING_7002_TITLE', 'WING_7002_INFO', '', 0, 0, 0),
    (7003, 1, 6, 'Quatzalcuatl', 'WING_7003_TITLE', 'WING_7003_INFO', '', 0, 0, 0),
    (7004, 1, 6, 'Squarer', 'WING_7004_TITLE', 'WING_7004_INFO', '', 0, 0, 0)
ON DUPLICATE KEY UPDATE model_dir_name = VALUES(model_dir_name),
    display_name_key = VALUES(display_name_key), category = VALUES(category);

INSERT IGNORE INTO room_object_def
    (object_key, enabled, badge_flag, category, max_placeable, required_level,
     asset_folder, name_loc_key, desc_loc_key)
VALUES
    (1001, 1, 0, 0, 1, 0, 'SKY01', 'SKY_01_TITLE', 'SKY_01_INFO'),
    (1002, 1, 0, 0, 1, 0, 'SKY02', 'SKY_02_TITLE', 'SKY_02_INFO'),
    (1003, 1, 0, 0, 1, 0, 'SKY03', 'Sky_03_TITLE', 'Sky_03_INFO'),
    (1004, 1, 0, 0, 1, 0, 'SKY04', 'Sky_04_TITLE', 'Sky_04_INFO'),
    (1005, 1, 0, 0, 1, 0, 'SKY05', 'Sky_05_TITLE', 'Sky_05_INFO'),
    (1006, 1, 0, 0, 1, 0, 'Sky06', 'Sky_06_TITLE', 'Sky_06_INFO'),
    (1007, 1, 0, 0, 1, 0, 'Sky07', 'SKY_07_TITLE', 'SKY_07_INFO'),
    (1008, 1, 0, 0, 1, 0, 'Sky08', 'SKY_08_TITLE', 'SKY_08_INFO'),
    (1009, 1, 0, 0, 1, 0, 'sky09', 'Sky_09_TITLE', 'Sky_09_INFO'),
    (1010, 1, 0, 0, 1, 0, 'sky10', 'Sky_10_TITLE', 'Sky_10_INFO'),
    (1011, 1, 0, 0, 1, 0, 'sky11', 'Sky_11_TITLE', 'Sky_11_INFO'),
    (2001, 1, 0, 1, 1, 0, 'Floor01', 'FLOOR_01_TITLE', 'FLOOR_01_INFO'),
    (2002, 1, 0, 1, 1, 0, 'Floor02', 'FLOOR_02_TITLE', 'FLOOR_02_INFO'),
    (2003, 1, 0, 1, 1, 0, 'Floor03', 'FLOOR_03_TITLE', 'FLOOR_03_INFO'),
    (2004, 1, 0, 1, 1, 0, 'floor04', 'FLOOR_04_TITLE', 'FLOOR_04_INFO'),
    (2005, 1, 0, 1, 1, 0, 'toyfloor01', 'FLOOR_05_TITLE', 'FLOOR_05_INFO'),
    (2006, 1, 0, 1, 1, 0, 'toyfloor02', 'FLOOR_06_TITLE', 'FLOOR_06_INFO'),
    (2007, 1, 0, 1, 1, 0, 'toyfloor03', 'FLOOR_07_TITLE', 'FLOOR_07_INFO'),
    (2008, 1, 0, 1, 1, 0, 'Ani_Floor01', 'FLOOR_08_TITLE', 'FLOOR_08_INFO'),
    (2009, 1, 0, 1, 1, 0, 'Ani_Floor02', 'FLOOR_09_TITLE', 'FLOOR_09_INFO'),
    (3001, 1, 0, 2, 1, 0, 'mountain01', 'BACKGROUND_01_TITLE', 'BACKGROUND_01_INFO'),
    (3002, 1, 0, 2, 1, 0, 'waterfall01', 'BACKGROUND_02_TITLE', 'BACKGROUND_02_INFO'),
    (4001, 1, 0, 3, 10, 0, 'fortree01', 'OBJECT_01_TITLE', 'OBJECT_01_INFO'),
    (4002, 1, 0, 3, 10, 0, 'fortree02', 'OBJECT_02_TITLE', 'OBJECT_02_INFO'),
    (4003, 1, 0, 3, 10, 0, 'fortree03', 'OBJECT_03_TITLE', 'OBJECT_03_INFO'),
    (4004, 1, 0, 3, 10, 0, 'mushroom01', 'OBJECT_04_TITLE', 'OBJECT_04_INFO'),
    (4005, 1, 0, 3, 10, 0, 'melonfield01', 'OBJECT_05_TITLE', 'OBJECT_05_INFO'),
    (4006, 1, 0, 3, 10, 0, 'monkey01', 'OBJECT_06_TITLE', 'OBJECT_06_INFO'),
    (4007, 1, 0, 3, 10, 0, 'frog', 'OBJECT_07_TITLE', 'OBJECT_07_INFO'),
    (4008, 1, 0, 3, 10, 0, 'fence01', 'OBJECT_08_TITLE', 'OBJECT_08_INFO'),
    (4009, 1, 0, 3, 10, 0, 'panther01', 'OBJECT_09_TITLE', 'OBJECT_09_INFO'),
    (4010, 1, 0, 3, 10, 0, 'grass01', 'OBJECT_10_TITLE', 'OBJECT_10_INFO'),
    (4011, 1, 0, 3, 10, 0, 'grass02', 'OBJECT_11_TITLE', 'OBJECT_11_INFO'),
    (4012, 1, 0, 3, 10, 0, 'grass03', 'OBJECT_12_TITLE', 'OBJECT_12_INFO'),
    (4013, 1, 0, 3, 10, 0, 'toytree01', 'OBJECT_13_TITLE', 'OBJECT_13_INFO'),
    (4014, 1, 0, 3, 10, 0, 'toytree02', 'OBJECT_14_TITLE', 'OBJECT_14_INFO'),
    (4015, 1, 0, 3, 10, 0, 'toyfence01', 'OBJECT_15_TITLE', 'OBJECT_15_INFO'),
    (4016, 1, 0, 3, 10, 0, 'Legohouse01', 'OBJECT_16_TITLE', 'OBJECT_16_INFO'),
    (4017, 1, 0, 3, 10, 0, 'Legohouse02', 'OBJECT_17_TITLE', 'OBJECT_17_INFO'),
    (4018, 1, 0, 3, 10, 0, 'Book01', 'OBJECT_18_TITLE', 'OBJECT_18_INFO'),
    (4019, 1, 0, 3, 10, 0, 'Toyshoot01', 'OBJECT_19_TITLE', 'OBJECT_19_INFO'),
    (4020, 1, 0, 3, 10, 0, 'Puzzle', 'OBJECT_20_TITLE', 'OBJECT_20_INFO'),
    (4021, 1, 0, 3, 10, 0, 'tong01', 'OBJECT_21_TITLE', 'OBJECT_21_INFO'),
    (4022, 1, 0, 3, 10, 0, 'Book02', 'OBJECT_22_TITLE', 'OBJECT_22_INFO'),
    (4023, 1, 0, 3, 10, 0, 'Robot01', 'OBJECT_23_TITLE', 'OBJECT_23_INFO'),
    (4024, 1, 0, 3, 10, 0, 'sheep01', 'OBJECT_24_TITLE', 'OBJECT_24_INFO'),
    (4025, 1, 0, 3, 10, 0, 'toyfence02', 'OBJECT_25_TITLE', 'OBJECT_25_INFO'),
    (4026, 1, 0, 3, 10, 0, 'Kong01', 'OBJECT_26_TITLE', 'OBJECT_26_INFO'),
    (4027, 1, 0, 3, 10, 0, 'Dollhouse01', 'OBJECT_27_TITLE', 'OBJECT_27_INFO'),
    (4028, 1, 0, 3, 10, 0, 'Bed01', 'OBJECT_28_TITLE', 'OBJECT_28_INFO'),
    (4029, 1, 0, 3, 10, 0, 'flower01', 'OBJECT_29_TITLE', 'OBJECT_29_INFO'),
    (4030, 1, 0, 3, 10, 0, 'flower02', 'OBJECT_30_TITLE', 'OBJECT_30_INFO'),
    (4031, 1, 0, 3, 10, 0, 'Horse01', 'OBJECT_31_TITLE', 'OBJECT_31_INFO'),
    (4032, 1, 0, 3, 10, 0, 'Horse02', 'OBJECT_32_TITLE', 'OBJECT_32_INFO'),
    (5001, 1, 0, 4, 4, 0, 'flame01', 'EFFECT_01_TITLE', 'EFFECT_01_INFO'),
    (5002, 1, 0, 4, 4, 0, 'rain01', 'EFFECT_02_TITLE', 'EFFECT_02_INFO'),
    (5003, 1, 0, 4, 4, 0, 'snow01', 'EFFECT_03_TITLE', 'EFFECT_03_INFO'),
    (5004, 1, 0, 4, 4, 0, 'Petal01', 'EFFECT_04_TITLE', 'EFFECT_04_INFO'),
    (5005, 1, 0, 4, 4, 0, 'Skull01', 'EFFECT_05_TITLE', 'EFFECT_05_INFO'),
    (5006, 1, 0, 4, 4, 0, 'Star01', 'EFFECT_06_TITLE', 'EFFECT_06_INFO'),
    (5007, 1, 0, 4, 4, 0, 'pumpkin', 'EFFECT_07_TITLE', 'EFFECT_07_INFO'),
    (5008, 1, 0, 4, 4, 0, 'heart01', 'EFFECT_08_TITLE', 'EFFECT_08_INFO'),
    (5009, 1, 0, 4, 4, 0, 'note01', 'EFFECT_09_TITLE', 'EFFECT_09_INFO'),
    (5010, 1, 0, 4, 4, 0, 'bat', 'EFFECT_10_TITLE', 'EFFECT_10_INFO'),
    (5011, 1, 0, 4, 4, 0, 'goblin01', 'EFFECT_11_TITLE', 'EFFECT_11_INFO')
ON DUPLICATE KEY UPDATE asset_folder = VALUES(asset_folder),
    category = VALUES(category), name_loc_key = VALUES(name_loc_key);

-- disabled on purpose the seed below hands every character every part

INSERT IGNORE INTO owned_character (character_id, base_key, period_mode, period_value, active_flag)
SELECT c.id, COALESCE(NULLIF(c.driver_base_key, 0), c.equipped_driver_id, 1), 0, 0, 1
  FROM characters c
 WHERE COALESCE(NULLIF(c.driver_base_key, 0), c.equipped_driver_id, 0) <> 0;

INSERT IGNORE INTO owned_kart (character_id, base_key, period_mode, period_value, active_flag)
SELECT v.character_id, v.vehicle_type_id, 3, GREATEST(COALESCE(v.durability, 100), 0), 1
  FROM vehicles v
 WHERE v.vehicle_type_id <> 0;

UPDATE characters c
  JOIN owned_kart k ON k.character_id = c.id
   SET c.selected_kart_instance_id = k.id
 WHERE c.selected_kart_instance_id = 0;

INSERT IGNORE INTO owned_item (character_id, base_key, unk_08, period_mode, period_value, active_flag, in_use_flag)
SELECT c.id, 1000, 0, 2, 5, 1, 0 FROM characters c;

CREATE TABLE IF NOT EXISTS def_kart_part_wire (
    part_key INT UNSIGNED NOT NULL PRIMARY KEY COMMENT 'record +0x08 container key',
    visible INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'record +0x00',
    badge INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'record +0x04',
    word_0c INT NOT NULL DEFAULT 0 COMMENT 'record +0x0C, no reader proven',
    model_name VARCHAR(36) NOT NULL DEFAULT '' COMMENT 'record +0x10, buffer 36',
    word_34 INT NOT NULL DEFAULT 0 COMMENT 'record +0x34, no reader proven',
    word_38 INT NOT NULL DEFAULT 0 COMMENT 'record +0x38, install slot code',
    word_3c INT NOT NULL DEFAULT -1 COMMENT 'record +0x3C, driver restriction, -1 any',
    display_name_key VARCHAR(33) NOT NULL DEFAULT '' COMMENT 'record +0x40, buffer 33',
    description_key VARCHAR(35) NOT NULL DEFAULT '' COMMENT 'record +0x61, buffer 35',
    tail0_hex CHAR(16) NOT NULL DEFAULT '0000000000000000' COMMENT 'record +0x84, 8 bytes hex',
    tail1_hex CHAR(16) NOT NULL DEFAULT '0000000000000000' COMMENT 'record +0x8C, 8 bytes hex'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO def_kart_part_wire
  (part_key, visible, badge, word_0c, model_name, word_34, word_38, word_3c,
   display_name_key, description_key, tail0_hex, tail1_hex)
VALUES
  (2001, 1, 0, 0, 'DS1_ENGINE_001', 0, 0, -1, 'PART_2001', '', '0000000000000000', '0000000000000000'),
  (2002, 1, 0, 0, 'DS1_BUMPER_001', 0, 1, -1, 'PART_2002', '', '0000000000000000', '0000000000000000'),
  (2003, 1, 0, 0, 'DS1_WING_001',   0, 8, -1, 'PART_2003', '', '0000000000000000', '0000000000000000'),
  (2004, 1, 0, 0, 'QD1_ENGINE_001', 0, 0, -1, 'PART_2004', '', '0000000000000000', '0000000000000000'),
  (2005, 1, 0, 0, 'QD1_BUMPER_001', 0, 1, -1, 'PART_2005', '', '0000000000000000', '0000000000000000'),
  (2006, 1, 0, 0, 'QD1_WING_001',   0, 8, -1, 'PART_2006', '', '0000000000000000', '0000000000000000'),
  (2007, 1, 0, 0, 'RB1_ENGINE_001', 0, 0, -1, 'PART_2007', '', '0000000000000000', '0000000000000000'),
  (2008, 1, 0, 0, 'RB1_BUMPER_001', 0, 1, -1, 'PART_2008', '', '0000000000000000', '0000000000000000'),
  (2009, 1, 0, 0, 'RB1_WING_001',   0, 8, -1, 'PART_2009', '', '0000000000000000', '0000000000000000');

CREATE TABLE IF NOT EXISTS def_item_wire (
    item_key INT UNSIGNED NOT NULL PRIMARY KEY,
    visible INT UNSIGNED NOT NULL DEFAULT 1,
    badge INT UNSIGNED NOT NULL DEFAULT 0,
    use_type INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'definition +0x0C',
    word_10 INT UNSIGNED NOT NULL DEFAULT 0,
    display_name_key VARCHAR(33) NOT NULL DEFAULT '',
    icon_key VARCHAR(36) NOT NULL DEFAULT '',
    description_key VARCHAR(35) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO def_item_wire
  (item_key, visible, badge, use_type, word_10, display_name_key, icon_key, description_key)
VALUES (1000, 1, 0, 0, 0, 'ITEM_1000', '', '');

INSERT IGNORE INTO owned_pet
  (character_id, base_key, equipped_flag, unk_0c, period_mode, period_value, active_flag)
SELECT c.id, 1, 1, 0, 0, 0, 1 FROM characters c;
