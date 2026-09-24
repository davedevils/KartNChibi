-- 062 the driver accessory rows the shop could not sell
-- facts read from server data reference login burst bin 473 rows of 0x00C2

-- the burst sells 367 of the 473 part rows every sold row carries three price options
-- slot 0 one day slot 1 seven days slot 2 permanent and the row order is that order
-- equip slot 2 to 6 the driver accessories use 2003 3003 1003
-- equip slot 0 kart colour 1 name plate 8 kart item use 2004 3004 1004
-- the other 106 rows ship visible 0 with one option whose price key is 0 so they never sell
-- every part row of the burst carries required level 0 and badge 0

-- ShopPackets validatePurchase reads shop definition after shop option
-- we had 1463 option rows for 457 part keys but only 57 definition rows the 9001 to 9328 kart parts
-- so every driver accessory answered UnknownDefinition which the client prints as MSG UNKNOWN ERROR
INSERT IGNORE INTO shop_definition
  (category, base_key, shop_visible_flag, badge, subtype, level_req, required_pendant_key,
   str1_name, str2, str3_desc)
SELECT 3, s.skin_key,
       CASE WHEN o.base_key IS NULL THEN 0 ELSE 1 END,
       0, s.category, 0, 0,
       LEFT(s.name, 32),
       LEFT(CASE WHEN s.display_name = '' THEN s.name ELSE s.display_name END, 32),
       LEFT(CASE WHEN s.display_name = '' THEN s.name ELSE s.display_name END, 33)
  FROM def_kart_skin s
  LEFT JOIN (SELECT DISTINCT base_key FROM shop_option WHERE category = 3) o
         ON o.base_key = s.skin_key;

-- the subtype column now carries the def kart skin family so a reader never has to join back
UPDATE shop_definition d
  JOIN def_kart_skin s ON s.skin_key = d.base_key
   SET d.subtype = s.category
 WHERE d.category = 3;

-- the burst gives every part row required level 0 ours carried 1 on the 57 kart part rows
UPDATE shop_definition SET level_req = 0 WHERE category = 3;

-- the same hole on the item tab any visible 0xC1 row without a definition answers UnknownDefinition
-- item 1000 the slot changer is the only one today it has no price option so the client never buys it
INSERT IGNORE INTO shop_definition
  (category, base_key, shop_visible_flag, badge, subtype, level_req, required_pendant_key,
   str1_name, str2, str3_desc)
SELECT 2, i.item_key, i.visible, 0, 0, 0, 0,
       LEFT(i.display_name_key, 32), LEFT(i.display_name_key, 32), LEFT(i.description_key, 33)
  FROM def_item_wire i;

-- the pet rows 5001 to 5004 of the burst are our 10 20 30 40 the definitions and the options are there
-- the repair scrolls 3000 and 3001 of migration 060 already carry a definition an option and a price
