-- replaces the 002 003 placeholder seed data with real content enumerated from the shipped DevClient assets

-- vehicle templates columns match the VehicleData struct order so a row copies straight into a blob
CREATE TABLE IF NOT EXISTS vehicle_templates (
    id INT UNSIGNED PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE,     -- client asset basename matches DevClient Data Car folder name
    display_name VARCHAR(96) NOT NULL,
    category ENUM('starter','kart','bike','concept','special','premium','event','bonus') DEFAULT 'kart',
    rarity ENUM('common','uncommon','rare','epic','legendary') DEFAULT 'common',
    required_level INT UNSIGNED DEFAULT 1,
    price_gold INT UNSIGNED DEFAULT 0,
    price_cash INT UNSIGNED DEFAULT 0,
    -- VehicleData stat layout as int32 per certified offsets 0x10 to 0x28
    max_durability INT NOT NULL DEFAULT 100,
    stat_speed INT NOT NULL DEFAULT 50,
    stat_accel INT NOT NULL DEFAULT 50,
    stat_handling INT NOT NULL DEFAULT 50,
    stat_drift INT NOT NULL DEFAULT 40,
    stat_boost INT NOT NULL DEFAULT 30,
    stat_weight INT NOT NULL DEFAULT 50,
    stat_special INT NOT NULL DEFAULT 0,
    is_enabled TINYINT(1) DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- owned vehicles reference a template id for stats while older rows still keep individual columns for upgrades
ALTER TABLE vehicles
    ADD COLUMN IF NOT EXISTS template_id INT UNSIGNED NULL AFTER vehicle_type_id;

-- backfills older rows using the old type id as the template reference
UPDATE vehicles SET template_id = vehicle_type_id WHERE template_id IS NULL;

-- seeds the 48 cars from DevClient Car ids by tier basic licensed concept and themed
INSERT INTO vehicle_templates
    (id, name, display_name, category, rarity, required_level, price_gold, price_cash,
     max_durability, stat_speed, stat_accel, stat_handling, stat_drift, stat_boost, stat_weight, stat_special) VALUES
    -- Starters
    (10001, 'default',         'Default Kart',     'starter', 'common',    1,     0, 0, 100, 45, 50, 55, 40, 30, 50, 0),
    (10010, 'basic_1',          'Basic Kart 1',     'starter', 'common',    1,  1000, 0, 100, 50, 50, 50, 40, 30, 50, 0),
    (10011, 'basic_2',          'Basic Kart 2',     'starter', 'common',    2,  2000, 0, 100, 52, 50, 50, 40, 32, 50, 0),
    (10012, 'basic_3',          'Basic Kart 3',     'starter', 'common',    3,  3000, 0, 100, 54, 52, 50, 40, 34, 50, 0),
    (10013, 'basic_4',          'Basic Kart 4',     'starter', 'common',    5,  5000, 0, 100, 56, 54, 52, 42, 36, 50, 0),
    (10014, 'basic_5',          'Basic Kart 5',     'starter', 'common',    7,  7000, 0, 100, 58, 56, 54, 44, 38, 50, 0),
    (10015, 'basic_6',          'Basic Kart 6',     'starter', 'common',   10, 10000, 0, 100, 60, 58, 56, 46, 40, 50, 0),
    -- Bikes
    (10101, 'bike_01',          'Sport Bike',       'bike',    'uncommon',  4,  6000, 0,  90, 62, 60, 45, 50, 44, 40, 0),
    (10102, 'bike_02',          'Street Bike',      'bike',    'uncommon',  6,  8000, 0,  90, 64, 60, 46, 52, 46, 40, 0),
    (10103, 'bike_03',          'Racing Bike',      'bike',    'rare',     10, 15000, 0,  90, 68, 62, 48, 54, 48, 40, 0),
    (10104, 'bike_03_Ongame',   'Racing Bike R',    'bike',    'rare',     12, 18000, 0,  90, 68, 64, 48, 54, 50, 40, 0),
    (10105, 'bike_04',          'Drift Bike',       'bike',    'rare',     12, 16000, 0,  90, 66, 60, 50, 60, 46, 40, 0),
    (10106, 'bike_05',          'Turbo Bike',       'bike',    'epic',     15, 28000, 0,  90, 72, 65, 48, 52, 54, 40, 0),
    (10107, 'bike_06',          'Super Bike',       'bike',    'epic',     18, 35000, 0,  90, 74, 68, 50, 54, 56, 40, 0),
    -- licensed and showcar rows
    (11001, 'citroen',          'Licensed Sedan',   'premium', 'rare',      8, 12000, 0, 110, 60, 58, 62, 40, 38, 60, 0),
    (11002, 'kona',             'Licensed SUV',     'premium', 'rare',      9, 13000, 0, 120, 58, 56, 60, 40, 36, 65, 0),
    (11003, 'mini',             'Mini',             'premium', 'rare',      6, 10000, 0, 110, 60, 62, 64, 44, 40, 55, 0),
    (11004, 'mini_ongame',      'Mini Ongame',      'premium', 'rare',      8, 14000, 0, 110, 62, 64, 64, 44, 42, 55, 0),
    (11101, 'ds_01',            'DS Model',         'premium', 'epic',     10, 20000, 0, 110, 66, 60, 66, 42, 44, 58, 0),
    (11201, 'qd_01',            'QD Model 1',       'premium', 'rare',      9, 15000, 0, 110, 62, 60, 62, 42, 42, 56, 0),
    (11202, 'qd_02',            'QD Model 2',       'premium', 'epic',     12, 22000, 0, 110, 66, 62, 64, 44, 46, 58, 0),
    (11301, 'rb_01',            'RB Model 1',       'premium', 'rare',      9, 15000, 0, 110, 64, 60, 60, 42, 44, 56, 0),
    (11302, 'rb_02',            'RB Model 2',       'premium', 'epic',     13, 25000, 0, 110, 68, 64, 62, 44, 48, 58, 0),
    (11401, 'fr_01',            'FR Model 1',       'premium', 'uncommon',  5,  8000, 0, 100, 58, 56, 58, 42, 38, 52, 0),
    (11402, 'fr_02',            'FR Model 2',       'premium', 'uncommon',  6, 10000, 0, 100, 60, 58, 58, 42, 40, 52, 0),
    (11403, 'fr_03',            'FR Model 3',       'premium', 'rare',      8, 14000, 0, 100, 62, 60, 60, 44, 42, 52, 0),
    (11404, 'fr_04',            'FR Model 4',       'premium', 'rare',     10, 18000, 0, 100, 64, 62, 60, 44, 44, 54, 0),
    (11405, 'fr_05',            'FR Model 5',       'premium', 'epic',     13, 25000, 0, 100, 68, 64, 62, 46, 48, 54, 0),
    -- concept and special rows
    (12001, 'M500',             'M500',             'concept', 'epic',     15, 40000, 0, 100, 72, 66, 56, 50, 52, 48, 0),
    (12002, 'Circler',          'Circler',          'concept', 'epic',     15, 0,   120, 100, 68, 60, 72, 60, 50, 45, 0),
    (12003, 'Squarer',          'Squarer',          'concept', 'epic',     15, 0,   120, 120, 64, 56, 68, 40, 46, 70, 0),
    (12004, 'Striper',          'Striper',          'concept', 'rare',     12, 30000, 0, 100, 66, 62, 62, 46, 50, 50, 0),
    (12005, 'Firedragon',       'Fire Dragon',      'special', 'legendary', 20, 0,   300, 100, 80, 70, 58, 50, 65, 45, 5),
    (12006, 'Quatzalcuatl',     'Quetzalcoatl',     'special', 'legendary', 20, 0,   300, 100, 78, 72, 60, 52, 62, 45, 5),
    (12007, 'thunder',          'Thunder',          'special', 'epic',     16, 0,   150, 100, 74, 68, 56, 48, 56, 48, 0),
    (12008, 'tank',             'Tank',             'special', 'rare',     12, 0,   100, 150, 50, 48, 55, 30, 36, 90, 0),
    (12009, 'toilet',           'Toilet',           'special', 'uncommon',  5,  5000, 0,  80, 48, 50, 52, 40, 32, 40, 0),
    (12010, 'oskart',           'O.S. Kart',        'special', 'rare',      8, 12000, 0, 100, 60, 58, 58, 42, 40, 52, 0),
    -- themed and event rows
    (13001, 'halloween',        'Halloween Kart',   'event',   'rare',     10, 0,    80, 100, 62, 60, 60, 44, 44, 50, 0),
    (13002, 'pumpkin',          'Pumpkin Rider',    'event',   'rare',     10, 0,    80, 100, 60, 58, 62, 46, 42, 50, 0),
    (13003, 'mummy',            'Mummy Rider',      'event',   'rare',     12, 0,   100, 100, 62, 60, 60, 46, 44, 50, 0),
    (13004, 'witch',            'Witch Broom',      'event',   'rare',     12, 0,   100,  90, 66, 64, 55, 50, 46, 42, 0),
    (13005, 'wolf',             'Wolf Rider',       'event',   'rare',     12, 0,   100, 100, 64, 62, 58, 48, 44, 50, 0),
    (13006, 'prince',           'Prince',           'event',   'epic',     15, 0,   150, 100, 66, 62, 62, 48, 46, 52, 0),
    (13007, 'princess',         'Princess',         'event',   'epic',     15, 0,   150, 100, 64, 60, 64, 50, 44, 50, 0),
    (13008, 'monster',          'Monster',          'event',   'epic',     15, 0,   150, 120, 68, 60, 56, 42, 50, 60, 0),
    (13009, 'yuk',              'Yuk',              'event',   'epic',     15, 0,   150, 100, 66, 62, 60, 46, 46, 50, 0),
    (13010, 'Rudolf_01',        'Rudolf',           'bonus',   'legendary', 1, 0,     0, 100, 60, 58, 60, 46, 44, 50, 0)
ON DUPLICATE KEY UPDATE
    display_name = VALUES(display_name),
    category = VALUES(category),
    rarity = VALUES(rarity),
    required_level = VALUES(required_level),
    price_gold = VALUES(price_gold),
    price_cash = VALUES(price_cash);

-- points the legacy starter template id 1001 at the canonical basic row
INSERT IGNORE INTO vehicle_templates
    (id, name, display_name, category, rarity, required_level, price_gold,
     max_durability, stat_speed, stat_accel, stat_handling, stat_drift, stat_boost, stat_weight, stat_special)
VALUES
    (1001, 'legacy_starter', 'Starter Kart (legacy)', 'starter', 'common', 1, 0,
     100, 50, 50, 50, 40, 30, 50, 0);

-- item templates replace generic placeholders with 25 real folders from DevClient Data Public Item ids grouped by category

-- adds columns needed for the race loot table if they do not already exist
ALTER TABLE item_templates
    ADD COLUMN IF NOT EXISTS effect_type VARCHAR(32) DEFAULT 'none' AFTER category,
    ADD COLUMN IF NOT EXISTS effect_value INT DEFAULT 0 AFTER effect_type,
    ADD COLUMN IF NOT EXISTS drop_weight INT UNSIGNED DEFAULT 0 COMMENT 'Race loot table weight (0 = not droppable)',
    ADD COLUMN IF NOT EXISTS asset_name VARCHAR(64) NULL COMMENT 'Matches DevClient/Data/Public/Item/<name>/';

-- clears placeholder item rows 1001 to 4004 that never matched a real asset
DELETE FROM item_templates WHERE id BETWEEN 1001 AND 4004;

INSERT INTO item_templates
    (id, name, category, effect_type, effect_value, drop_weight, asset_name, rarity, base_price, description) VALUES
    -- Standard race arsenal
    (20001, 'Bomb',         'item', 'explode',    50, 100, 'Bomb',        'common',    200, 'Place a bomb trap behind you'),
    (20002, 'Rocket',       'item', 'homing',     40, 120, 'Rocket',      'common',    250, 'Homing missile seeks the racer ahead'),
    (20003, 'Hammer',       'item', 'stun',       30,  90, 'Hammer',      'common',    220, 'Swing a hammer at nearby racers'),
    (20004, 'Spike',        'item', 'trap',       20,  80, 'Spike',       'common',    180, 'Drop a spike trap'),
    (20005, 'Ice',          'item', 'freeze',     40,  60, 'Ice',         'uncommon',  350, 'Freeze a targeted racer'),
    (20006, 'Storm',        'item', 'aoe_slow',   50,  40, 'Storm',       'rare',      600, 'Summon a storm that slows all ahead'),
    (20007, 'Thunder',      'item', 'aoe_stun',   60,  20, 'Thunder',     'epic',     1200, 'Strike all racers ahead with lightning'),
    (20008, 'Hive',         'item', 'aoe_slow',   30,  50, 'Hive',        'uncommon',  400, 'Release a bee hive'),
    (20009, 'Handle',       'item', 'self_turn',  10,  40, 'Handle',      'common',    150, 'Tighten steering for 5s'),
    -- Utility
    (20101, 'Shield',       'item', 'defense',   100, 150, 'Shield',      'common',    150, 'Block one incoming item'),
    (20102, 'Magnet',       'item', 'attract',    80,  70, 'Magnet',      'uncommon',  400, 'Pull coins and items toward you'),
    (20103, 'Smoke',        'item', 'obscure',    40,  60, 'Smoke',       'common',    180, 'Leave a smoke cloud behind'),
    (20104, 'Flash',        'item', 'blind',      30,  40, 'Flash',       'uncommon',  350, 'Blind racers behind you briefly'),
    (20105, 'Dung',         'item', 'stick',      30,  50, 'Dung',        'common',    200, 'Stick a sticky trap behind'),
    (20106, 'Bite',         'item', 'bite',       30,  40, 'Bite',        'uncommon',  300, 'Bite the nearest racer'),
    -- companion and pet item rows
    (20201, 'Angel',        'item', 'heal',       40,  30, 'Angel',       'rare',      700, 'Summon an angel that protects you'),
    (20202, 'Devil',        'item', 'curse',      40,  30, 'Devil',       'rare',      700, 'Curse the leader'),
    (20203, 'DevilRed',     'item', 'curse',      60,  15, 'DevilRed',    'epic',     1400, 'Red devil — stronger curse'),
    (20204, 'Rabbit',       'item', 'speed',      30, 100, 'Rabbit',      'common',    250, 'Quick speed burst'),
    (20205, 'BlueRabbit',   'item', 'speed',      50,  40, 'BlueRabbit',  'uncommon',  500, 'Stronger speed burst'),
    (20206, 'Turtle',       'item', 'defense',    60,  60, 'Turtle',      'uncommon',  400, 'Heavy shell defense'),
    (20207, 'Money',        'item', 'coins',     100,  25, 'Money',       'rare',      800, 'Instantly gain bonus coins'),
    -- environment and special rows not droppable so drop weight is 0
    (20901, 'ItemBox',      'item', 'container',   0,   0, 'ItemBox',     'common',      0, 'In-race item box prop (not owned)'),
    (20902, 'ItemBall',     'item', 'container',   0,   0, 'ItemBall',    'common',      0, 'In-race item ball prop'),
    (20903, 'ItemDrum',     'item', 'container',   0,   0, 'ItemDrum',    'common',      0, 'In-race item drum prop')
ON DUPLICATE KEY UPDATE
    effect_type = VALUES(effect_type),
    effect_value = VALUES(effect_value),
    drop_weight = VALUES(drop_weight),
    asset_name = VALUES(asset_name),
    rarity = VALUES(rarity),
    base_price = VALUES(base_price),
    description = VALUES(description);

-- maps replace the placeholder seed with real theme based track names from DevClient Data Public World
ALTER TABLE maps
    ADD COLUMN IF NOT EXISTS theme_folder VARCHAR(32) DEFAULT NULL AFTER name,
    ADD COLUMN IF NOT EXISTS track_folder VARCHAR(64) DEFAULT NULL AFTER theme_folder;

-- clears the placeholder rows before seeding the real ones
DELETE FROM maps WHERE id BETWEEN 1 AND 100;

-- full track list enumerated from DevClient World folders that contain tracknif or geometrynif alternate dumps are skipped
INSERT INTO maps
    (id, name, display_name, category, difficulty, min_players, max_players,
     theme_folder, track_folder) VALUES
    -- forest tracks
    (1,  'forest_01', 'Forest Circuit',      'normal', 1, 1, 8, 'Forest', 'Forest_01'),
    (2,  'forest_02', 'Forest Canyon',       'normal', 2, 1, 8, 'Forest', 'Forest_02'),
    (3,  'forest_03', 'Deep Woods',          'normal', 3, 1, 8, 'Forest', 'Forest_03'),
    (4,  'forest_04', 'Forest Rally',        'normal', 4, 1, 8, 'Forest', 'Forest_04'),
    -- desert tracks
    (10, 'desert_01', 'Desert Dunes',        'normal', 1, 1, 8, 'Desert', 'Desert_01'),
    (11, 'desert_02', 'Desert Mirage',       'normal', 2, 1, 8, 'Desert', 'Desert_02'),
    (12, 'desert_03', 'Sand Storm',          'normal', 3, 1, 8, 'Desert', 'Desert_03'),
    (13, 'desert_04', 'Oasis Run',           'normal', 4, 1, 8, 'Desert', 'Desert_04'),
    -- snow tracks
    (20, 'snow_01',   'Snow Peak',           'normal', 1, 1, 8, 'Snow',   'Snow_01'),
    (21, 'snow_02',   'Frozen Lake',         'normal', 2, 1, 8, 'Snow',   'Snow_02'),
    (22, 'snow_03',   'Ice Cavern',          'normal', 3, 1, 8, 'Snow',   'Snow_03'),
    (23, 'snow_04',   'Glacier Slide',       'normal', 4, 1, 8, 'Snow',   'Snow_04'),
    -- palace tracks
    (30, 'palace_01', 'Royal Garden',        'normal', 2, 1, 8, 'Palace', 'Palace_01'),
    (31, 'palace_02', 'Palace Halls',        'normal', 3, 1, 8, 'Palace', 'Palace_02'),
    (32, 'palace_03', 'Throne Room',         'normal', 4, 1, 8, 'Palace', 'Palace_03'),
    (33, 'palace_04', 'Castle Yard',         'normal', 3, 1, 8, 'Palace', 'Palace_04'),
    (34, 'palace_05', 'Royal Ballroom',      'normal', 5, 1, 8, 'Palace', 'Palace_05'),
    -- cookie tracks
    (40, 'cookie_01', 'Cookie Town',         'normal',     1, 1, 8, 'Cookie', 'Cookie_01'),
    (41, 'cookie_02', 'Candy Rush',          'normal',     2, 1, 8, 'Cookie', 'Cookie_02'),
    (42, 'cookie_03', 'Sugar Speedway',      'normal',     3, 1, 8, 'Cookie', 'Cookie_03'),
    (43, 'cookie_04', 'Gingerbread Hills',   'normal',     4, 1, 8, 'Cookie', 'Cookie_04'),
    -- toy tracks
    (50, 'toy_01',    'Toy Room',            'normal',     1, 1, 8, 'Toy',    'Toy_01'),
    (51, 'toy_02',    'Attic Adventure',     'normal',     2, 1, 8, 'Toy',    'Toy_02'),
    (52, 'toy_03',    'Block City',          'normal',     3, 1, 8, 'Toy',    'Toy_03'),
    (53, 'toy_04',    'Playroom',            'normal',     4, 1, 8, 'Toy',    'Toy_04'),
    -- devil and hard tracks four present two skipped plus one extra
    (60, 'devil_01',  'Demon Valley',        'normal', 4, 1, 8, 'Devil',  'Devil_01'),
    (61, 'devil_02',  'Inferno',             'normal', 5, 1, 8, 'Devil',  'Devil_02'),
    (62, 'devil_03',  'Hellfire Pit',        'normal', 5, 1, 8, 'Devil',  'Devil_03'),
    (63, 'devil_04',  'Cursed Pass',         'normal', 5, 1, 8, 'Devil',  'Devil_04'),
    (67, 'devil_07',  'Abyss',               'normal', 5, 1, 8, 'Devil',  'Devil_07'),
    -- swamp tracks
    (70, 'swamp_01',  'Swamp Trail',         'normal', 3, 1, 8, 'Swamp',  'Swamp_01'),
    (71, 'swamp_02',  'Bog Path',            'normal', 3, 1, 8, 'Swamp',  'Swamp_02'),
    (72, 'swamp_03',  'Murky Waters',        'normal', 4, 1, 8, 'Swamp',  'Swamp_03'),
    -- race time trial tracks
    (80, 'race_01',   'Speed Test',          'bonus',    1, 1, 1, 'Race',   'Race_01'),
    (81, 'race_02',   'Time Attack',         'bonus',    2, 1, 1, 'Race',   'Race_02'),
    -- license tutorial tracks
    (90, 'license_01','License Test 1',      'tutorial',1, 1, 1, 'License','License_01'),
    (91, 'license_02','License Test 2',      'tutorial',2, 1, 1, 'License','License_02'),
    (92, 'license_03','License Test 3',      'tutorial',3, 1, 1, 'License','License_03'),
    -- mission scenario maps
    (100,'mission_01','Mission One',         'scenario', 1, 1, 4, 'Mission','Mission_01'),
    (101,'mission_02','Mission Two',         'scenario', 2, 1, 4, 'Mission','Mission_02'),
    (102,'mission_03','Mission Three',       'scenario', 3, 1, 4, 'Mission','Mission_03'),
    (103,'mission_04','Mission Four',        'scenario', 4, 1, 4, 'Mission','Mission_04'),
    (104,'mission_05','Mission Five',        'scenario', 5, 1, 4, 'Mission','Mission_05')
ON DUPLICATE KEY UPDATE
    display_name = VALUES(display_name),
    theme_folder = VALUES(theme_folder),
    track_folder = VALUES(track_folder);

-- pets replace the placeholder rows with the 4 real pet models from DevClient Data Public Pet
DELETE FROM pets WHERE id BETWEEN 1 AND 10;
INSERT INTO pets (id, name, display_name, rarity, price_gold, price_cash, effect_type, effect_value) VALUES
    (1, 'Pet_01', 'Pet 01 - Rookie',   'common',    2000,   0, 'luck',    5),
    (2, 'Pet_02', 'Pet 02 - Speedy',   'common',    5000,   0, 'speed',   3),
    (3, 'Pet_03', 'Pet 03 - Guardian', 'rare',     12000,   0, 'defense',10),
    (4, 'Pet_04', 'Pet 04 - Mystic',   'epic',         0, 100, 'item',    8)
ON DUPLICATE KEY UPDATE
    name = VALUES(name),
    display_name = VALUES(display_name),
    rarity = VALUES(rarity),
    price_gold = VALUES(price_gold),
    price_cash = VALUES(price_cash),
    effect_type = VALUES(effect_type),
    effect_value = VALUES(effect_value);

-- shop rows promote placeholders to the real vehicle and item ids adding new stock entries
DELETE FROM shop_items WHERE template_id BETWEEN 1001 AND 3099;

INSERT INTO shop_items
    (category, template_id, name, price_gold, price_cash, required_level, stock) VALUES
    -- Vehicles
    ('vehicle', 10010, 'Basic Kart 1',    1000,   0,  1, -1),
    ('vehicle', 10011, 'Basic Kart 2',    2000,   0,  2, -1),
    ('vehicle', 10012, 'Basic Kart 3',    3000,   0,  3, -1),
    ('vehicle', 10013, 'Basic Kart 4',    5000,   0,  5, -1),
    ('vehicle', 10014, 'Basic Kart 5',    7000,   0,  7, -1),
    ('vehicle', 10015, 'Basic Kart 6',   10000,   0, 10, -1),
    ('vehicle', 10101, 'Sport Bike',      6000,   0,  4, -1),
    ('vehicle', 10103, 'Racing Bike',    15000,   0, 10, -1),
    ('vehicle', 11003, 'Mini',           10000,   0,  6, -1),
    ('vehicle', 12005, 'Fire Dragon',        0, 300, 20, 100),
    -- Items
    ('item',    20001, 'Bomb',             200,   0,  1, -1),
    ('item',    20002, 'Rocket',           250,   0,  1, -1),
    ('item',    20003, 'Hammer',           220,   0,  1, -1),
    ('item',    20101, 'Shield',           150,   0,  1, -1),
    ('item',    20204, 'Rabbit',           250,   0,  1, -1),
    ('item',    20206, 'Turtle',           400,   0,  5, -1),
    -- accessory rows reuse legacy ids from the 002 complete data migration
    ('accessory', 3001, 'Speed +5',       1000,   0, 1, -1),
    ('accessory', 3002, 'Accel +5',       1000,   0, 1, -1),
    ('accessory', 3003, 'Handling +5',    1000,   0, 1, -1)
ON DUPLICATE KEY UPDATE
    name = VALUES(name),
    price_gold = VALUES(price_gold),
    price_cash = VALUES(price_cash);

-- missions replace placeholders with real race driven missions keyed off race history
DELETE FROM missions WHERE id BETWEEN 1 AND 100;

-- extends the mission type enum before inserting since it lacks shop purchase and license pass
ALTER TABLE missions
    MODIFY mission_type ENUM('race','win','item','collect','bonus','tutorial','shop_purchase','license_pass') NOT NULL;

INSERT INTO missions
    (id, name, description, category, mission_type, target_count, reward_gold, reward_cash, reward_xp, is_repeatable) VALUES
    (1, 'First Lap',         'Complete your first race',         'story',       'race',          1,   500,   0, 100, 0),
    (2, 'Rookie Win',        'Win a race for the first time',    'story',       'win',           1,  1000,   0, 200, 0),
    (3, 'Daily Racer',       'Finish 3 races today',             'daily',       'race',          3,   300,   0,  50, 1),
    (4, 'Daily Winner',      'Win 1 race today',                 'daily',       'win',           1,   500,   0, 100, 1),
    (5, 'Weekly Veteran',    'Finish 20 races this week',        'weekly',      'race',         20,  2000,  50, 500, 1),
    (6, 'Grand Prix',        'Win 10 races this week',           'weekly',      'win',          10,  3000, 100, 800, 1),
    (7, 'Garage Collector',  'Own 5 different vehicles',         'achievement', 'collect',       5,  1500,   0, 300, 0),
    (8, 'Item Buyer',        'Purchase 10 items from the shop',  'achievement', 'shop_purchase',10,   500,   0, 100, 0),
    (9, 'License Apprentice','Pass the rookie license exam',     'story',       'license_pass',  1,  1000,   0, 500, 0),
    (10,'Tutorial',          'Complete the tutorial track',      'story',       'tutorial',      1,  1000,   0, 500, 0)
ON DUPLICATE KEY UPDATE
    name = VALUES(name),
    description = VALUES(description),
    reward_gold = VALUES(reward_gold),
    reward_xp = VALUES(reward_xp);
