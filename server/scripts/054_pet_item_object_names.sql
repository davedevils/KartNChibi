-- pets renamed to real client keys PET 10 Rosie PET 20 Chai PET 30 Porki PET 40 Dim Dim
UPDATE shop_definition SET base_key = base_key * 10 WHERE category = 4 AND base_key BETWEEN 1 AND 4;
UPDATE shop_option     SET base_key = base_key * 10 WHERE category = 4 AND base_key BETWEEN 1 AND 4;
UPDATE owned_pet       SET base_key = base_key * 10 WHERE base_key BETWEEN 1 AND 4;
UPDATE shop_definition SET str2 = CONCAT('PET_', base_key, '_TITLE'), str3_desc = CONCAT('PET_', base_key, '_INFO')
 WHERE category = 4 AND base_key IN (10, 20, 30, 40);

-- the slot exchange has a client text but the race items only read their icon name raw
UPDATE def_item_wire SET display_name_key = 'ITEM_1000_TITLE', description_key = 'ITEM_1000_INFO', icon_key = 'slotchanger'
 WHERE item_key = 1000;
UPDATE def_item_wire SET display_name_key = icon_key, description_key = icon_key
 WHERE item_key BETWEEN 20000 AND 29999 AND icon_key <> '';

-- four room objects with no client text so the folder name is used instead
UPDATE room_object_def SET name_loc_key = 'Panther', desc_loc_key = 'Panther' WHERE object_key = 4009;
UPDATE room_object_def SET name_loc_key = 'Book',    desc_loc_key = 'Book'    WHERE object_key = 4018;
UPDATE room_object_def SET name_loc_key = 'Book 2',  desc_loc_key = 'Book 2'  WHERE object_key = 4022;
UPDATE room_object_def SET name_loc_key = 'Flower',  desc_loc_key = 'Flower'  WHERE object_key = 4030;
