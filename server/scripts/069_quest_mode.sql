-- 069 the quest mode of the stock client stage 22 ScenarioMenu and stage 17 Quest game
-- every column below is a field of the 156 byte S2C 0x00F3 record or a server rule
-- the flow and the readers are in docs packets systems scenario md

ALTER TABLE scenario_def
    ADD COLUMN IF NOT EXISTS track_id INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'def 0x0C 0xC3 track the detail thumbnail and the stage 17 world',
    ADD COLUMN IF NOT EXISTS entry_fee INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'def 0x1C MSG QUEST PAY charged by C2S 0xF5 while the row is not cleared',
    ADD COLUMN IF NOT EXISTS reward_key INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'def 0x2C catalogue key of the reward category 7 is a pendant key',
    ADD COLUMN IF NOT EXISTS title_key VARCHAR(32) NOT NULL DEFAULT ''
        COMMENT 'def 0x38 def quest index key of the list row title',
    ADD COLUMN IF NOT EXISTS info_key VARCHAR(32) NOT NULL DEFAULT ''
        COMMENT 'def 0x59 def quest index key of the detail panel line',
    ADD COLUMN IF NOT EXISTS story_key VARCHAR(27) NOT NULL DEFAULT ''
        COMMENT 'def 0x7A def quest index key the HUD appends START SUCCESS and FAIL to',
    ADD COLUMN IF NOT EXISTS required_level INT UNSIGNED NOT NULL DEFAULT 1
        COMMENT 'server only the row is listed from this character level a new quest every level',
    ADD COLUMN IF NOT EXISTS goal_time_ms INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'server only zero means any finish wins when no rival ghost runs',
    ADD COLUMN IF NOT EXISTS is_enabled TINYINT(1) NOT NULL DEFAULT 1
        COMMENT 'server only zero keeps the row off the wire';

ALTER TABLE scenario_def
    MODIFY COLUMN reward_category INT UNSIGNED NOT NULL DEFAULT 8
        COMMENT 'def 0x28 0 to 7 picks the icon and the 0x00F8 kind 3 tail 8 means no item',
    MODIFY COLUMN gold_reward INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'def 0x20 UNIT MILEAGE gold paid once on the first clear',
    MODIFY COLUMN exp_reward INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'def 0x24 UNIT EXP exp paid once on the first clear';

ALTER TABLE scenario_key_progress
    ADD COLUMN IF NOT EXISTS best_time_ms INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'best winning race time C2S 0xF8 reports',
    ADD COLUMN IF NOT EXISTS attempts INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'result reports received for this quest',
    ADD COLUMN IF NOT EXISTS cleared_at TIMESTAMP NULL DEFAULT NULL
        COMMENT 'the first clear the only one that pays';

-- the twenty quests of def quest index the rival and the place come from each story text
-- Frankie monster 13 Morrigan witch 11 Peekay pumpkin 10 Buttercup princess 9 Gragg wolf 12
-- Brag yuk 14 Cleo mummy 5 no Prince his 18 letter name overruns the rival name buffer
-- quest 5 10 15 20 free Rosie Chai Porki and Dim Dim and give pendant 2 3 4 5
INSERT INTO scenario_def
    (scenario_key, track_id, character_def_key, kart_def_key, entry_fee, gold_reward, exp_reward,
     reward_category, reward_key, title_key, info_key, story_key, required_level, goal_time_ms, is_enabled)
