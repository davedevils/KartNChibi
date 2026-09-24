-- 072 the car factory lists every owned chassis one slot each with a BASIC part set installed
-- sub 42EFE0 draws one top row slot per built preset and the garage lists only built presets
-- a chassis runs on the durability bar owned kart 0x2C mode 3 and 0x30 up to 500

-- a basic set per chassis so one part key may have several instances
ALTER TABLE custom_car_part_instance DROP INDEX IF EXISTS uq_part_char_key;
ALTER TABLE custom_car_preset DROP INDEX IF EXISTS uq_preset_kart;

UPDATE owned_kart k
JOIN vehicle_templates v ON v.id = k.base_key
SET k.period_mode = 3, k.period_value = 500
WHERE COALESCE(v.is_factory_car, 0) = 1 AND k.period_mode <> 3;

-- marks the parts granted here so the slots below find them
ALTER TABLE custom_car_part_instance ADD COLUMN IF NOT EXISTS fit_kart INT UNSIGNED NULL;

DROP TEMPORARY TABLE IF EXISTS tmp_fit;
CREATE TEMPORARY TABLE tmp_fit AS
SELECT k.character_id, k.id AS kart_id, v.name AS model
FROM owned_kart k
JOIN vehicle_templates v ON v.id = k.base_key
WHERE COALESCE(v.is_factory_car, 0) = 1 AND COALESCE(v.is_enabled, 1) = 1
  AND NOT EXISTS (SELECT 1 FROM custom_car_preset p
                  WHERE p.character_id = k.character_id AND p.slot_state = 1
                    AND p.kart_instance_id = k.id);

-- the lowest part key of each category in the chassis folder permanent grade 0
INSERT INTO custom_car_part_instance
    (character_id, part_key, category, equip_refcount, price_table_key, period_type,
     period_value, period_active, grade, fit_kart)
SELECT f.character_id, d.part_key, d.category, 0,
       COALESCE((SELECT pr.price_table_key FROM carcraft_part_price pr
                 WHERE pr.part_key = d.part_key AND pr.period_type = 0
                 ORDER BY pr.row_index LIMIT 1), 0),
       0, 0, 1, 0, f.kart_id
FROM tmp_fit f
JOIN carcraft_part_def d ON LOWER(d.model_dir_name) = LOWER(f.model)
WHERE d.category <= 6
  AND d.part_key = (SELECT MIN(d2.part_key) FROM carcraft_part_def d2
                    WHERE d2.category = d.category AND LOWER(d2.model_dir_name) = LOWER(f.model));

-- an empty slot next to an owned chassis would draw a blank slot in the top row
DELETE p FROM custom_car_preset p
WHERE p.slot_state <> 1
  AND p.character_id IN (SELECT k.character_id FROM owned_kart k
                         JOIN vehicle_templates v ON v.id = k.base_key
                         WHERE COALESCE(v.is_factory_car, 0) = 1);

INSERT INTO custom_car_preset
    (character_id, slot_state, name, kart_instance_id, part_inst_cover, part_inst_tire,
     part_inst_booster, part_inst_bumper, part_inst_ffender, part_inst_rfender, part_inst_wing)
SELECT f.character_id, 1, '', f.kart_id,
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 0), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 2), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 1), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 5), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 3), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 4), 0),
       COALESCE((SELECT MIN(i.instance_id) FROM custom_car_part_instance i WHERE i.fit_kart = f.kart_id AND i.category = 6), 0)
FROM tmp_fit f;

DROP TEMPORARY TABLE IF EXISTS tmp_fit;
ALTER TABLE custom_car_part_instance DROP COLUMN IF EXISTS fit_kart;

-- the equip count is how many built slots hold the part sub 42F6C0 counts the same way
UPDATE custom_car_part_instance i
SET i.equip_refcount = (
    SELECT COALESCE(SUM((p.part_inst_cover = i.instance_id) + (p.part_inst_tire = i.instance_id) +
                        (p.part_inst_booster = i.instance_id) + (p.part_inst_bumper = i.instance_id) +
                        (p.part_inst_ffender = i.instance_id) + (p.part_inst_rfender = i.instance_id) +
                        (p.part_inst_wing = i.instance_id)), 0)
    FROM custom_car_preset p
    WHERE p.character_id = i.character_id AND p.slot_state = 1);
