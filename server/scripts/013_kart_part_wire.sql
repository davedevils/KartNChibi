-- def kart part wire feeds PartStatPackets an empty table left sub 409DA0 and sub 490A70 with no starter part

DROP TABLE IF EXISTS def_kart_part_wire;

CREATE TABLE def_kart_part_wire (
    part_key          INT UNSIGNED NOT NULL PRIMARY KEY,
    visible           INT UNSIGNED NOT NULL DEFAULT 1,
    badge             INT UNSIGNED NOT NULL DEFAULT 0,
    word_0c           INT UNSIGNED NOT NULL DEFAULT 0,
    model_name        VARCHAR(32)  NOT NULL DEFAULT '',
    word_34           INT UNSIGNED NOT NULL DEFAULT 0,
    word_38           INT UNSIGNED NOT NULL DEFAULT 0,
    word_3c           INT UNSIGNED NOT NULL DEFAULT 0,
    display_name_key  VARCHAR(32)  NOT NULL DEFAULT '',
    description_key   VARCHAR(33)  NOT NULL DEFAULT '',
    tail0_hex         VARCHAR(136) NOT NULL DEFAULT '',
    tail1_hex         VARCHAR(136) NOT NULL DEFAULT '',
    category          INT UNSIGNED NOT NULL DEFAULT 0,
    KEY idx_category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- word 0c carries the category the only field the client uses to bucket a part into its slot
INSERT INTO def_kart_part_wire
    (part_key, visible, badge, word_0c, model_name,
     word_34, word_38, word_3c, display_name_key, description_key,
     tail0_hex, tail1_hex, category)
SELECT
    part_key,
    enabled,
    0,
    category,
    model_dir_name,
    0, 0, 0,
    display_name_key,
    description_key,
    '', '',
    category
FROM carcraft_part_def;

-- every character needs one part per category or sub 490A70 has no body to attach
INSERT INTO owned_part (character_id, base_key, period_mode, period_value, active_flag)
SELECT c.id, d.part_key, 0, 0, 1
FROM characters c
JOIN (
    SELECT MIN(part_key) AS part_key
    FROM def_kart_part_wire
    WHERE visible <> 0
    GROUP BY category
) d
WHERE NOT EXISTS (
    SELECT 1 FROM owned_part o WHERE o.character_id = c.id AND o.base_key = d.part_key
);

-- the car craft stage reads instances not owned part sub 490A70 needs one per category
INSERT INTO custom_car_part_instance
    (character_id, part_key, category, equip_refcount, price_table_key,
     period_type, period_value, period_active, grade)
SELECT c.id, d.part_key, d.category, 0, 0, 0, 0, 1, 0
FROM characters c
JOIN (
    SELECT MIN(part_key) AS part_key, category
    FROM def_kart_part_wire
    WHERE visible <> 0
    GROUP BY category
) d
WHERE NOT EXISTS (
    SELECT 1 FROM custom_car_part_instance i
    WHERE i.character_id = c.id AND i.part_key = d.part_key
);

-- a zero equip refcount left the whole 60 byte block empty so sub 490A70 had no body
UPDATE custom_car_part_instance SET equip_refcount = 1 WHERE equip_refcount = 0;

-- fill every preset slot from the owned instance of the matching category
UPDATE custom_car_preset p
JOIN characters c ON c.id = p.character_id
SET p.part_inst_cover   = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 0 LIMIT 1), p.part_inst_cover),
    p.part_inst_booster = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 1 LIMIT 1), p.part_inst_booster),
    p.part_inst_tire    = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 2 LIMIT 1), p.part_inst_tire),
    p.part_inst_ffender = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 3 LIMIT 1), p.part_inst_ffender),
    p.part_inst_rfender = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 4 LIMIT 1), p.part_inst_rfender),
    p.part_inst_bumper  = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 5 LIMIT 1), p.part_inst_bumper),
    p.part_inst_wing    = COALESCE((SELECT i.instance_id FROM custom_car_part_instance i WHERE i.character_id = c.id AND i.category = 6 LIMIT 1), p.part_inst_wing);

