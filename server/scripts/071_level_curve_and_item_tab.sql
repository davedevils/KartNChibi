-- 071 the level curve of docs packets LEVEL CURVE md and the Item tab of the official server video

INSERT INTO level_curve (level, cum_exp) VALUES
    (1,0), (2,100), (3,300), (4,600), (5,1000),
    (6,1500), (7,2150), (8,2950), (9,3900), (10,5000),
    (11,6300), (12,7900), (13,9800), (14,12000), (15,14500),
    (16,17350), (17,20800), (18,24700), (19,29100), (20,34100),
    (21,39700), (22,46100), (23,53300), (24,61400), (25,70600),
    (26,81000), (27,92750), (28,106000), (29,121000), (30,137950),
    (31,157150), (32,178800), (33,203300), (34,230950), (35,262250),
    (36,297600), (37,337600), (38,382800), (39,433850), (40,491550),
    (41,556800), (42,630550), (43,713900), (44,808150), (45,914650),
    (46,1035000), (47,1171050), (48,1324850), (49,1498650), (50,1695150),
    (51,1917250), (52,2168250), (53,2452000), (54,2772700), (55,3135200)
ON DUPLICATE KEY UPDATE cum_exp = VALUES(cum_exp);

-- 1 to 17 are GOA totals 18 to 55 grow 13 % a level so level 46 starts at 1035000
DELETE FROM level_curve WHERE level > 55;

-- the new ladder sits under the old one at every level so a level only ever goes up here
UPDATE characters c
  JOIN (
        SELECT c2.id AS cid, MAX(lc.level) AS lvl
          FROM characters c2
          JOIN level_curve lc ON lc.cum_exp <= c2.experience
         GROUP BY c2.id
       ) d ON d.cid = c.id
   SET c.level = LEAST(55, GREATEST(c.level, d.lvl));

UPDATE characters c
  JOIN level_curve lc ON lc.level = c.level
   SET c.exp_floor = lc.cum_exp,
       c.exp_next  = COALESCE(
           (SELECT MIN(n.cum_exp) FROM level_curve n WHERE n.level > c.level),
           lc.cum_exp + 1);

-- the client FDIVP must never see a zero span
UPDATE characters SET exp_next = exp_floor + 1 WHERE exp_next <= exp_floor;
UPDATE characters SET exp_floor = experience WHERE exp_floor > experience;

-- the video sells Slot Exchange in gold by uses 50 uses for 50 105 for 100 220 for 200
INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency) VALUES
  (4201, 0, 0, 2,  50,  50, 0, 0),
  (4202, 0, 0, 2, 105, 100, 0, 0),
  (4203, 0, 0, 2, 220, 200, 0, 0)
ON DUPLICATE KEY UPDATE unit_type = VALUES(unit_type), unit_amount = VALUES(unit_amount),
    price_base = VALUES(price_base), price_sale = VALUES(price_sale), currency = VALUES(currency);

DELETE FROM shop_option WHERE category = 2 AND base_key = 1000;
INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3) VALUES
  (2, 1000, 0, 4201, 2,  50, 0),
  (2, 1000, 1, 4202, 2, 105, 0),
  (2, 1000, 2, 4203, 2, 220, 0);

-- ITEM 1000 TITLE Slot Exchange with the slotchanger icon the client ships in Image Parts
UPDATE def_item_wire SET visible = 1, use_type = 0, display_name_key = 'ITEM_1000_TITLE',
    icon_key = 'slotchanger', description_key = 'ITEM_1000_INFO' WHERE item_key = 1000;
UPDATE shop_definition SET shop_visible_flag = 1, str1_name = 'Slot Exchange'
 WHERE category = 2 AND base_key = 1000;

-- the Gacha Coin and the 50% kit are not on the video tab owned copies keep working
UPDATE def_item_wire SET visible = 0 WHERE item_key IN (2000, 3000);
UPDATE shop_definition SET shop_visible_flag = 0 WHERE category = 2 AND base_key IN (2000, 3000);

-- the 100% Repair Kit keeps its unit type 3 row of 500 which is the full bar of 0x42AD20
UPDATE def_item_wire SET visible = 1, use_type = 5, icon_key = 'repair_full',
    display_name_key = 'ITEM_3001_TITLE', description_key = 'ITEM_3001_INFO' WHERE item_key = 3001;
UPDATE shop_price SET unit_type = 3, unit_amount = 500 WHERE price_key = 4102;
UPDATE def_item_wire SET visible = 1 WHERE item_key = 2001;
