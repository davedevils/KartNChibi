-- rental price tiers each tier points at its own price row so the three tiles show different prices

INSERT INTO shop_price (price_key, unknown_04, unknown_08, unit_type, unit_amount, price_base, price_sale, currency)
VALUES (90001, 0, 0, 0, 0, 1000, 0, 0),
       (90002, 0, 0, 0, 0, 3000, 0, 0)
ON DUPLICATE KEY UPDATE price_base = VALUES(price_base);

UPDATE shop_option SET price_key = 90001 WHERE slot = 0;
UPDATE shop_option SET price_key = 90002 WHERE slot = 1;
-- slot 2 permanent keeps whatever price key the item seed gave it
