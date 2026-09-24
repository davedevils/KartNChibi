-- 060 kart skin keys ability pairs and durability
-- facts read from the captured login burst server data reference login burst bin

-- every 0xC0 kart row of the burst ships 1000 colour and 1100 plate at 0x84 slots 0 and 1
-- the burst owned kart row ships the same pair so the keys are global not per kart
-- ours are 9007 RED and 9100 NAMEBOX NORMAL a zero key makes sub 4510C0 return null and the kart invisible
UPDATE owned_kart SET skin_primary = 9007 WHERE skin_primary = 0;
UPDATE owned_kart SET skin_secondary = 9100 WHERE skin_secondary = 0;

-- a raw insert that skips the keys must still land a visible kart
ALTER TABLE owned_kart
  MODIFY skin_primary INT UNSIGNED NOT NULL DEFAULT 9007
    COMMENT 'rec 0x08 0xC2 part key overlays def 0x84 sub 490A70 resolves it zero makes the kart invisible',
  MODIFY skin_secondary INT UNSIGNED NOT NULL DEFAULT 9100
    COMMENT 'rec 0x0C 0xC2 part key overlays def 0x88 sub 490A70 loads its model zero makes the kart invisible';

-- ability pairs every kart and part row of the burst carries id -1 percent 0 in both pairs
-- kart ability pairs draw 0x42B910 draws a pair when the id is 0 to 25 so -1 hides it
-- no kart on the real wire has an ability so no column the 0xC0 writer sends -1 0 by default

-- durability no burst kart is sold with period mode 3 and no 0xC6 row has unit type 3
-- the burst owned kart is period mode 0 so the values below are the emulator rule
-- kart durability bar draw 0x42AD20 clamps the owned kart 0x30 at 500 that is the max
ALTER TABLE owned_kart
  MODIFY period_mode INT UNSIGNED NOT NULL DEFAULT 3
    COMMENT 'rec 0x2C 0 permanent 1 day number 2 uses 3 durability',
  MODIFY period_value INT UNSIGNED NOT NULL DEFAULT 100
    COMMENT 'rec 0x30 with mode 3 the durability 0 to 500 minus 1 per finished race repair scrolls add capped';

-- rows over the client max cannot come from the shop or a race but an old seed could hold one
UPDATE owned_kart SET period_value = 500 WHERE period_mode = 3 AND period_value > 500;

-- repair scrolls the client ships Image Parts item repair half and item repair full icons
-- 0xC1 use type 4 to 7 opens MSG REPAIR USE in the garage item tab 0x412BC0 on mode 3 karts
INSERT INTO def_item_wire (item_key, visible, badge, use_type, word_10, display_name_key, icon_key, description_key) VALUES
  (3000, 1, 0, 4, 0, 'Repair Scroll Half', 'repair_half', 'Restores 250 kart durability'),
  (3001, 1, 0, 5, 0, 'Repair Scroll Full', 'repair_full', 'Restores 500 kart durability');

-- a price row with unit type 3 sells its unit amount of durability the only rate on the wire
INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency) VALUES
  (4101, 0, 0, 3, 250, 800, 0, 0),
  (4102, 0, 0, 3, 500, 1400, 0, 0);

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3) VALUES
  (2, 3000, 0, 4101, 0, 0, 0),
  (2, 3001, 0, 4102, 0, 0, 0);

INSERT INTO shop_definition (category, base_key, shop_visible_flag, badge, subtype, level_req, required_pendant_key, str1_name, str2, str3_desc) VALUES
  (2, 3000, 1, 0, 0, 1, 0, 'Repair Scroll Half', 'Repair Scroll Half', 'Restores 250 kart durability'),
  (2, 3001, 1, 0, 0, 1, 0, 'Repair Scroll Full', 'Repair Scroll Full', 'Restores 500 kart durability');
