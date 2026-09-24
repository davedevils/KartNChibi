-- 068 the shop on the price rows of the captured burst and the catalogue fixes of the shop audit
-- facts read from server data reference login burst bin its 23 rows of 0x00C6 and every catalogue row
-- the whole audit is docs packets SHOP CATALOGUE md

-- the detail panel 0x45E36B draws the Astro icon beside a sale price and the Gold icon beside a base price
-- so the burst sells every permanent row for 3000 Astro and every rental for gold
-- the game server of this change charges the sale in Astro whatever the currency column says
-- the column follows the same rule here so the transaction log reads right
INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency) VALUES
  (1001, 0, 0, 0, 0,     0, 3000, 1),
  (1002, 0, 0, 0, 0,     0, 3000, 1),
  (1003, 0, 0, 0, 0,     0, 3000, 1),
  (1004, 0, 0, 0, 0,     0, 3000, 1),
  (1005, 0, 0, 0, 0,     0, 3000, 1),
  (1006, 0, 0, 0, 0,     0, 3000, 1),
  (1007, 0, 0, 0, 0,     0, 3000, 1),
  (2001, 0, 0, 1, 1,  1500,    0, 0),
  (2002, 0, 0, 1, 1,  1800,    0, 0),
  (2003, 0, 0, 1, 1,   500,    0, 0),
  (2004, 0, 0, 1, 1,   400,    0, 0),
  (2005, 0, 0, 1, 1,  2500,    0, 0),
  (2006, 0, 0, 1, 1,   300,    0, 0),
  (2007, 0, 0, 1, 1,  1000,    0, 0),
  (3001, 0, 0, 1, 7,  7500,    0, 0),
  (3002, 0, 0, 1, 7,  9000,    0, 0),
  (3003, 0, 0, 1, 7,  2500,    0, 0),
  (3004, 0, 0, 1, 7,  2000,    0, 0),
  (3005, 0, 0, 1, 7, 12500,    0, 0),
  (3006, 0, 0, 1, 7,  1500,    0, 0),
  (3007, 0, 0, 1, 7,  5000,    0, 0),
  (4001, 0, 0, 0, 0,     0, 1000, 1),
  (4002, 0, 0, 0, 0,  2500,    0, 0)
ON DUPLICATE KEY UPDATE
  unknown_04 = VALUES(unknown_04), unknown_08 = VALUES(unknown_08),
  unit_type = VALUES(unit_type), unit_amount = VALUES(unit_amount),
  price_base = VALUES(price_base), price_sale = VALUES(price_sale), currency = VALUES(currency);

-- burst tier keys per category one day 2000 plus tier seven days 3000 plus tier permanent 1000 plus tier
-- driver 1 kart 2 worn part 3 kart part 4 pet 5 room object 6 car craft part 7
-- ours had drivers on tier 4 karts on tier 1 kart parts on tier 2 and a three day tier

-- drivers every definition row the 0xBF builder publishes the enabled ones
DELETE FROM shop_option WHERE category = 0;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 0, base_key, 0, 2001, 1, 1, 0 FROM shop_definition WHERE category = 0;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 0, base_key, 1, 3001, 1, 7, 0 FROM shop_definition WHERE category = 0;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 0, base_key, 2, 1001, 0, 0, 0 FROM shop_definition WHERE category = 0;

-- karts the enabled templates the 0xC0 builder publishes the three disabled ones keep their own rows
DELETE o FROM shop_option o
  JOIN vehicle_templates v ON v.id = o.base_key AND COALESCE(v.is_enabled, 1) = 1
 WHERE o.category = 1;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 1, id, 0, 2002, 1, 1, 0 FROM vehicle_templates WHERE COALESCE(is_enabled, 1) = 1;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 1, id, 1, 3002, 1, 7, 0 FROM vehicle_templates WHERE COALESCE(is_enabled, 1) = 1;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 1, id, 2, 1002, 0, 0, 0 FROM vehicle_templates WHERE COALESCE(is_enabled, 1) = 1;

-- the stock plate is an equip slot 1 row on the burst key 1100 hidden and never sold
-- as slot 7 no garage tab could take it and 0xB9 found no kart field for it
UPDATE def_kart_skin SET category = 1 WHERE skin_key = 9100;
UPDATE shop_definition SET subtype = 1 WHERE category = 3 AND base_key = 9100;

-- the burst sells ANT 19 the War Flag key 1219 ours had no row the client ships its nif
-- its Emerald Tiara key 14213 stays out the client ships no nif and the driver build stops at it
INSERT IGNORE INTO def_kart_skin (skin_key, name, category, display_name) VALUES (9329, 'ant_19', 8, 'PART_1219_TITLE');
INSERT IGNORE INTO shop_definition
  (category, base_key, shop_visible_flag, badge, subtype, level_req, required_pendant_key, str1_name, str2, str3_desc)
VALUES (3, 9329, 1, 0, 8, 0, 0, 'ant_19', 'PART_1219_TITLE', 'PART_1219_INFO');

