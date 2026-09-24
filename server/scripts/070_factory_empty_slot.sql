-- 070 the car factory opens on one empty slot like the real server and a preset name fits 9 chars
-- sub 432B20 widens the name of a built slot into a 10 WCHAR stack cell with a 20 char count
-- so the 11 char default Factory Car overran the stack cookie on the name plate click
-- a kart skin column never holds a zero key sub 484B10 puts the kart def paint and plate back on a Remove

-- an empty slot carries kart 0 so a character may hold several of them
ALTER TABLE custom_car_preset DROP INDEX IF EXISTS uq_preset_kart;

-- the server made default name and any name past the rename cell
UPDATE custom_car_preset SET name = '' WHERE name = 'Factory Car' OR CHAR_LENGTH(name) > 9;

-- a built car needs an owned factory chassis and an owned live tire else it is the empty slot again
UPDATE custom_car_preset p
LEFT JOIN owned_kart k ON k.id = p.kart_instance_id AND k.character_id = p.character_id
LEFT JOIN vehicle_templates v ON v.id = k.base_key
LEFT JOIN custom_car_part_instance t ON t.instance_id = p.part_inst_tire
      AND t.character_id = p.character_id AND t.category = 2 AND t.period_active <> 0
SET p.slot_state = 0, p.name = '', p.kart_instance_id = 0,
    p.part_inst_cover = 0, p.part_inst_tire = 0, p.part_inst_booster = 0, p.part_inst_bumper = 0,
    p.part_inst_ffender = 0, p.part_inst_rfender = 0, p.part_inst_wing = 0
WHERE p.slot_state <> 1 OR k.id IS NULL OR COALESCE(v.is_factory_car, 0) = 0 OR t.instance_id IS NULL;

ALTER TABLE custom_car_preset
    MODIFY COLUMN slot_state INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0x0107 row 0x04 0 the empty slot 1 a built car sub 433140 draws the name plate only on 1',
    MODIFY COLUMN name VARCHAR(9) NOT NULL DEFAULT ''
        COMMENT 'ascii 9 chars sub 432B20 and sub 455C20 copy it into 10 WCHAR cells';

-- a part counts as worn only while a built car holds it
UPDATE custom_car_part_instance i
SET i.equip_refcount = IF(EXISTS (
        SELECT 1 FROM custom_car_preset p
        WHERE p.character_id = i.character_id AND p.slot_state = 1
          AND i.instance_id IN (p.part_inst_cover, p.part_inst_tire, p.part_inst_booster,
                                p.part_inst_bumper, p.part_inst_ffender, p.part_inst_rfender,
                                p.part_inst_wing)), 1, 0);

-- the paint and the plate a Remove used to zero go back to the kart def keys
UPDATE owned_kart SET skin_primary = 9007 WHERE skin_primary = 0;
UPDATE owned_kart SET skin_secondary = 9100 WHERE skin_secondary = 0;
