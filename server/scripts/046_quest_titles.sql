-- fixes quest titles that used keys the lookup never found drawing an empty list

UPDATE quest_def SET str_key_title = CONCAT('QUEST_0', (quest_index % 4) + 1, '_TITLE');
UPDATE quest_def SET goal_count = CASE quest_index % 4 WHEN 0 THEN 3 WHEN 1 THEN 10 WHEN 2 THEN 3 ELSE 1 END;