-- parts every sold definition row the worn ones on tier 3 the paint plate and antenna on tier 4
DELETE FROM shop_option WHERE category = 3;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 3, d.base_key, 0, CASE WHEN s.category = 2 THEN 2003 ELSE 2004 END, 1, 1, 0
    FROM shop_definition d JOIN def_kart_skin s ON s.skin_key = d.base_key
   WHERE d.category = 3 AND d.shop_visible_flag = 1;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 3, d.base_key, 1, CASE WHEN s.category = 2 THEN 3003 ELSE 3004 END, 1, 7, 0
    FROM shop_definition d JOIN def_kart_skin s ON s.skin_key = d.base_key
   WHERE d.category = 3 AND d.shop_visible_flag = 1;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 3, d.base_key, 2, CASE WHEN s.category = 2 THEN 1003 ELSE 1004 END, 0, 0, 0
    FROM shop_definition d JOIN def_kart_skin s ON s.skin_key = d.base_key
   WHERE d.category = 3 AND d.shop_visible_flag = 1;

-- pets tier 5 the three day 5005 row the burst never had goes
DELETE FROM shop_option WHERE category = 4;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 4, base_key, 0, 2005, 1, 1, 0 FROM shop_definition WHERE category = 4;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 4, base_key, 1, 3005, 1, 7, 0 FROM shop_definition WHERE category = 4;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
  SELECT 4, base_key, 2, 1005, 0, 0, 0 FROM shop_definition WHERE category = 4;

-- room objects and car craft parts already carry the burst keys 2006 3006 1006 and 2007 3007 1007
-- their option words now carry the unit and the amount like the burst rows
UPDATE shop_option SET opt_word1 = 1, opt_word2 = 1, opt_word3 = 0 WHERE category IN (5, 6) AND price_key IN (2006, 2007);
UPDATE shop_option SET opt_word1 = 1, opt_word2 = 7, opt_word3 = 0 WHERE category IN (5, 6) AND price_key IN (3006, 3007);
UPDATE shop_option SET opt_word1 = 0, opt_word2 = 0, opt_word3 = 0 WHERE category IN (5, 6) AND price_key IN (1006, 1007);

-- the burst sells two items the Gacha Coin 2000 on 4001 and the Gold Coin 2001 on 4002
-- a coin buy adds one coin to the count the gacha popup spends whatever unit the row prints
DELETE FROM shop_option WHERE category = 2 AND base_key IN (2000, 2001);
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3) VALUES
  (2, 2000, 0, 4001, 0, 0, 0),
  (2, 2001, 0, 4002, 0, 0, 0);
UPDATE shop_definition SET level_req = 0 WHERE category = 2 AND base_key IN (2000, 2001);

-- the 25 race item rows 20001 to 20903 are ours the client ships no icon and no title for them
-- the shop drew 25 blank tiles on them and nothing uses an owned row so they leave the shop
UPDATE def_item_wire SET visible = 0 WHERE item_key BETWEEN 20001 AND 20903;
UPDATE shop_definition SET shop_visible_flag = 0 WHERE category = 2 AND base_key BETWEEN 20001 AND 20903;
DELETE FROM shop_option WHERE category = 2 AND base_key BETWEEN 20001 AND 20903;

-- the slot changer 1000 has no price option in the burst or ours so its row is not for sale
UPDATE def_item_wire SET visible = 0 WHERE item_key = 1000;
UPDATE shop_definition SET shop_visible_flag = 0 WHERE category = 2 AND base_key = 1000;

-- the repair scrolls of migration 060 take the title and info keys the client ships for them
UPDATE def_item_wire SET display_name_key = 'ITEM_3000_TITLE', description_key = 'ITEM_3000_INFO' WHERE item_key = 3000;
UPDATE def_item_wire SET display_name_key = 'ITEM_3001_TITLE', description_key = 'ITEM_3001_INFO' WHERE item_key = 3001;
UPDATE shop_definition SET str2 = 'ITEM_3000_TITLE', str3_desc = 'ITEM_3000_INFO' WHERE category = 2 AND base_key = 3000;
UPDATE shop_definition SET str2 = 'ITEM_3001_TITLE', str3_desc = 'ITEM_3001_INFO' WHERE category = 2 AND base_key = 3001;

-- the price rows only ours carried the coin packs 4003 to 4006 and the three day tier 5001 to 5005
-- no option names them any more and no owned row keeps a price key so they leave the 0x00C6 table
DELETE FROM shop_option WHERE price_key IN (4003, 4004, 4005, 4006, 5001, 5002, 5003, 5004, 5005);
DELETE FROM shop_price WHERE price_key IN (4003, 4004, 4005, 4006, 5001, 5002, 5003, 5004, 5005);

-- PENDANT 01 reads you passed the tutorial so it comes from the rookie licence grade only
-- mission 2 paid it as its item reward and migration 037 gave it to every character of its day
UPDATE mission_def SET reward_item_type = 0, reward_item_key = 0
 WHERE mission_id = 2 AND reward_item_type = 7 AND reward_item_key = 1;
UPDATE characters SET pendant_key = 0 WHERE pendant_key = 1 AND COALESCE(license_class, 0) = 0;
DELETE o FROM owned_pendant o JOIN characters c ON c.id = o.character_id
 WHERE o.pendant_key = 1 AND COALESCE(c.license_class, 0) = 0;
