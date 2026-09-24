-- shares 23 price keys across items instead of one per item so the login burst stays small

DELETE FROM shop_option WHERE price_key >= 7000000;
DELETE FROM shop_price  WHERE price_key >= 7000000;

-- band 1 is the cheapest band 7 the dearest
INSERT INTO shop_price (price_key, unit_type, unit_amount, price_base, price_sale, currency)
VALUES
    (1001, 0, 0,  3000, 0, 0), (1002, 0, 0,  6000, 0, 0), (1003, 0, 0, 10000, 0, 0),
    (1004, 0, 0, 16000, 0, 0), (1005, 0, 0, 24000, 0, 0), (1006, 0, 0, 34000, 0, 0),
    (1007, 0, 0, 50000, 0, 0),
    (2001, 1, 1,   300, 0, 0), (2002, 1, 1,   500, 0, 0), (2003, 1, 1,   800, 0, 0),
    (2004, 1, 1,  1200, 0, 0), (2005, 1, 1,  1800, 0, 0), (2006, 1, 1,  2500, 0, 0),
    (2007, 1, 1,  3500, 0, 0),
    (5001, 1, 3,   700, 0, 0), (5002, 1, 3,  1200, 0, 0), (5003, 1, 3,  1900, 0, 0),
    (5004, 1, 3,  2800, 0, 0), (5005, 1, 3,  4200, 0, 0), (5006, 1, 3,  5800, 0, 0),
    (5007, 1, 3,  8000, 0, 0),
    (3001, 1, 7,  1500, 0, 0), (3002, 1, 7,  2500, 0, 0), (3003, 1, 7,  4000, 0, 0),
    (3004, 1, 7,  6000, 0, 0), (3005, 1, 7,  9000, 0, 0), (3006, 1, 7, 12500, 0, 0),
    (3007, 1, 7, 17500, 0, 0)
ON DUPLICATE KEY UPDATE
    unit_type = VALUES(unit_type), unit_amount = VALUES(unit_amount),
    price_base = VALUES(price_base);

-- puts every definition in a band by category or kart speed
DROP TEMPORARY TABLE IF EXISTS band_of;
CREATE TEMPORARY TABLE band_of (
    category TINYINT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL,
    band     TINYINT NOT NULL,
    PRIMARY KEY (category, base_key)
);

INSERT INTO band_of (category, base_key, band)
SELECT 1, k.kart_id, LEAST(7, GREATEST(1, CEIL(CAST(k.stat1_speed AS UNSIGNED) / 3)))
FROM kart_catalog k;

INSERT INTO band_of (category, base_key, band)
SELECT 2, t.id, 2 FROM item_templates t
ON DUPLICATE KEY UPDATE band = VALUES(band);

INSERT INTO band_of (category, base_key, band)
SELECT 4, p.id, 5 FROM pets p
ON DUPLICATE KEY UPDATE band = VALUES(band);

INSERT INTO band_of (category, base_key, band)
SELECT d.category, d.base_key,
       CASE d.category WHEN 0 THEN 4 WHEN 3 THEN 2 WHEN 5 THEN 1 WHEN 6 THEN 3 ELSE 2 END
FROM shop_definition d
WHERE d.category IN (0, 3, 5, 6)
ON DUPLICATE KEY UPDATE band = VALUES(band);

-- slot 0 one day slot 1 three days slot 2 seven days slot 3 permanent
INSERT INTO shop_option (category, base_key, slot, price_key)
SELECT b.category, b.base_key, 0, 2000 + b.band FROM band_of b
UNION ALL SELECT b.category, b.base_key, 1, 5000 + b.band FROM band_of b
UNION ALL SELECT b.category, b.base_key, 2, 3000 + b.band FROM band_of b
UNION ALL SELECT b.category, b.base_key, 3, 1000 + b.band FROM band_of b
ON DUPLICATE KEY UPDATE price_key = VALUES(price_key);

DROP TEMPORARY TABLE IF EXISTS band_of;

-- drops price rows nothing points at since the login burst sends one frame per row
DELETE p FROM shop_price p
WHERE NOT EXISTS (SELECT 1 FROM shop_option o WHERE o.price_key = p.price_key);