VALUES
    ( 1, 10, 13, 13008,   0,  300,  200, 8, 0, 'Quest_01_TITLE', 'Quest_01_Info', 'QUEST_01_STORY',  1, 0, 1),
    ( 2, 11, 11, 13004,  50,  400,  300, 8, 0, 'Quest_02_TITLE', 'Quest_02_Info', 'QUEST_02_STORY',  2, 0, 1),
    ( 3, 20, 10, 13002, 100,  500,  400, 8, 0, 'Quest_03_TITLE', 'Quest_03_Info', 'QUEST_03_STORY',  3, 0, 1),
    ( 4, 70,  9, 13007, 150,  600,  500, 8, 0, 'Quest_04_TITLE', 'Quest_04_Info', 'QUEST_04_STORY',  4, 0, 1),
    ( 5, 50, 12, 13005, 200,  700,  600, 7, 2, 'Quest_05_TITLE', 'Quest_05_Info', 'QUEST_05_STORY',  5, 0, 1),
    ( 6, 21, 10, 10015, 250,  800,  700, 8, 0, 'Quest_06_TITLE', 'Quest_06_Info', 'QUEST_06_STORY',  6, 0, 1),
    ( 7, 71,  9, 13007, 300,  900,  800, 8, 0, 'Quest_07_TITLE', 'Quest_07_Info', 'QUEST_07_STORY',  7, 0, 1),
    ( 8, 12, 13, 13008, 350, 1000,  900, 8, 0, 'Quest_08_TITLE', 'Quest_08_Info', 'QUEST_08_STORY',  8, 0, 1),
    ( 9, 80, 11, 13004, 400, 1100, 1000, 8, 0, 'Quest_09_TITLE', 'Quest_09_Info', 'QUEST_09_STORY',  9, 0, 1),
    (10, 51, 12, 13005, 450, 1200, 1100, 7, 3, 'Quest_10_TITLE', 'Quest_10_Info', 'QUEST_10_STORY', 10, 0, 1),
    (11, 40, 13, 13008, 500, 1300, 1200, 8, 0, 'Quest_11_TITLE', 'Quest_11_Info', 'QUEST_11_STORY', 11, 0, 1),
    (12, 60, 14, 13009, 550, 1400, 1300, 8, 0, 'Quest_12_TITLE', 'Quest_12_Info', 'QUEST_12_STORY', 12, 0, 1),
    (13, 61,  9, 13007, 600, 1500, 1400, 8, 0, 'Quest_13_TITLE', 'Quest_13_Info', 'QUEST_13_STORY', 13, 0, 1),
    (14, 30,  5, 13003, 650, 1600, 1500, 8, 0, 'Quest_14_TITLE', 'Quest_14_Info', 'QUEST_14_STORY', 14, 0, 1),
    (15, 41, 13, 13008, 700, 1700, 1600, 7, 4, 'Quest_15_TITLE', 'Quest_15_Info', 'QUEST_15_STORY', 15, 0, 1),
    (16, 42, 13, 13008, 750, 1800, 1700, 8, 0, 'Quest_16_TITLE', 'Quest_16_Info', 'QUEST_16_STORY', 16, 0, 1),
    (17, 32,  5, 13003, 800, 1900, 1800, 8, 0, 'Quest_17_TITLE', 'Quest_17_Info', 'QUEST_17_STORY', 17, 0, 1),
    (18, 81, 13, 13006, 850, 2000, 1900, 8, 0, 'Quest_18_TITLE', 'Quest_18_Info', 'QUEST_18_STORY', 18, 0, 1),
    (19, 13,  9, 13007, 900, 2100, 2000, 8, 0, 'Quest_19_TITLE', 'Quest_19_Info', 'QUEST_19_STORY', 19, 0, 1),
    (20, 52, 12, 13005, 950, 2200, 2100, 7, 5, 'Quest_20_TITLE', 'Quest_20_Info', 'QUEST_20_STORY', 20, 0, 1)
ON DUPLICATE KEY UPDATE
    track_id = VALUES(track_id), character_def_key = VALUES(character_def_key),
    kart_def_key = VALUES(kart_def_key), entry_fee = VALUES(entry_fee),
    gold_reward = VALUES(gold_reward), exp_reward = VALUES(exp_reward),
    reward_category = VALUES(reward_category), reward_key = VALUES(reward_key),
    title_key = VALUES(title_key), info_key = VALUES(info_key), story_key = VALUES(story_key),
    required_level = VALUES(required_level), goal_time_ms = VALUES(goal_time_ms),
    is_enabled = VALUES(is_enabled);

-- the rival ghost of a quest is the best replay of its own track so every key names that track
-- GhostHandler sendQuestGhost reads this row on C2S 0xF5 and a wrong track drives the ghost off the road
INSERT INTO ghost_quest_replay (quest_index, track_id, char_id, is_enabled) VALUES
    ( 1, 10, 0, 1), ( 2, 11, 0, 1), ( 3, 20, 0, 1), ( 4, 70, 0, 1), ( 5, 50, 0, 1),
    ( 6, 21, 0, 1), ( 7, 71, 0, 1), ( 8, 12, 0, 1), ( 9, 80, 0, 1), (10, 51, 0, 1),
    (11, 40, 0, 1), (12, 60, 0, 1), (13, 61, 0, 1), (14, 30, 0, 1), (15, 41, 0, 1),
    (16, 42, 0, 1), (17, 32, 0, 1), (18, 81, 0, 1), (19, 13, 0, 1), (20, 52, 0, 1)
ON DUPLICATE KEY UPDATE track_id = VALUES(track_id), char_id = VALUES(char_id),
    is_enabled = VALUES(is_enabled);
