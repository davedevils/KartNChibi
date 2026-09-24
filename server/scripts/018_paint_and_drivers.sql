-- 018 universal paint real driver roster accessory backfill

-- only five body colours ship for every kart model a black paint with no folder makes sub 490A70 render white
ALTER TABLE owned_kart MODIFY skin_primary INT UNSIGNED NOT NULL DEFAULT 9007;
UPDATE owned_kart SET skin_primary = 9007
 WHERE skin_primary NOT IN (9002, 9003, 9006, 9007, 9009);

-- racer basic pro speed and drift are seed junk racer mummy is real renamed to what the client expects
UPDATE drivers SET name = 'mummy' WHERE id = 5 AND name = 'racer_mummy';
UPDATE drivers SET is_enabled = 0 WHERE id IN (1, 2, 3, 4);

-- a row on a dead driver is deleted the unique key blocks remapping onto one the player already owns
DELETE FROM owned_character WHERE base_key IN (1, 2, 3, 4);
UPDATE characters SET equipped_driver_id = 10 WHERE equipped_driver_id IN (1, 2, 3, 4);
UPDATE characters SET driver_base_key = 10 WHERE driver_base_key IN (1, 2, 3, 4);

-- a driver bought or won came with every BODYSET slot at zero invisible chibi resolved from the mesh table
UPDATE owned_character oc
  JOIN drivers d ON d.id = oc.base_key
  JOIN def_kart_skin s ON s.category = 2 AND s.name LIKE CONCAT(d.name, '\_char\_body\_%')
   SET oc.acc_body = s.skin_key
 WHERE oc.acc_body = 0;

UPDATE owned_character oc
  JOIN drivers d ON d.id = oc.base_key
  JOIN def_kart_skin s ON s.category = 2 AND s.name LIKE CONCAT(d.name, '\_char\_face\_%')
   SET oc.acc_face = s.skin_key
 WHERE oc.acc_face = 0;

UPDATE owned_character oc
  JOIN drivers d ON d.id = oc.base_key
  JOIN def_kart_skin s ON s.category = 2 AND s.name LIKE CONCAT(d.name, '\_char\_head\_%')
   SET oc.acc_head = s.skin_key
 WHERE oc.acc_head = 0;
