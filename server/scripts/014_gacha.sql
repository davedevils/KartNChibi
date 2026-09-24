-- ticket is owned item base key 2000 or 2001 checked by sub 4571D0 sub 4516D0 and sub 4830C0

-- banners one per client tab the popup only has two tabs so the third stays inactive
ALTER TABLE gacha_banners
    ADD COLUMN IF NOT EXISTS ticket_base_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'owned_item.base_key the client hardcodes, 2000 tab 0, 2001 tab 1';

UPDATE gacha_banners SET ticket_base_key = 2000, is_active = 1 WHERE id = 1;
UPDATE gacha_banners SET ticket_base_key = 2001, is_active = 1 WHERE id = 2;
-- no third tab exists in sub 4576B0 it registers Gacha tab C and Gacha tab P only
UPDATE gacha_banners SET ticket_base_key = 0,    is_active = 0 WHERE id = 3;

-- prize pool loadPrizePool reads exactly these columns each category resolves against a different client catalog
DROP TABLE IF EXISTS gacha_items;
CREATE TABLE gacha_items (
    id              INT UNSIGNED NOT NULL AUTO_INCREMENT,
    gacha_id        INT UNSIGNED NOT NULL,
    ticket_base_key INT UNSIGNED NOT NULL COMMENT '2000 or 2001, matches the owned_item row',
    weight          INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'relative, 0 is never picked',
    rare_flag       TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'non zero plays the rare machine',
    prize_category  INT UNSIGNED NOT NULL COMMENT '0 char 1 kart 2 item 3 part',
    prize_base_key  INT UNSIGNED NOT NULL COMMENT 'key in that category own catalog',
    period_mode     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 permanent 1 days 2 times',
    period_value    INT NOT NULL DEFAULT 0,
    rarity          ENUM('common','uncommon','rare','epic','legendary') NOT NULL DEFAULT 'common',
    label           VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'human note, never sent on the wire',
    enabled         TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (id),
    KEY idx_ticket (ticket_base_key, enabled),
    CONSTRAINT gacha_items_banner_fk FOREIGN KEY (gacha_id)
        REFERENCES gacha_banners (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- roll log columns are exactly what logRollInTx inserts
DROP TABLE IF EXISTS gacha_history;
CREATE TABLE gacha_history (
    id                 BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    character_id       INT UNSIGNED NOT NULL,
    ticket_instance_id INT UNSIGNED NOT NULL DEFAULT 0,
    ticket_base_key    INT UNSIGNED NOT NULL DEFAULT 0,
    remaining_after    INT NOT NULL DEFAULT 0,
    rare_flag          TINYINT UNSIGNED NOT NULL DEFAULT 0,
    prize_category     INT UNSIGNED NOT NULL DEFAULT 6 COMMENT '6 means the roll paid nothing',
    prize_base_key     INT UNSIGNED NOT NULL DEFAULT 0,
    created_at         TIMESTAMP NULL DEFAULT current_timestamp(),
    PRIMARY KEY (id),
    KEY idx_character (character_id),
    KEY idx_ticket (ticket_base_key),
    CONSTRAINT gacha_history_char_fk FOREIGN KEY (character_id)
        REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- standard ticket 2000 banner 1 weights sum to 1000 parts reach the client 0x00C2 container via PacketBuilder partCatalog
INSERT INTO gacha_items
 (gacha_id, ticket_base_key, weight, rare_flag, prize_category, prize_base_key,
  period_mode, period_value, rarity, label) VALUES
 (1, 2000,  80, 0, 3, 1000, 0, 0, 'common',   'COVER Firedragon'),
 (1, 2000,  80, 0, 3, 1001, 0, 0, 'common',   'COVER Striper'),
 (1, 2000,  80, 0, 3, 1003, 0, 0, 'common',   'COVER Quatzalcuatl'),
 (1, 2000,  80, 0, 3, 3000, 0, 0, 'common',   'TIRES Firedragon'),
 (1, 2000,  80, 0, 3, 3001, 0, 0, 'common',   'TIRES Striper'),
 (1, 2000,  70, 0, 3, 4000, 0, 0, 'common',   'F_FENDER Firedragon'),
 (1, 2000,  70, 0, 3, 4001, 0, 0, 'common',   'F_FENDER Striper'),
 (1, 2000,  60, 0, 3, 5000, 0, 0, 'uncommon', 'cat4 Firedragon'),
 (1, 2000,  60, 0, 3, 6000, 0, 0, 'uncommon', 'cat5 Firedragon'),
 (1, 2000,  60, 0, 3, 7000, 0, 0, 'uncommon', 'cat6 Firedragon'),
 -- item base key 1000 is the race swap consumable proven to land in the 0x001D container
 (1, 2000, 120, 0, 2, 1000, 2,  3, 'common',   'swap consumable x3'),
 (1, 2000,  50, 0, 2, 1000, 2, 10, 'uncommon', 'swap consumable x10'),
 (1, 2000,  40, 1, 0,    6, 0, 0, 'rare',     'driver Cosmo'),
 (1, 2000,  40, 1, 0,    7, 0, 0, 'rare',     'driver Moriko'),
 (1, 2000,  20, 1, 1, 10011, 0, 0, 'epic',    'kart basic_2'),
 (1, 2000,  10, 1, 1, 10012, 0, 0, 'epic',    'kart basic_3');

-- premium ticket 2001 banner 2 weights sum to 1000
INSERT INTO gacha_items
 (gacha_id, ticket_base_key, weight, rare_flag, prize_category, prize_base_key,
  period_mode, period_value, rarity, label) VALUES
 (2, 2001,  60, 0, 3, 1004, 0, 0, 'common',   'COVER Squarer'),
 (2, 2001,  60, 0, 3, 2004, 0, 0, 'common',   'BOOSTER Squarer'),
 (2, 2001,  60, 0, 3, 3004, 0, 0, 'common',   'TIRES Squarer'),
 (2, 2001,  60, 0, 3, 4004, 0, 0, 'common',   'F_FENDER Squarer'),
 (2, 2001,  50, 0, 3, 5004, 0, 0, 'uncommon', 'cat4 Squarer'),
 (2, 2001,  50, 0, 3, 6004, 0, 0, 'uncommon', 'cat5 Squarer'),
 (2, 2001,  50, 0, 3, 7004, 0, 0, 'uncommon', 'cat6 Squarer'),
 (2, 2001, 110, 0, 2, 1000, 2, 20, 'uncommon', 'swap consumable x20'),
 (2, 2001,  70, 1, 0,    5, 0, 0, 'rare',     'driver Cleo'),
 (2, 2001,  70, 1, 0,   11, 0, 0, 'rare',     'driver Madea'),
 (2, 2001,  70, 1, 0,   12, 0, 0, 'rare',     'driver Huck'),
 (2, 2001,  50, 1, 0,   13, 0, 0, 'epic',     'driver Frankie'),
 (2, 2001,  50, 1, 0,   14, 0, 0, 'epic',     'driver Brag'),
 (2, 2001,  40, 1, 0,    8, 0, 0, 'epic',     'driver Prince Waddles III'),
 (2, 2001,  40, 1, 0,    9, 0, 0, 'epic',     'driver Buttercup'),
 (2, 2001,  30, 1, 1, 10101, 0, 0, 'rare',      'kart bike_01'),
 (2, 2001,  25, 1, 1, 10103, 0, 0, 'rare',      'kart bike_03'),
 (2, 2001,  25, 1, 1, 11003, 0, 0, 'rare',      'kart mini'),
 (2, 2001,  20, 1, 1, 13010, 0, 0, 'legendary', 'kart Rudolf'),
 (2, 2001,  10, 1, 1, 12007, 0, 0, 'epic',      'kart thunder');

-- without an owned item row on 2000 sub 4571D0 draws zero and sub 4830C0 refuses to send
INSERT INTO owned_item (character_id, base_key, unk_08, period_mode, period_value,
                        active_flag, in_use_flag)
SELECT c.id, 2000, 0, 2, 20, 1, 0 FROM characters c
WHERE NOT EXISTS (SELECT 1 FROM owned_item o
                  WHERE o.character_id = c.id AND o.base_key = 2000);

INSERT INTO owned_item (character_id, base_key, unk_08, period_mode, period_value,
                        active_flag, in_use_flag)
SELECT c.id, 2001, 0, 2, 10, 1, 0 FROM characters c
WHERE NOT EXISTS (SELECT 1 FROM owned_item o
                  WHERE o.character_id = c.id AND o.base_key = 2001);
