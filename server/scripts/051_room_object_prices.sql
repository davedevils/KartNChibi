-- FUN 004199D0 room craft tiles never drew the 0x10C row needs price keys 1006 2006 3006 permanent one seven days
INSERT IGNORE INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency) VALUES
  (1006, 0, 0, 0, 0, 4000, 0, 0),
  (2006, 0, 0, 1, 1,  300, 0, 0),
  (3006, 0, 0, 1, 7, 1500, 0, 0);

UPDATE shop_option SET price_key = 1006 WHERE category = 5 AND price_key = 1001;
UPDATE shop_option SET price_key = 2006 WHERE category = 5 AND price_key = 2001;
UPDATE shop_option SET price_key = 3006 WHERE category = 5 AND price_key = 3001;
DELETE FROM shop_option WHERE category = 5 AND price_key = 5001;
