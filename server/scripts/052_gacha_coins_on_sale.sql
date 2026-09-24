-- the two gacha coins the client names itself the roll handler only accepts owned rows on keys 2000 and 2001

INSERT IGNORE INTO def_item_wire
    (item_key, visible, badge, use_type, word_10, display_name_key, icon_key, description_key)
VALUES
    (2000, 1, 0, 0, 0, 'ITEM_2000_TITLE', 'gacha_coin',  'ITEM_2000_INFO'),
    (2001, 1, 0, 0, 0, 'ITEM_2001_TITLE', 'gacha_coin2', 'ITEM_2001_INFO');

INSERT IGNORE INTO shop_definition
    (category, base_key, shop_visible_flag, badge, subtype, level_req, unlock_condition_key, str1_name, str2, str3_desc)
VALUES
    (2, 2000, 1, 0, 0, 1, 0, 'Gacha Coin', 'ITEM_2000_TITLE', 'ITEM_2000_INFO'),
    (2, 2001, 1, 0, 0, 1, 0, 'Gold Coin',  'ITEM_2001_TITLE', 'ITEM_2001_INFO');

-- unit type 2 is a count the purchase adds it to the coins already held
INSERT IGNORE INTO shop_price
    (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency)
VALUES
    (4001, 0, 0, 2,  1,  1000, 0, 0),
    (4002, 0, 0, 2,  5,  4500, 0, 0),
    (4003, 0, 0, 2, 10,  8000, 0, 0),
    (4004, 0, 0, 2,  1,  3000, 0, 0),
    (4005, 0, 0, 2,  5, 13500, 0, 0),
    (4006, 0, 0, 2, 10, 24000, 0, 0);

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
VALUES
    (2, 2000, 0, 4001, 2,  1, 0),
    (2, 2000, 1, 4002, 2,  5, 0),
    (2, 2000, 2, 4003, 2, 10, 0),
    (2, 2001, 0, 4004, 2,  1, 0),
    (2, 2001, 1, 4005, 2,  5, 0),
    (2, 2001, 2, 4006, 2, 10, 0);

-- the draw tickets nobody could redeem
DELETE FROM shop_option     WHERE category = 2 AND base_key IN (90001, 90002, 90003);
DELETE FROM shop_definition WHERE category = 2 AND base_key IN (90001, 90002, 90003);
DELETE FROM def_item_wire   WHERE item_key IN (90001, 90002, 90003);

-- the item tile reads its options in slot order
DELETE FROM shop_option WHERE category = 2 AND price_key = 5002;
