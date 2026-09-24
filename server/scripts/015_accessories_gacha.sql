-- def kart skin feeds the 0xC2 container category is the shop tab id read by sub 00418c80

DELETE FROM def_kart_skin WHERE skin_key BETWEEN 9107 AND 9199;
DELETE FROM def_kart_skin WHERE skin_key BETWEEN 9301 AND 9399;

INSERT INTO def_kart_skin (skin_key, category, name) VALUES
  (9107, 1, 'NAMEBOX_007'),
  (9108, 1, 'NAMEBOX_008'),
  (9109, 1, 'NAMEBOX_009'),
  (9110, 1, 'NAMEBOX_010'),
  (9111, 1, 'NAMEBOX_011'),
  (9112, 1, 'NAMEBOX_012'),
  (9113, 1, 'NAMEBOX_013'),
  (9114, 1, 'NAMEBOX_014'),
  (9115, 1, 'NAMEBOX_015'),
  (9116, 1, 'NAMEBOX_016'),
  (9117, 1, 'NAMEBOX_017'),
  (9118, 1, 'NAMEBOX_018'),
  (9119, 1, 'NAMEBOX_019'),
  (9120, 1, 'NAMEBOX_020'),
  (9301, 8, 'ANT_01'),
  (9302, 8, 'ANT_02'),
  (9303, 8, 'ANT_03'),
  (9304, 8, 'ANT_04'),
  (9305, 8, 'ANT_05'),
  (9306, 8, 'ANT_06'),
  (9307, 8, 'ANT_07'),
  (9308, 8, 'ANT_08'),
  (9309, 8, 'ANT_10'),
  (9310, 8, 'ANT_11'),
  (9311, 8, 'ANT_12'),
  (9312, 8, 'ANT_13'),
  (9313, 8, 'ANT_14'),
  (9314, 8, 'ANT_15'),
  (9315, 8, 'ANT_16'),
  (9316, 8, 'ANT_17'),
  (9317, 8, 'ANT_18'),
  (9318, 8, 'ANT_23'),
  (9319, 8, 'ANT_24'),
  (9320, 8, 'ANT_25'),
  (9321, 8, 'ANT_26'),
  (9322, 8, 'ANT_27'),
  (9323, 8, 'ANT_28'),
  (9324, 8, 'ANT_29'),
  (9325, 8, 'ANT_30'),
  (9326, 8, 'ANT_31'),
  (9327, 8, 'ANT_32'),
  (9328, 8, 'ANT_33')
ON DUPLICATE KEY UPDATE category = VALUES(category), name = VALUES(name);

-- gacha prize category is the reveal switch in sub 00456e60 body parts sat on 3 with no art
UPDATE gacha_items SET prize_category = 5 WHERE prize_category = 3;

-- a handful of real accessories so category 3 pays something with a picture
DELETE FROM gacha_items WHERE label LIKE 'accessory %';
INSERT INTO gacha_items (gacha_id, ticket_base_key, weight, rare_flag, prize_category, prize_base_key, period_mode, period_value, rarity, label, enabled) VALUES
  (1, 2000, 60, 0, 3, 9101, 0, 0, 'common', 'accessory NAMEBOX_001', 1),
  (1, 2000, 60, 0, 3, 9102, 0, 0, 'common', 'accessory NAMEBOX_002', 1),
  (1, 2000, 50, 0, 3, 9103, 0, 0, 'common', 'accessory NAMEBOX_003', 1),
  (1, 2000, 60, 0, 3, 9301, 0, 0, 'common', 'accessory ANT_01', 1),
  (1, 2000, 50, 0, 3, 9302, 0, 0, 'common', 'accessory ANT_02', 1),
  (1, 2000, 40, 0, 3, 9303, 0, 0, 'common', 'accessory ANT_03', 1),
  (2, 2001, 50, 0, 3, 9110, 0, 0, 'common', 'accessory NAMEBOX_010', 1),
  (2, 2001, 40, 0, 3, 9115, 0, 0, 'common', 'accessory NAMEBOX_015', 1),
  (2, 2001, 50, 0, 3, 9310, 0, 0, 'common', 'accessory ANT_11', 1),
  (2, 2001, 40, 0, 3, 9320, 0, 0, 'common', 'accessory ANT_25', 1),
  (2, 2001, 40, 0, 3, 9001, 0, 0, 'common', 'accessory BLACK paint', 1),
  (2, 2001, 40, 0, 3, 9007, 0, 0, 'common', 'accessory RED paint', 1);
