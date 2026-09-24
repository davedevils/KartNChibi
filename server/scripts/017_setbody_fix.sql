-- 017 stops set body fail number 1

-- paint key at kart record offset 0x08 sub 4510C0 and sub 490A70 look it up skin key 9001 or above
ALTER TABLE owned_kart
  MODIFY skin_primary   INT UNSIGNED NOT NULL DEFAULT 9001,
  MODIFY skin_secondary INT UNSIGNED NOT NULL DEFAULT 9101;

UPDATE owned_kart SET skin_primary   = 9001 WHERE skin_primary   = 0;
UPDATE owned_kart SET skin_secondary = 9101 WHERE skin_secondary = 0;

-- these three templates have no model on disk at all so any kart pointing at them fails the body build
UPDATE vehicle_templates SET is_enabled = 0 WHERE id IN (1001, 10001, 12001);

-- anything still pointing at a dead template moves to basic 1 which ships
UPDATE owned_kart SET base_key = 10010 WHERE base_key IN (1001, 10001, 12001);
UPDATE vehicles SET vehicle_type_id = 10010 WHERE vehicle_type_id IN (1001, 10001, 12001);
