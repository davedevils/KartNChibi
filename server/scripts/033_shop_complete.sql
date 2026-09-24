-- karts items and pets were unpriced so those shop tabs opened empty this prices them across four tiers

-- ---------------------------------------------------------------- karts
INSERT IGNORE INTO shop_definition
    (category, base_key, shop_visible_flag, badge, subtype, level_req,
     unlock_condition_key, str1_name, str2, str3_desc)
SELECT 1, k.kart_id, 1, 0, 0,
       -- a faster kart asks for a little more time in the game
       LEAST(30, GREATEST(1, CAST(k.stat1_speed AS UNSIGNED))),
       0,
       LEFT(k.model_name, 32),
       LEFT(COALESCE(NULLIF(k.name2, ''), k.model_name), 32),
       LEFT(COALESCE(NULLIF(k.name3, ''), k.model_name), 33)
FROM kart_catalog k;

INSERT IGNORE INTO shop_price
    (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 7100000 + k.kart_id, 1, 1,
       GREATEST(300,  CAST(k.stat1_speed AS UNSIGNED) * 120), 0, 0 FROM kart_catalog k
UNION ALL
SELECT 7200000 + k.kart_id, 1, 3,
       GREATEST(800,  CAST(k.stat1_speed AS UNSIGNED) * 300), 0, 0 FROM kart_catalog k
UNION ALL
SELECT 7300000 + k.kart_id, 1, 7,
       GREATEST(1600, CAST(k.stat1_speed AS UNSIGNED) * 600), 0, 0 FROM kart_catalog k;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 1, k.kart_id, 0, 7100000 + k.kart_id FROM kart_catalog k
UNION ALL SELECT 1, k.kart_id, 1, 7200000 + k.kart_id FROM kart_catalog k
UNION ALL SELECT 1, k.kart_id, 2, 7300000 + k.kart_id FROM kart_catalog k;

-- ---------------------------------------------------------------- items
INSERT IGNORE INTO shop_definition
    (category, base_key, shop_visible_flag, badge, subtype, level_req,
     unlock_condition_key, str1_name, str2, str3_desc)
SELECT 2, t.id, 1, 0, 0, 1, 0,
       LEFT(t.name, 32), LEFT(t.name, 32), LEFT(t.name, 33)
FROM item_templates t;

INSERT IGNORE INTO shop_price
    (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 7210000 + t.id, 1, 1, 200,  0, 0 FROM item_templates t
UNION ALL SELECT 7220000 + t.id, 1, 3, 500,  0, 0 FROM item_templates t
UNION ALL SELECT 7230000 + t.id, 1, 7, 1000, 0, 0 FROM item_templates t;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 2, t.id, 0, 7210000 + t.id FROM item_templates t
UNION ALL SELECT 2, t.id, 1, 7220000 + t.id FROM item_templates t
UNION ALL SELECT 2, t.id, 2, 7230000 + t.id FROM item_templates t;

-- ---------------------------------------------------------------- pets
INSERT IGNORE INTO shop_definition
    (category, base_key, shop_visible_flag, badge, subtype, level_req,
     unlock_condition_key, str1_name, str2, str3_desc)
SELECT 4, p.id, 1, 0, 0, 5, 0,
       LEFT(p.name, 32), LEFT(p.name, 32), LEFT(p.name, 33)
FROM pets p;

INSERT IGNORE INTO shop_price
    (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 7410000 + p.id, 1, 1, 1500, 0, 0 FROM pets p
UNION ALL SELECT 7420000 + p.id, 1, 3, 3500, 0, 0 FROM pets p
UNION ALL SELECT 7430000 + p.id, 1, 7, 6000, 0, 0 FROM pets p;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 4, p.id, 0, 7410000 + p.id FROM pets p
UNION ALL SELECT 4, p.id, 1, 7420000 + p.id FROM pets p
UNION ALL SELECT 4, p.id, 2, 7430000 + p.id FROM pets p;

-- permanent tiers for the three new categories slot 3 unit type 0 ignores amount
INSERT IGNORE INTO shop_price
    (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 7400000 + k.kart_id, 0, 0,
       GREATEST(6000, CAST(k.stat1_speed AS UNSIGNED) * 2400), 0, 0 FROM kart_catalog k
UNION ALL SELECT 7240000 + t.id, 0, 0, 4000,  0, 0 FROM item_templates t
UNION ALL SELECT 7440000 + p.id, 0, 0, 24000, 0, 0 FROM pets p;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT 1, k.kart_id, 3, 7400000 + k.kart_id FROM kart_catalog k
UNION ALL SELECT 2, t.id, 3, 7240000 + t.id FROM item_templates t
UNION ALL SELECT 4, p.id, 3, 7440000 + p.id FROM pets p;

-- permanent price for what already sold four times the dearest existing tier floored
INSERT IGNORE INTO shop_price
    (price_key, unit_type, unit_amount, price_base, price_sale, currency)
SELECT 7800000 + d.category * 100000 + d.base_key, 0, 0,
       GREATEST(5000, COALESCE(MAX(p.price_base), 1000) * 4), 0, 0
FROM shop_definition d
LEFT JOIN shop_option o ON o.category = d.category AND o.base_key = d.base_key
LEFT JOIN shop_price  p ON p.price_key = o.price_key
WHERE d.category IN (0, 3, 5, 6)
GROUP BY d.category, d.base_key;

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key)
SELECT d.category, d.base_key, 3, 7800000 + d.category * 100000 + d.base_key
FROM shop_definition d
WHERE d.category IN (0, 3, 5, 6)
  AND NOT EXISTS (SELECT 1 FROM shop_option o2
                  WHERE o2.category = d.category AND o2.base_key = d.base_key AND o2.slot = 3);

-- the item wire table came from a single row so every consumable and template is mirrored into it
INSERT IGNORE INTO def_item_wire
    (item_key, visible, badge, use_type, word_10, display_name_key, icon_key, description_key)
SELECT t.id, 1, 0, 0, 0,
       CONCAT('ITEM_', t.id),
       LEFT(COALESCE(NULLIF(t.asset_name, ''), CONCAT('item_', t.id)), 36),
       CONCAT('ITEM_', t.id, '_INFO')
FROM item_templates t
WHERE t.category IN ('item', 'consumable', 'accessory');
