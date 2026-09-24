
ALTER TABLE shop_items
    MODIFY category ENUM('vehicle', 'item', 'accessory', 'special', 'gacha_ticket') NOT NULL,
    ADD COLUMN IF NOT EXISTS gacha_banner_id INT UNSIGNED NULL AFTER template_id;

-- standard draw ticket links to default banner id 1 and the purchase path auto rolls it
INSERT INTO shop_items
    (category, template_id, gacha_banner_id, name, price_gold, price_cash, required_level, stock) VALUES
    ('gacha_ticket', 90001, 1, 'Standard Draw Ticket',  1000, 0, 1, -1),
    ('gacha_ticket', 90002, 2, 'Premium Draw Ticket',      0, 150, 5, -1),
    ('gacha_ticket', 90003, 3, 'Vehicle Gacha Ticket',     0, 300, 10, -1)
ON DUPLICATE KEY UPDATE
    gacha_banner_id = VALUES(gacha_banner_id),
    name = VALUES(name),
    price_gold = VALUES(price_gold),
    price_cash = VALUES(price_cash);

-- matching item templates row so ShopHandler handlePurchase can join category and effect info
INSERT INTO item_templates
    (id, name, category, effect_type, effect_value, drop_weight, rarity, base_price, description) VALUES
    (90001, 'Standard Draw Ticket', 'item', 'gacha_ticket', 1, 0, 'common',    1000, 'Redeem for one roll on the Standard Gacha'),
    (90002, 'Premium Draw Ticket',  'item', 'gacha_ticket', 2, 0, 'uncommon',  1500, 'Redeem for one roll on the Premium Gacha'),
    (90003, 'Vehicle Gacha Ticket', 'item', 'gacha_ticket', 3, 0, 'rare',      3000, 'Redeem for one roll on the Vehicle Gacha')
ON DUPLICATE KEY UPDATE
    description = VALUES(description);

-- client loads pet state from follow NN ini so the server just persists the equipped pet id
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS active_pet_id INT UNSIGNED NULL AFTER equipped_driver_id;

-- trade history is unused since no CMD drives it and stays kept for a future protocol
