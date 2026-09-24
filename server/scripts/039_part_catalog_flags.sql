-- reverse engineered kart part flags visible and badge were copies of the key not real flags band 2 matches 034

UPDATE def_kart_part_wire SET visible = 1 WHERE visible <> 0;
UPDATE def_kart_part_wire SET badge = 0 WHERE badge <> 0;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 3, part_key, 0, 2002 FROM def_kart_part_wire
UNION ALL SELECT 3, part_key, 1, 5002 FROM def_kart_part_wire
UNION ALL SELECT 3, part_key, 2, 3002 FROM def_kart_part_wire
UNION ALL SELECT 3, part_key, 3, 1002 FROM def_kart_part_wire;
