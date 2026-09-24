-- 061 the Room Craft and Car Craft shop tabs against the captured burst
-- facts read from server data reference login burst bin 35 rows of 0x0108 and 71 of 0x010C
-- shop roomcraft grid draw 0x41A160 skips a tile whose first price option misses its 0x00C6 row

-- the car craft price tier of the burst every 0x0108 row carries 2007 one day 3007 seven days 1007 permanent
INSERT IGNORE INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency) VALUES
  (1007, 0, 0, 0, 0,    0, 3000, 0),
  (2007, 0, 0, 1, 1, 1000,    0, 0),
  (3007, 0, 0, 1, 7, 5000,    0, 0);

-- our part rows pointed at 7001000 and friends which no 0x00C6 row ever names so no tile drew
DELETE FROM carcraft_part_price;
INSERT INTO carcraft_part_price (part_key, row_index, price_table_key, period_type, period_value, active)
  SELECT part_key, 0, 2007, 1, 1, 0 FROM carcraft_part_def;
INSERT INTO carcraft_part_price (part_key, row_index, price_table_key, period_type, period_value, active)
  SELECT part_key, 1, 3007, 1, 7, 0 FROM carcraft_part_def;
INSERT INTO carcraft_part_price (part_key, row_index, price_table_key, period_type, period_value, active)
  SELECT part_key, 2, 1007, 0, 0, 0 FROM carcraft_part_def;

-- the buy verb reads shop option so the three keys must match the def rows the client saw
DELETE FROM shop_option WHERE category = 6;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 6, part_key, 0, 2007, 0, 0, 0 FROM carcraft_part_def;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 6, part_key, 1, 3007, 0, 0, 0 FROM carcraft_part_def;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 6, part_key, 2, 1007, 0, 0, 0 FROM carcraft_part_def;

-- the burst ships 71 room objects ours had 65 the four floors and the two back objects below were missing
-- the burst names these six by their folder since Define Eng def trans index has no title key for them
INSERT IGNORE INTO room_object_def
  (object_key, enabled, badge, category, max_placeable, required_level, asset_folder, name_loc_key, desc_loc_key) VALUES
  (2010, 1, 0, 1, 1, 0, 'Ani_Floor03',  'Ani_Floor03',  'Ani_Floor03'),
  (2011, 1, 0, 1, 1, 0, 'dollhouse01',  'dollhouse01',  'dollhouse01'),
  (2012, 1, 0, 1, 1, 0, 'fence01',      'fence01',      'fence01'),
  (2013, 1, 0, 1, 1, 0, 'floor05',      'floor05',      'floor05'),
  (3003, 1, 0, 2, 1, 0, '_mountain01',  '_mountain01',  '_mountain01'),
  (3004, 1, 0, 2, 1, 0, '_waterfall01', '_waterfall01', '_waterfall01');

-- every room object of the burst that sells carries the same triple 2006 3006 1006
INSERT IGNORE INTO room_object_price (object_key, slot_index, currency_key, period_type, period_value, extra) VALUES
  (2010, 0, 2006, 1, 1, 0), (2010, 1, 3006, 1, 7, 0), (2010, 2, 1006, 0, 0, 0),
  (2011, 0, 2006, 1, 1, 0), (2011, 1, 3006, 1, 7, 0), (2011, 2, 1006, 0, 0, 0),
  (2012, 0, 2006, 1, 1, 0), (2012, 1, 3006, 1, 7, 0), (2012, 2, 1006, 0, 0, 0),
  (2013, 0, 2006, 1, 1, 0), (2013, 1, 3006, 1, 7, 0), (2013, 2, 1006, 0, 0, 0),
  (3003, 0, 2006, 1, 1, 0), (3003, 1, 3006, 1, 7, 0), (3003, 2, 1006, 0, 0, 0),
  (3004, 0, 2006, 1, 1, 0), (3004, 1, 3006, 1, 7, 0), (3004, 2, 1006, 0, 0, 0);

-- the shop needs the definition row for the level gate and the option rows for the buy verb
INSERT IGNORE INTO shop_definition
  (category, base_key, shop_visible_flag, badge, subtype, level_req, required_pendant_key, str1_name, str2, str3_desc)
  SELECT 5, object_key, 1, 0, 0, 1, 0, asset_folder, name_loc_key, desc_loc_key
    FROM room_object_def WHERE object_key IN (2010, 2011, 2012, 2013, 3003, 3004);

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 5, object_key, slot_index, currency_key, 0, 0, 0
    FROM room_object_price WHERE object_key IN (2010, 2011, 2012, 2013, 3003, 3004);