-- every lobby bar button is gated by a licence level in sub 42BCE0 leaving fresh characters a dead menu
UPDATE characters SET license_class = 1
WHERE tutorial_completed = 1 AND COALESCE(license_class, 0) = 0;

-- sub 4A5ED0 and sub 4510C0 copy 32 bytes from owned kart over the kart def block so keys live there
UPDATE owned_kart k
JOIN vehicle_templates vt ON vt.id = k.base_key
SET k.skin_primary   = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 0 LIMIT 1), 0),
    k.skin_secondary = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 1 LIMIT 1), 0),
    k.skin_tertiary  = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 2 LIMIT 1), 0),
    k.custom3        = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 3 LIMIT 1), 0),
    k.custom4        = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 4 LIMIT 1), 0),
    k.custom5        = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 5 LIMIT 1), 0),
    k.applied_item_a = COALESCE((SELECT d.part_key FROM carcraft_part_def d WHERE d.model_dir_name = vt.name AND d.category = 6 LIMIT 1), 0)
WHERE k.skin_primary = 0;

-- the model builder resolves owned kart slots into folder names slot one is a body colour
DROP TABLE IF EXISTS def_kart_skin;
CREATE TABLE def_kart_skin (
    skin_key  INT UNSIGNED NOT NULL PRIMARY KEY,
    name      VARCHAR(32)  NOT NULL,
    category  INT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO def_kart_skin (skin_key, name, category) VALUES
 (9001,'BLACK',0), (9002,'BLUE',0),   (9003,'GREEN',0),
 (9004,'ORANGE',0),(9005,'PINK',0),   (9006,'PURPPLE',0),
 (9007,'RED',0),   (9008,'SILVER',0), (9009,'YELLOW',0);

-- this chassis has no body folder so it can never render as a plain kart
UPDATE owned_kart SET base_key = 10010 WHERE base_key = 12002;
UPDATE vehicles   SET vehicle_type_id = 10010 WHERE vehicle_type_id = 12002;

-- slot one is the body colour the rest stay to be measured one at a time
UPDATE owned_kart SET skin_primary = 9007, skin_secondary = 9007;

-- slot two resolves into the kart name plate folder and is required the build returns 0 without it
INSERT INTO def_kart_skin (skin_key, name, category) VALUES
 (9101,'NAMEBOX_001',1), (9102,'NAMEBOX_002',1), (9103,'NAMEBOX_003',1),
 (9104,'NAMEBOX_004',1), (9105,'NAMEBOX_005',1), (9106,'NAMEBOX_006',1)
ON DUPLICATE KEY UPDATE name = VALUES(name);

UPDATE owned_kart SET skin_secondary = 9101;

-- slot three resolves into an item folder and is skipped when null an unequipped kart must carry zero there
UPDATE owned_kart SET skin_tertiary = 0, custom3 = 0, custom4 = 0, custom5 = 0,
                      applied_item_a = 0, applied_item_b = 0;

-- the licence and room stages overwrite the driver def with five dwords resolved onto bones per sub 443D10
UPDATE owned_character oc
JOIN drivers d ON d.id = oc.base_key
SET oc.acc_body = COALESCE((SELECT s.skin_key FROM def_kart_skin s
                            WHERE s.category = 2 AND s.name = CONCAT(d.name,'_char_body_001')), 0),
    oc.acc_face = COALESCE((SELECT s.skin_key FROM def_kart_skin s
                            WHERE s.category = 2 AND s.name = CONCAT(d.name,'_char_face_001')), 0),
    oc.acc_head = COALESCE((SELECT s.skin_key FROM def_kart_skin s
                            WHERE s.category = 2 AND s.name = CONCAT(d.name,'_char_head_001')), 0)
WHERE oc.acc_body = 0;

-- these five slot keys are what the client resolves for body face and head or the character renders faceless
INSERT INTO def_kart_skin (skin_key, name, category) VALUES
 (9201,'Cosmo_char_body_001',2),
 (9202,'Cosmo_char_face_001',2),
 (9203,'Cosmo_char_head_001',2),
 (9204,'Monster_char_body_001',2),
 (9205,'Monster_char_face_001',2),
 (9206,'Monster_char_head_001',2),
 (9207,'Moriko_char_body_001',2),
 (9208,'Moriko_char_face_001',2),
 (9209,'Moriko_char_head_001',2),
 (9210,'Mummy_char_body_001',2),
 (9211,'Mummy_char_face_001',2),
 (9212,'Mummy_char_head_001',2),
 (9213,'Prince_char_body_001',2),
 (9214,'Prince_char_face_001',2),
 (9215,'Prince_char_head_001',2),
 (9216,'Princess_char_body_001',2),
 (9217,'Princess_char_face_001',2),
 (9218,'Princess_char_head_001',2),
 (9219,'Pumpkin_char_body_001',2),
 (9220,'Pumpkin_char_face_001',2),
 (9221,'Pumpkin_char_head_001',2),
 (9222,'Witch_char_body_001',2),
 (9223,'Witch_char_face_001',2),
 (9224,'Witch_char_head_001',2),
 (9225,'Wolf_char_body_001',2),
 (9226,'Wolf_char_face_001',2),
 (9227,'Wolf_char_head_001',2),
 (9228,'Yuk_char_body_001',2),
 (9229,'Yuk_char_face_001',2),
 (9230,'Yuk_char_head_001',2)
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- equip them on every owned character matching the driver body asset folder
UPDATE owned_character oc
JOIN drivers d ON d.id = oc.base_key
JOIN def_kart_skin sb ON sb.category = 2 AND sb.name = CONCAT(
        CASE LOWER(d.name) WHEN 'racer_mummy' THEN 'Mummy' ELSE CONCAT(UPPER(LEFT(d.name,1)), LOWER(SUBSTRING(d.name,2))) END,
        '_char_body_001')
SET oc.acc_body = sb.skin_key,
    oc.acc_face = sb.skin_key + 1,
    oc.acc_head = sb.skin_key + 2;

-- real starters are Pumpkin and Witch the racer rows have no body folder and only render via the Cosmo fallback
UPDATE owned_character oc
JOIN drivers d ON LOWER(d.name) = 'pumpkin'
SET oc.base_key = d.id,
    oc.acc_body = (SELECT skin_key FROM def_kart_skin WHERE name = 'Pumpkin_char_body_001'),
    oc.acc_face = (SELECT skin_key FROM def_kart_skin WHERE name = 'Pumpkin_char_face_001'),
    oc.acc_head = (SELECT skin_key FROM def_kart_skin WHERE name = 'Pumpkin_char_head_001')
WHERE oc.character_id = 2;

UPDATE characters c
JOIN drivers d ON LOWER(d.name) = 'pumpkin'
SET c.driver_base_key = d.id, c.equipped_driver_id = d.id
WHERE c.id = 2;

-- licence practice tracks sub 41F690 and sub 453330 need track 0 or the tutorial stage fails silently
INSERT INTO theme_catalog (theme_id, theme_folder, display_name, record_field_0)
VALUES (30000000, 'License', 'THEME_LICENSE_INFO', 1)
ON DUPLICATE KEY UPDATE theme_folder = VALUES(theme_folder);

INSERT INTO track_catalog (track_id, map_id, theme_id, folder_name, tail_string, required_level)
VALUES (0, NULL, 30000000, 'License_01', '', 0),
       (1, NULL, 30000000, 'License_02', '', 0),
       (2, NULL, 30000000, 'License_03', '', 0)
ON DUPLICATE KEY UPDATE folder_name = VALUES(folder_name), theme_id = VALUES(theme_id);

-- the licence reward dialog reads two track slots as EXP and gold not fog so send raw integers
ALTER TABLE track_catalog
    ADD COLUMN IF NOT EXISTS wire_slot_48 INT UNSIGNED NOT NULL DEFAULT 1053609165,
    ADD COLUMN IF NOT EXISTS wire_slot_52 INT UNSIGNED NOT NULL DEFAULT 1058642330;

-- keep every existing track byte identical these are the 0dot4 and 0dot6 bit patterns
UPDATE track_catalog SET wire_slot_48 = 1053609165, wire_slot_52 = 1058642330
WHERE track_id > 2;

-- licence practice pays a small sane reward
UPDATE track_catalog SET wire_slot_48 = 50, wire_slot_52 = 100 WHERE track_id <= 2;

-- the seed character was below the level 5 floor so progression kept logging exp below floor
UPDATE characters c
JOIN level_curve lc ON lc.level = c.level
SET c.experience = lc.cum_exp
WHERE c.experience < lc.cum_exp;

-- license test def drives both the credited reward and the client's predicted amount on screen
INSERT INTO license_test_def (license_key, unknown_00, name, param_00, param_01, param_02, str_key_a, str_key_b)
VALUES
 ( 0,0,'LICENSE_R_00', 0,  50, 100,'',''), ( 1,0,'LICENSE_R_01', 1,  50, 100,'',''),
 ( 2,0,'LICENSE_R_02', 2,  50, 100,'',''), ( 3,0,'LICENSE_R_03', 3,  50, 100,'',''),
 (10,0,'LICENSE_A_00',10, 150, 300,'',''), (11,0,'LICENSE_A_01',11, 150, 300,'',''),
 (12,0,'LICENSE_A_02',12, 150, 300,'',''), (13,0,'LICENSE_A_03',13, 150, 300,'',''),
 (20,0,'LICENSE_M_00',20, 300, 600,'',''), (21,0,'LICENSE_M_01',21, 300, 600,'',''),
 (22,0,'LICENSE_M_02',22, 300, 600,'',''), (23,0,'LICENSE_M_03',23, 300, 600,'','')
ON DUPLICATE KEY UPDATE param_01 = VALUES(param_01), param_02 = VALUES(param_02);

-- real display names read out of the translation files the client draws the raw key when it cannot resolve one
ALTER TABLE drivers ADD COLUMN IF NOT EXISTS display_name VARCHAR(32) NOT NULL DEFAULT '';
UPDATE drivers SET display_name = CASE LOWER(name)
    WHEN 'pumpkin'     THEN 'Jacko'
    WHEN 'witch'       THEN 'Madea'
    WHEN 'monster'     THEN 'Frankie'
    WHEN 'princess'    THEN 'Buttercup'
    WHEN 'prince'      THEN 'Prince Waddles III'
    WHEN 'racer_mummy' THEN 'Cleo'
    WHEN 'wolf'        THEN 'Huck'
    WHEN 'yuk'         THEN 'Brag'
    ELSE display_name END;

-- licence tests advertise an item reward that nothing ever filled these columns drive both the grant and the reward blob
ALTER TABLE license_test_def
    ADD COLUMN IF NOT EXISTS reward_item_type  INT UNSIGNED NOT NULL DEFAULT 7,
    ADD COLUMN IF NOT EXISTS reward_item_key   INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS reward_item_count INT UNSIGNED NOT NULL DEFAULT 0;

-- 1000 is the swap consumable the only item key the client ever reports back
UPDATE license_test_def SET reward_item_type = 7, reward_item_key = 1000,
    reward_item_count = CASE WHEN license_key < 10 THEN 50
                             WHEN license_key < 20 THEN 75
                             ELSE 100 END;

-- the custom car preset pointed at a factory chassis after the owned kart moved so sub 490A70 refused
UPDATE custom_car_preset p
JOIN owned_kart k ON k.character_id = p.character_id
LEFT JOIN carcraft_part_def d ON d.model_dir_name = (
        SELECT vt.name FROM vehicle_templates vt WHERE vt.id = k.base_key)
SET p.kart_instance_id = 0
WHERE d.part_key IS NULL;
