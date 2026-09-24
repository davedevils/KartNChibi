-- Packet field names settled on the client bytes wire and values unchanged only names and comments move

-- owned rows the fourth dword after the slots is the 0xC6 price row key sub 45E3D0 and sub 415FF0 resolve it through sub 451E00
ALTER TABLE owned_character RENAME COLUMN unk_1c TO price_key;
ALTER TABLE owned_character MODIFY COLUMN price_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x1C 0xC6 price row key the shop and garage panels draw the price from it';

ALTER TABLE owned_kart RENAME COLUMN unk_28 TO price_key;
ALTER TABLE owned_kart MODIFY COLUMN price_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x28 0xC6 price row key the shop and garage panels draw the price from it';
ALTER TABLE owned_kart MODIFY COLUMN skin_primary INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x08 0xC2 part key overlays def 0x84 sub 490A70 resolves it in the part catalog';
ALTER TABLE owned_kart MODIFY COLUMN skin_secondary INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x0C 0xC2 part key overlays def 0x88 sub 490A70 loads its model';
ALTER TABLE owned_kart MODIFY COLUMN skin_tertiary INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x10 0xC2 part key overlays def 0x8C sub 482DB0 reports a use when its owned row has period mode 2';
ALTER TABLE owned_kart MODIFY COLUMN custom3 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x14 overlays def 0x90 no reader in this build';
ALTER TABLE owned_kart MODIFY COLUMN custom4 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x18 overlays def 0x94 no reader in this build';
ALTER TABLE owned_kart MODIFY COLUMN custom5 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x1C overlays def 0x98 no reader in this build';
ALTER TABLE owned_kart MODIFY COLUMN applied_item_a INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x20 0xC1 item key overlays def 0x9C sub 490A70 resolves it in the item catalog';
ALTER TABLE owned_kart MODIFY COLUMN applied_item_b INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x24 0xC1 item key overlays def 0xA0 sub 490A70 resolves it in the item catalog';

ALTER TABLE owned_item RENAME COLUMN unk_08 TO price_key;
ALTER TABLE owned_item MODIFY COLUMN price_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x08 0xC6 price row key the shop and garage panels draw the price from it';

ALTER TABLE owned_part RENAME COLUMN unk_0c TO price_key;
ALTER TABLE owned_part MODIFY COLUMN price_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x0C 0xC6 price row key the shop and garage panels draw the price from it';
ALTER TABLE owned_part MODIFY COLUMN unk_08 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x08 no reader in this build the client only echoes it in C2S 0x00CC';

ALTER TABLE owned_pet RENAME COLUMN unk_0c TO price_key;
ALTER TABLE owned_pet MODIFY COLUMN price_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'rec 0x0C 0xC6 price row key the shop and garage panels draw the price from it';

-- mission definition the two currency numbers carry the labels the client composes for them
ALTER TABLE mission_def RENAME COLUMN reward_currency_a TO reward_mileage;
ALTER TABLE mission_def RENAME COLUMN reward_currency_b TO reward_exp;
ALTER TABLE mission_def MODIFY COLUMN reward_mileage INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x1C UNIT MILEAGE number sub 43C060 draws it at y 0x231 and the result popup sub 46B2B0 at x 0x267';
ALTER TABLE mission_def MODIFY COLUMN reward_exp INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x20 UNIT EXP number sub 43C060 draws it at y 0x246 and the result popup sub 46B2B0 at x 0x1E5';
ALTER TABLE mission_def MODIFY COLUMN reward_extra INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x18 number drawn by sub 43C060 at y 0x26F and by the HUD sub 4B4350 label in the art';
ALTER TABLE mission_def MODIFY COLUMN unknown_00 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x00 no reader in this build';
ALTER TABLE mission_def MODIFY COLUMN unknown_0c INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x0C no reader in this build';
ALTER TABLE mission_def MODIFY COLUMN unknown_24 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x24 no reader in this build';
ALTER TABLE mission_def MODIFY COLUMN unknown_30 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x30 no reader in this build';
ALTER TABLE mission_def MODIFY COLUMN unknown_34 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x34 no reader in this build';

-- quest definition the two numbers are drawn on the quest detail panel the label sits in the art
ALTER TABLE quest_def RENAME COLUMN goal_or_reward_a TO detail_value_lower;
ALTER TABLE quest_def RENAME COLUMN goal_or_reward_b TO detail_value_upper;
ALTER TABLE quest_def MODIFY COLUMN detail_value_lower INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x14 number sub 43CCC0 draws at panel y plus 0x1F7';
ALTER TABLE quest_def MODIFY COLUMN detail_value_upper INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x18 number sub 43CCC0 draws at panel y plus 0x1D4';

-- pet shop rows the condition dword is a pendant key checked against the 0x11A owned pendant list
ALTER TABLE shop_definition RENAME COLUMN unlock_condition_key TO required_pendant_key;
ALTER TABLE shop_definition MODIFY COLUMN required_pendant_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'pet only wire 0x0C sub 460A10 refuses the buy with MSG NOT CONDITION unless the owned pendant list holds this key';

-- room craft definitions the badge dword picks the same two shop corner sprites as every other category
ALTER TABLE room_object_def RENAME COLUMN badge_flag TO badge;
ALTER TABLE room_object_def MODIFY COLUMN badge INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'wire 0x04 1 draws UI Shop itembox hot 2 draws UI Shop itembox new sub 41A160 anything else draws nothing';

-- license progress the third dword has no reader
ALTER TABLE char_license_progress MODIFY COLUMN unknown_08 INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'wire 0x08 no reader in this build';

-- scenario definition the two catalog keys are proven by their readers now
ALTER TABLE scenario_def MODIFY COLUMN character_def_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x10 0xBF driver key sub 47DD10 loads the driver and sub 4B5A00 draws its icon';
ALTER TABLE scenario_def MODIFY COLUMN kart_def_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x14 0xC0 kart key sub 47DD10 loads the kart';
ALTER TABLE scenario_def MODIFY COLUMN reward_category INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'def 0x28 0 to 7 picks the reward icon and the 0x00F8 kind 3 tail';
