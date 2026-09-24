-- goal or reward a draws as gold below goal or reward b draws as exp above

UPDATE quest_def SET
    goal_or_reward_a = (CASE quest_index % 4 WHEN 0 THEN 300 WHEN 1 THEN 500 WHEN 2 THEN 400 ELSE 800 END) * (theme_id + 1),
    goal_or_reward_b = (CASE quest_index % 4 WHEN 0 THEN 100 WHEN 1 THEN 200 WHEN 2 THEN 150 ELSE 300 END) * (theme_id + 1);
