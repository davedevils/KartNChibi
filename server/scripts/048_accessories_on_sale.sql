-- puts every accessory on sale since most def kart skin rows had no purchase options

INSERT IGNORE INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 3, s.skin_key, 0, IF(s.name LIKE '%\_char\_body\_%', 2002, 2001), 0, 0, 0 FROM def_kart_skin s WHERE s.name LIKE '%\_char\_%';
INSERT IGNORE INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 3, s.skin_key, 1, IF(s.name LIKE '%\_char\_body\_%', 3002, 3001), 0, 0, 0 FROM def_kart_skin s WHERE s.name LIKE '%\_char\_%';
INSERT IGNORE INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT 3, s.skin_key, 2, IF(s.name LIKE '%\_char\_body\_%', 1002, 1001), 0, 0, 0 FROM def_kart_skin s WHERE s.name LIKE '%\_char\_%';
