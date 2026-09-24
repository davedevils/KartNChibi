-- empty owned character and owned kart rows meant no 0x1B or 0x1C ever sent

-- one body per character using the first enabled driver
INSERT INTO owned_character
    (character_id, base_key, acc_body, acc_face, acc_head, acc_glass, acc_back,
     unk_1c, period_mode, period_value, active_flag)
SELECT c.id,
       COALESCE(NULLIF(c.equipped_driver_id, 0),
                (SELECT MIN(d.id) FROM drivers d WHERE COALESCE(d.is_enabled, 1) = 1)),
       0, 0, 0, 0, 0, 0, 0, 0, 1
FROM characters c
WHERE NOT EXISTS (SELECT 1 FROM owned_character o WHERE o.character_id = c.id);

-- one kart per character using the first catalog entry
INSERT INTO owned_kart
    (character_id, base_key, skin_primary, skin_secondary, skin_tertiary,
     custom3, custom4, custom5)
SELECT c.id,
       (SELECT MIN(k.kart_id) FROM kart_catalog k),
       0, 0, 0, 0, 0, 0
FROM characters c
WHERE NOT EXISTS (SELECT 1 FROM owned_kart o WHERE o.character_id = c.id);

-- points each character at its new rows since selected kart instance id drove the profile blob
UPDATE characters c
JOIN (SELECT character_id, MIN(id) AS kart_id FROM owned_kart GROUP BY character_id) k
  ON k.character_id = c.id
SET c.selected_kart_instance_id = k.kart_id
WHERE c.selected_kart_instance_id = 0;

-- equipped driver id must not stay zero since the driver resolves through it
UPDATE characters c
JOIN (SELECT character_id, MIN(base_key) AS base_key FROM owned_character GROUP BY character_id) o
  ON o.character_id = c.id
SET c.equipped_driver_id = o.base_key
WHERE COALESCE(c.equipped_driver_id, 0) = 0;
