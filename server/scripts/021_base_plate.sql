-- sub 00418C80 shows a plate nif always hangs on O NAME since BODY nif has no plate geometry
INSERT INTO def_kart_skin (skin_key, category, name, display_name)
VALUES (9100, 7, 'NAMEBOX_NORMAL', 'Standard Plate')
ON DUPLICATE KEY UPDATE category = VALUES(category), name = VALUES(name),
                        display_name = VALUES(display_name);

ALTER TABLE owned_kart MODIFY skin_secondary INT UNSIGNED NOT NULL DEFAULT 9100;

-- 9101 and 9102 were only handed out by our own defaulting so give those karts back the stock plate
UPDATE owned_kart SET skin_secondary = 9100 WHERE skin_secondary IN (0, 9101, 9102);
