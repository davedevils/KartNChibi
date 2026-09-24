-- starter kart is now unlimited like chibikart it was wrongly limited to 100 uses drawing a bolt bar

UPDATE owned_kart SET period_mode = 0, period_value = 0 WHERE base_key = 10010;
DELETE FROM theme_catalog WHERE theme_id = 0 AND theme_folder = '';
DELETE FROM track_catalog WHERE track_id = 20000000 AND folder_name = '';
-- a license already held counts the tutorial as done so login lands on the lobby
UPDATE characters SET tutorial_completed = 1 WHERE license_class >= 1;
