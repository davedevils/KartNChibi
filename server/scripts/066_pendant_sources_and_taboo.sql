-- 066 the pendant sources of the stock text table and the rest of def taboo

-- type 7 of a reward is the pendant sub 47C3C0 removes that key then appends the row
-- key 1000 count 50 went out as instance 1000 pendant 50 and put a phantom pendant on every client
-- PENDANT 01 Training Master reads you passed the tutorial so the third rookie test grants it
-- param 04 and 05 are the def 0x3C and 0x40 the licence clear board draws as the reward icon
UPDATE license_test_def
   SET reward_item_type = 0, reward_item_key = 0, reward_item_count = 0, param_04 = 0, param_05 = 0;
UPDATE license_test_def
   SET reward_item_type = 7, reward_item_key = 1, reward_item_count = 1, param_04 = 7, param_05 = 1
 WHERE license_key = 2;

-- def taboo of Data Eng carries seven single marks the name check refuses the 130 words were already in
INSERT IGNORE INTO banned_words (word, scope) VALUES
    (CONVERT(0xC2AE USING utf8mb4), 'both'),
    (CONVERT(0xC2BC USING utf8mb4), 'both'),
    (CONVERT(0xC2BE USING utf8mb4), 'both'),
    (CONVERT(0xC2B9 USING utf8mb4), 'both'),
    (CONVERT(0xC2B2 USING utf8mb4), 'both'),
    (CONVERT(0xC2B3 USING utf8mb4), 'both'),
    (CONVERT(0xC2BA USING utf8mb4), 'both');

-- mission def 0x18 is the entry fee the Mission Back art labels it and MISSION ENTER asks it
ALTER TABLE mission_def
    MODIFY COLUMN reward_extra INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x18 the entry fee charged by C2S 0x0090 while the row is not cleared';
