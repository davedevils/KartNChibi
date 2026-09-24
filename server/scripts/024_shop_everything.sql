-- categories accessory room craft and car craft had no shop rows so purchase failed and tiles froze

-- category 7 is the stock plate and hidden car craft parts neither is meant to be sold
DELETE FROM shop_price      WHERE price_key BETWEEN 5000000 AND 5999999;
DELETE FROM shop_option     WHERE category = 3;
DELETE FROM shop_definition WHERE category = 3;

INSERT INTO shop_definition (category, base_key, shop_visible_flag, badge, subtype,
                             level_req, unlock_condition_key, str1_name, str2, str3_desc)
SELECT 3, skin_key, 1, 0, category, 1, 0,
       name,
       CASE WHEN COALESCE(display_name,'') <> '' THEN display_name ELSE name END,
       CASE WHEN COALESCE(display_name,'') <> '' THEN display_name ELSE name END
FROM def_kart_skin WHERE category <> 7;

INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount,
                        price_base, price_sale, currency)
SELECT 5000000 + skin_key, 0, 0, 0, 0, 1000, 0, 0
FROM def_kart_skin WHERE category <> 7;

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 3, skin_key, 0, 5000000 + skin_key, 0, 0, 0
FROM def_kart_skin WHERE category <> 7;

-- ---------------------------------------------------------------- room craft
DELETE FROM shop_price      WHERE price_key BETWEEN 6000000 AND 6999999;
DELETE FROM shop_option     WHERE category = 5;
DELETE FROM shop_definition WHERE category = 5;
DELETE FROM room_object_price;

INSERT INTO shop_definition (category, base_key, shop_visible_flag, badge, subtype,
                             level_req, unlock_condition_key, str1_name, str2, str3_desc)
SELECT 5, object_key, 1, 0, category, 1, 0, asset_folder, name_loc_key, desc_loc_key
FROM room_object_def;

INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount,
                        price_base, price_sale, currency)
SELECT 6000000 + object_key, 0, 0, 0, 0, 500, 0, 0 FROM room_object_def;

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 5, object_key, 0, 6000000 + object_key, 0, 0, 0 FROM room_object_def;

-- the 0x010C definition carries its own price sublist an empty one is the unguarded deref
INSERT INTO room_object_price (object_key, slot_index, currency_key, period_type,
                               period_value, extra)
SELECT object_key, 0, 6000000 + object_key, 0, 0, 1 FROM room_object_def;

-- ---------------------------------------------------------------- car craft
DELETE FROM shop_price      WHERE price_key BETWEEN 7000000 AND 7999999;
DELETE FROM shop_option     WHERE category = 6;
DELETE FROM shop_definition WHERE category = 6;
DELETE FROM carcraft_part_price;

INSERT INTO shop_definition (category, base_key, shop_visible_flag, badge, subtype,
                             level_req, unlock_condition_key, str1_name, str2, str3_desc)
SELECT 6, part_key, 1, 0, category, 1, 0, model_dir_name, display_name_key, description_key
FROM carcraft_part_def;

INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount,
                        price_base, price_sale, currency)
SELECT 7000000 + part_key, 0, 0, 0, 0, 2000, 0, 0 FROM carcraft_part_def;

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 6, part_key, 0, 7000000 + part_key, 0, 0, 0 FROM carcraft_part_def;

INSERT INTO carcraft_part_price (part_key, row_index, price_table_key, period_type,
                                 period_value, active)
SELECT part_key, 0, 7000000 + part_key, 0, 0, 1 FROM carcraft_part_def;
