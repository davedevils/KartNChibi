-- seeds the two missing rental slots reusing the same price key so all three tiles share one price

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT category, base_key, 1 AS slot, price_key, 0, 0, 0
FROM shop_option WHERE slot = 0
ON DUPLICATE KEY UPDATE price_key = VALUES(price_key);

INSERT INTO shop_option (category, base_key, slot, price_key, opt_word1, opt_word2, opt_word3)
SELECT category, base_key, 2 AS slot, price_key, 0, 0, 0
FROM shop_option WHERE slot = 0
ON DUPLICATE KEY UPDATE price_key = VALUES(price_key);
