-- chibikart hands out the default sky in the tray with only the floor placed idempotent

UPDATE player_item_instance SET placed = 0
 WHERE object_key = 1001 AND category = 0 AND price_key = 0 AND placed = 1;
