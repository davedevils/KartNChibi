

-- role 2 hides the room button role 6 unlocks observer camera separate from gm level
ALTER TABLE accounts
    ADD COLUMN IF NOT EXISTS launcher_token VARCHAR(255) NULL,
    -- role is backticked because it is a keyword in MariaDB grammar
    ADD COLUMN IF NOT EXISTS `role` TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'S2C 0x0007 +0x4BD. 2 hides create room, 6 unlocks observer',
    ADD COLUMN IF NOT EXISTS channel_band TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'S2C 0x0007 +0x4A4 channel gate';


-- level exp gold cash license class and driver key already exist do not duplicate
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS driver_base_key INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0xBF catalog record+0x0C, value sent in C2S 0x0004',
    ADD COLUMN IF NOT EXISTS title_key INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'S2C 0x0007 +0x4C8',
    ADD COLUMN IF NOT EXISTS exp_floor INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'PlayerInfo +0x4BC exp bar lower bound',
    ADD COLUMN IF NOT EXISTS exp_next INT UNSIGNED NOT NULL DEFAULT 1000
        COMMENT 'PlayerInfo +0x4C0 exp bar upper bound, MUST be > exp_floor or FUN_00429990 divides by zero',
    -- duplicates owned character accessory slots on purpose see the conflict notes
    ADD COLUMN IF NOT EXISTS accessory_slot_2 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x08 O_BODY',
    ADD COLUMN IF NOT EXISTS accessory_slot_3 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x0C O_FACE',
    ADD COLUMN IF NOT EXISTS accessory_slot_4 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x10 O_HEAD',
    ADD COLUMN IF NOT EXISTS accessory_slot_5 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x14 O_GLASS',
    ADD COLUMN IF NOT EXISTS accessory_slot_6 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x18 O_BACK',
    -- mirrors client dword 80E66C echoed in S2C 0x010B and 0x000A
    ADD COLUMN IF NOT EXISTS selected_kart_instance_id INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'vehicles.id of the active kart',
    -- backs the profile popup blob 0x68 bytes
    ADD COLUMN IF NOT EXISTS character_key INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'profile blob +0x28 portrait sprite key',
    ADD COLUMN IF NOT EXISTS pendant_key INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'profile blob +0x64 pendant catalog key',
    ADD COLUMN IF NOT EXISTS pendant_slot INT NOT NULL DEFAULT 0
        COMMENT 'user list entry +0x24 sprite index 1..63, 0 or less draws the default',
    ADD COLUMN IF NOT EXISTS stat_a1 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x48 label unrecoverable',
    ADD COLUMN IF NOT EXISTS stat_b1 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x4C',
    ADD COLUMN IF NOT EXISTS stat_c1 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x50',
    ADD COLUMN IF NOT EXISTS stat_a2 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x58',
    ADD COLUMN IF NOT EXISTS stat_b2 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x5C',
    ADD COLUMN IF NOT EXISTS stat_c2 INT NOT NULL DEFAULT 0 COMMENT 'profile blob +0x60',
    ADD COLUMN IF NOT EXISTS pccafe_grade TINYINT NOT NULL DEFAULT 3
        COMMENT '0 1 2 draw an icafe icon, 3 or more draws nothing';

-- keeps the xp bar invariant true for rows that predate exp next
UPDATE characters SET exp_next = exp_floor + 1000 WHERE exp_next <= exp_floor;


-- after first login the client reauths with only a session id and token character id must be nullable
ALTER TABLE active_sessions
    MODIFY COLUMN character_id INT UNSIGNED NULL;

ALTER TABLE active_sessions
    ADD COLUMN IF NOT EXISTS reauth_session_id INT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'issued in S2C 0x0007 +0x004',
    ADD COLUMN IF NOT EXISTS reauth_token VARCHAR(128) NOT NULL DEFAULT ''
        COMMENT 'issued in S2C 0x0007 +0x008',
    ADD INDEX IF NOT EXISTS idx_reauth_session (reauth_session_id),
    ADD INDEX IF NOT EXISTS idx_reauth_token (reauth_token);


-- the client ignores the custom car block unless the kart catalog row is a factory car
ALTER TABLE vehicle_templates
    ADD COLUMN IF NOT EXISTS is_factory_car TINYINT(1) NOT NULL DEFAULT 0
        COMMENT '0xC0 catalog field +0x14 == 1';


-- S2C 0x000E entry is 16 fixed bytes not 12 capped at 80 rows
CREATE TABLE IF NOT EXISTS channels (
    id INT UNSIGNED NOT NULL PRIMARY KEY,
    name VARCHAR(127) NOT NULL,
    population INT UNSIGNED NOT NULL DEFAULT 0,
    capacity INT UNSIGNED NOT NULL DEFAULT 256,
    tier INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 beginner 1 advanced 2 free',
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,
    INDEX idx_tier (tier, sort_order)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO channels (id, name, population, capacity, tier, sort_order) VALUES
    (1, 'Beginner 1', 0, 256, 0, 0),
    (2, 'Beginner 2', 0, 256, 0, 1),
    (3, 'Beginner 3', 0, 256, 0, 2),
    (4, 'Advanced 1', 0, 256, 1, 3),
    (5, 'Advanced 2', 0, 256, 1, 4),
    (6, 'Advanced 3', 0, 256, 1, 5),
    (7, 'Free 1',     0, 256, 2, 6),
    (8, 'Free 2',     0, 256, 2, 7),
    (9, 'Free 3',     0, 256, 2, 8)
ON DUPLICATE KEY UPDATE name = VALUES(name);


-- client filters chat and nicknames via sub 4E15E0 from the same taboo list
CREATE TABLE IF NOT EXISTS banned_words (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    word VARCHAR(64) NOT NULL UNIQUE,
    scope ENUM('chat','nickname','both') NOT NULL DEFAULT 'both',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- shop price must exist first sub 451E00 does one flat scan across categories
CREATE TABLE IF NOT EXISTS shop_price (
    price_key INT UNSIGNED NOT NULL PRIMARY KEY,
    unknown_04 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'S2C 0x00C6 wire 0x04, no reader found, keep 0',
    unknown_08 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'S2C 0x00C6 wire 0x08, no reader found, keep 0',
    unit_type TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 permanent 1 days 2 uses 3 durability',
    unit_amount INT UNSIGNED NOT NULL DEFAULT 0,
    price_base INT NOT NULL DEFAULT 0,
    price_sale INT NOT NULL DEFAULT 0 COMMENT 'when > 0 it is what is displayed AND what must be charged',
    currency TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'server side only 0 gold 1 cash, never on the wire'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- sub 451CF0 caps the client vector at 4 slots per definition
CREATE TABLE IF NOT EXISTS shop_option (
    category TINYINT UNSIGNED NOT NULL COMMENT '0 char 1 kart 2 item 3 part 4 pet 5 roomcraft 6 carcraft',
    base_key INT UNSIGNED NOT NULL,
    slot TINYINT UNSIGNED NOT NULL COMMENT '0..3 only',
    price_key INT UNSIGNED NOT NULL,
    opt_word1 INT UNSIGNED NOT NULL DEFAULT 0,
    opt_word2 INT UNSIGNED NOT NULL DEFAULT 0,
    opt_word3 INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (category, base_key, slot),
    KEY idx_shop_option_price (price_key),
    CONSTRAINT fk_shop_option_price FOREIGN KEY (price_key)
        REFERENCES shop_price(price_key) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- read by ShopPackets validatePurchase for all seven categories
CREATE TABLE IF NOT EXISTS shop_definition (
    category TINYINT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL,
    shop_visible_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'wire 0x00, zero hides the row',
    badge TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'wire 0x04, 0 none 1 hot 2 new',
    subtype INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'roomcraft and carcraft sub tab index',
    level_req INT UNSIGNED NOT NULL DEFAULT 0,
    unlock_condition_key INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'pet only, wire 0x0C',
    str1_name VARCHAR(32) NOT NULL DEFAULT '',
    str2 VARCHAR(32) NOT NULL DEFAULT '',
    str3_desc VARCHAR(33) NOT NULL DEFAULT '',
    PRIMARY KEY (category, base_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- backs client dword 1A95244 category 4 buys need def plus 0x0C present
CREATE TABLE IF NOT EXISTS pet_condition (
    character_id INT UNSIGNED NOT NULL,
    condition_key INT UNSIGNED NOT NULL,
    granted_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, condition_key),
    CONSTRAINT fk_pet_condition_char FOREIGN KEY (character_id)
        REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- C2S 0x0098 gift target is a name not an id so both are kept
CREATE TABLE IF NOT EXISTS gift_log (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    sender_id INT UNSIGNED NOT NULL,
    target_name VARCHAR(32) NOT NULL,
    target_id INT UNSIGNED NULL,
    category TINYINT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL,
    price_key INT NOT NULL DEFAULT -1 COMMENT 'client sends -1 when its own price resolve failed',
    message VARCHAR(512) NOT NULL DEFAULT '',
    sent_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_gift_log_sender (sender_id),
    KEY idx_gift_log_target (target_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- every shop verb is sendable even with price key -1 so failed attempts are logged too
CREATE TABLE IF NOT EXISTS shop_transaction_log (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    verb ENUM('buy','sell','gift','extend') NOT NULL,
    category TINYINT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL DEFAULT 0,
    price_key INT NOT NULL DEFAULT -1,
    currency TINYINT UNSIGNED NOT NULL DEFAULT 0,
    amount INT NOT NULL DEFAULT 0,
    gold_before INT NOT NULL DEFAULT 0,
    gold_after INT NOT NULL DEFAULT 0,
    cash_before INT NOT NULL DEFAULT 0,
    cash_after INT NOT NULL DEFAULT 0,
    result VARCHAR(32) NOT NULL DEFAULT 'ok' COMMENT 'ShopPackets PurchaseResult name',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_shop_tx_char (character_id),
    KEY idx_shop_tx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- sub 44FDE0 erases rows by base key before inserting id is the wire instance id

CREATE TABLE IF NOT EXISTS owned_character (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT 'rec+0x00 wire instance id',
    character_id INT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL COMMENT 'rec+0x04, 0xBF def key',
    acc_body INT NOT NULL DEFAULT 0 COMMENT 'rec+0x08',
    acc_face INT NOT NULL DEFAULT 0 COMMENT 'rec+0x0C',
    acc_head INT NOT NULL DEFAULT 0 COMMENT 'rec+0x10',
    acc_glass INT NOT NULL DEFAULT 0 COMMENT 'rec+0x14',
    acc_back INT NOT NULL DEFAULT 0 COMMENT 'rec+0x18',
    unk_1c INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x1C, no reader proven, echo only',
    period_mode INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x20',
    period_value INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x24',
    active_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'rec+0x28, zero enables the garage Delete button',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_owned_character (character_id, base_key),
    INDEX idx_owned_character_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS owned_kart (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT 'rec+0x00 wire instance id',
    character_id INT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL COMMENT 'rec+0x04, 0xC0 def key',
    skin_primary INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x08 overlays def+0x84',
    skin_secondary INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x0C overlays def+0x88',
    skin_tertiary INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x10 overlays def+0x8C',
    custom3 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x14 overlays def+0x90, slot meaning unknown',
    custom4 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x18 overlays def+0x94',
    custom5 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x1C overlays def+0x98',
    applied_item_a INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x20, 0xC1 item key',
    applied_item_b INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x24, 0xC1 item key',
    unk_28 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x28, no reader proven',
    period_mode INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x2C',
    period_value INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x30, durability when period_mode is 3',
    active_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'rec+0x34',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_owned_kart (character_id, base_key),
    INDEX idx_owned_kart_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS owned_item (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT 'rec+0x00 wire instance id',
    character_id INT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL COMMENT 'rec+0x04, 0xC1 def key',
    unk_08 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x08, no reader proven',
    period_mode INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x0C, four bytes EARLIER than 0x1E and 0x0104',
    period_value INT NOT NULL DEFAULT 0 COMMENT 'rec+0x10, remaining uses for def types 4..7',
    active_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'rec+0x14',
    in_use_flag INT NOT NULL DEFAULT 0 COMMENT 'rec+0x18, client tests == 1 only',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_owned_item (character_id, base_key),
    INDEX idx_owned_item_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS owned_part (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT 'rec+0x00, MUST be unique across the whole 0x1E container',
    character_id INT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL COMMENT 'rec+0x04, 0xC2 def key',
    unk_08 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x08, echoed back in C2S 0x00CC',
    unk_0c INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x0C, no reader proven',
    period_mode INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x10, C2S 0x00CC gated on this being 2',
    period_value INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x14',
    active_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'rec+0x18',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_owned_part (character_id, base_key),
    INDEX idx_owned_part_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS owned_pet (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT 'rec+0x00 wire instance id',
    character_id INT UNSIGNED NOT NULL,
    base_key INT UNSIGNED NOT NULL COMMENT 'rec+0x04, 0x0103 def key',
    equipped_flag INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x08, client tests == 1, at most one per character',
    unk_0c INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x0C, no reader proven',
    period_mode INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x10',
    period_value INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rec+0x14',
    active_flag INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'rec+0x18',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_owned_pet (character_id, base_key),
    INDEX idx_owned_pet_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- stat block hex is a hex string not a blob since Database truncates at the first null byte
CREATE TABLE IF NOT EXISTS carcraft_part_def (
    part_key INT UNSIGNED NOT NULL PRIMARY KEY,
    enabled INT UNSIGNED NOT NULL DEFAULT 1,
    category INT UNSIGNED NOT NULL DEFAULT 0,
    model_dir_name VARCHAR(32) NOT NULL DEFAULT '' COMMENT 'ascii, client buffer is 33 with the NUL',
    display_name_key VARCHAR(32) NOT NULL DEFAULT '',
    description_key VARCHAR(33) NOT NULL DEFAULT '',
    stat_block_hex VARCHAR(136) NOT NULL DEFAULT '' COMMENT '17 little endian floats hex encoded, empty decodes to all zero',
    wheel_attach_0 INT UNSIGNED NOT NULL DEFAULT 0,
    wheel_attach_1 INT UNSIGNED NOT NULL DEFAULT 0,
    wheel_attach_2 INT UNSIGNED NOT NULL DEFAULT 0,
    INDEX idx_carcraft_def_category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS carcraft_part_price (
    part_key INT UNSIGNED NOT NULL,
    row_index TINYINT UNSIGNED NOT NULL COMMENT '0..3, client keeps at most 4',
    price_table_key INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'must exist in shop_price or sub_4852E0 returns -1',
    period_type INT UNSIGNED NOT NULL DEFAULT 0,
    period_value INT UNSIGNED NOT NULL DEFAULT 0,
    active INT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (part_key, row_index)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS custom_car_part_instance (
    instance_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    part_key INT UNSIGNED NOT NULL,
    category INT UNSIGNED NOT NULL DEFAULT 0,
    equip_refcount INT NOT NULL DEFAULT 0,
    price_table_key INT UNSIGNED NOT NULL DEFAULT 0,
    period_type INT UNSIGNED NOT NULL DEFAULT 0,
    period_value INT NOT NULL DEFAULT 0,
    period_active INT UNSIGNED NOT NULL DEFAULT 1,
    grade INT NOT NULL DEFAULT 0,
    UNIQUE KEY uq_part_char_key (character_id, part_key),
    INDEX idx_part_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- sub 4500E0 drops row 6 sub 450140 resolves a preset by kart id
CREATE TABLE IF NOT EXISTS custom_car_preset (
    preset_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id INT UNSIGNED NOT NULL,
    slot_state INT UNSIGNED NOT NULL DEFAULT 1,
    name VARCHAR(11) NOT NULL DEFAULT 'Factory Car' COMMENT 'ascii, 12 byte field with the NUL',
    kart_instance_id INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'vehicles.id',
    part_inst_cover INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_tire INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_booster INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_bumper INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_ffender INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_rfender INT UNSIGNED NOT NULL DEFAULT 0,
    part_inst_wing INT UNSIGNED NOT NULL DEFAULT 0,
    UNIQUE KEY uq_preset_kart (character_id, kart_instance_id),
    INDEX idx_preset_char (character_id),
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- column names match RoomCraftPackets SELECTs exactly do not rename
CREATE TABLE IF NOT EXISTS room_object_def (
    object_key INT UNSIGNED NOT NULL PRIMARY KEY,
    enabled INT UNSIGNED NOT NULL DEFAULT 1,
    badge_flag INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'wire +0x04, 1 and 2 pick two corner badges, rest unknown',
    category INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 Sky 1 Floor 2 BgObj 3 Object 4 Effect',
    max_placeable INT UNSIGNED NOT NULL DEFAULT 1,
    required_level INT UNSIGNED NOT NULL DEFAULT 0,
    asset_folder VARCHAR(32) NOT NULL DEFAULT '' COMMENT 'ascii, client buffer 33 with the NUL',
    name_loc_key VARCHAR(32) NOT NULL DEFAULT '',
    desc_loc_key VARCHAR(33) NOT NULL DEFAULT '',
    INDEX idx_room_object_category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS room_object_price (
    object_key INT UNSIGNED NOT NULL,
    slot_index TINYINT UNSIGNED NOT NULL COMMENT '0..3, sub_451CF0 drops slot 4 and later',
    currency_key INT UNSIGNED NOT NULL DEFAULT 0,
    period_type INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 permanent 1 expiry day 2 count',
    period_value INT UNSIGNED NOT NULL DEFAULT 0,
    extra INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'never read by the client, keep 0',
    PRIMARY KEY (object_key, slot_index),
    CONSTRAINT fk_room_object_price_def FOREIGN KEY (object_key)
        REFERENCES room_object_def(object_key) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- sub 452830 lookup key sub 47DA00 stores it back with no null check
CREATE TABLE IF NOT EXISTS player_item_instance (
    instance_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    player_id INT UNSIGNED NOT NULL COMMENT 'characters.id',
    object_key INT UNSIGNED NOT NULL,
    category INT UNSIGNED NOT NULL DEFAULT 0,
    pos_x FLOAT NOT NULL DEFAULT 0 COMMENT 'axis order unresolved, raw f32 copy only',
    pos_y FLOAT NOT NULL DEFAULT 0,
    pos_z FLOAT NOT NULL DEFAULT 0,
    yaw FLOAT NOT NULL DEFAULT 0,
    placed INT UNSIGNED NOT NULL DEFAULT 0,
    price_key INT UNSIGNED NOT NULL DEFAULT 0,
    period_type INT UNSIGNED NOT NULL DEFAULT 0,
    period_value INT UNSIGNED NOT NULL DEFAULT 0,
    active INT UNSIGNED NOT NULL DEFAULT 1,
    INDEX idx_player_item_player (player_id),
    INDEX idx_player_item_placed (player_id, placed),
    CONSTRAINT fk_player_item_char FOREIGN KEY (player_id)
        REFERENCES characters(id) ON DELETE CASCADE,
    CONSTRAINT fk_player_item_def FOREIGN KEY (object_key)
        REFERENCES room_object_def(object_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS room_decor (
    room_id INT UNSIGNED NOT NULL,
    instance_id INT UNSIGNED NOT NULL,
    object_key INT UNSIGNED NOT NULL,
    category INT UNSIGNED NOT NULL DEFAULT 0,
    pos_x FLOAT NOT NULL DEFAULT 0,
    pos_y FLOAT NOT NULL DEFAULT 0,
    pos_z FLOAT NOT NULL DEFAULT 0,
    yaw FLOAT NOT NULL DEFAULT 0,
    placed INT UNSIGNED NOT NULL DEFAULT 1,
    price_key INT UNSIGNED NOT NULL DEFAULT 0,
    period_type INT UNSIGNED NOT NULL DEFAULT 0,
    period_value INT UNSIGNED NOT NULL DEFAULT 0,
    active INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'sub_488300 skips the record unless this is 1',
    PRIMARY KEY (room_id, instance_id),
    INDEX idx_room_decor_room (room_id),
    INDEX idx_room_decor_object (object_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- mission id needs a progress row S2C 0x8C writes through it with no null check
CREATE TABLE IF NOT EXISTS mission_def (
    mission_id INT UNSIGNED NOT NULL PRIMARY KEY,
    unknown_00 INT UNSIGNED NOT NULL DEFAULT 0,
    mission_kind INT UNSIGNED NOT NULL DEFAULT 0,
    unknown_0c INT UNSIGNED NOT NULL DEFAULT 0,
    goal_count INT NOT NULL DEFAULT 0,
    time_limit_ms INT NOT NULL DEFAULT 0,
    reward_extra INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'def+0x18, wallet unproven',
    reward_currency_a INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'def+0x1C, wallet unproven',
    reward_currency_b INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'def+0x20, wallet unproven',
    unknown_24 INT UNSIGNED NOT NULL DEFAULT 0,
    reward_item_type INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0..7, picks the reward blob size',
    reward_item_key INT UNSIGNED NOT NULL DEFAULT 0,
    unknown_30 INT UNSIGNED NOT NULL DEFAULT 0,
    unknown_34 INT UNSIGNED NOT NULL DEFAULT 0,
    world_name VARCHAR(32) NOT NULL DEFAULT '',
    str_key_title VARCHAR(32) NOT NULL DEFAULT '',
    str_key_sub VARCHAR(32) NOT NULL DEFAULT '',
    str_key_desc VARCHAR(32) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS char_mission_progress (
    char_id INT UNSIGNED NOT NULL,
    mission_id INT UNSIGNED NOT NULL,
    cleared INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (char_id, mission_id),
    INDEX idx_char_mission_char (char_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- server only never on the wire validates C2S 0x8C against a deadline
CREATE TABLE IF NOT EXISTS char_mission_state (
    char_id INT UNSIGNED NOT NULL PRIMARY KEY,
    mission_id INT UNSIGNED NOT NULL,
    started_at_ms BIGINT NOT NULL DEFAULT 0,
    deadline_ms BIGINT NOT NULL DEFAULT 0,
    counter INT NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS license_test_def (
    license_key INT UNSIGNED NOT NULL PRIMARY KEY COMMENT 'observed set 0..3 10..13 20..23',
    unknown_00 INT UNSIGNED NOT NULL DEFAULT 0,
    name VARCHAR(35) NOT NULL DEFAULT '',
    param_00 INT UNSIGNED NOT NULL DEFAULT 0,
    param_01 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'struct +0x30 exp reward',
    param_02 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'struct +0x34 gold reward',
    param_03 INT UNSIGNED NOT NULL DEFAULT 0,
    param_04 INT UNSIGNED NOT NULL DEFAULT 0,
    param_05 INT UNSIGNED NOT NULL DEFAULT 0,
    param_06 INT UNSIGNED NOT NULL DEFAULT 0,
    param_07 INT UNSIGNED NOT NULL DEFAULT 0,
    param_08 INT UNSIGNED NOT NULL DEFAULT 0,
    param_09 INT UNSIGNED NOT NULL DEFAULT 0,
    param_10 INT UNSIGNED NOT NULL DEFAULT 0,
    param_11 INT UNSIGNED NOT NULL DEFAULT 0,
    param_12 INT UNSIGNED NOT NULL DEFAULT 0,
    str_key_a VARCHAR(32) NOT NULL DEFAULT '',
    str_key_b VARCHAR(34) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- S2C 0xA2 has no clear opcode send it exactly once per session
CREATE TABLE IF NOT EXISTS char_license_progress (
    char_id INT UNSIGNED NOT NULL,
    license_key INT UNSIGNED NOT NULL,
    passed INT UNSIGNED NOT NULL DEFAULT 0,
    unknown_08 INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (char_id, license_key),
    INDEX idx_char_license_char (char_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- motion relay is transient world key 0x1312D00 flips encoding from 25 to 34 bytes
CREATE TABLE IF NOT EXISTS race_session (
    session_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    room_id INT UNSIGNED NOT NULL,
    track_id INT NOT NULL DEFAULT 0,
    game_mode INT NOT NULL DEFAULT 0,
    world_key INT NOT NULL DEFAULT 0 COMMENT '20000000 means raw 34 byte motion entries',
    started_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    ended_at DATETIME NULL,
    INDEX idx_race_session_room (room_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS race_session_player (
    session_id INT UNSIGNED NOT NULL,
    player_id INT UNSIGNED NOT NULL COMMENT 'characters.id',
    grid_index TINYINT NOT NULL DEFAULT 0,
    PRIMARY KEY (session_id, player_id),
    INDEX idx_race_session_player_player (player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- only needed if server drives placement instead of client grid sub 486C20 sub 4A05C0
CREATE TABLE IF NOT EXISTS track_spawn (
    track_id INT NOT NULL,
    grid_index TINYINT NOT NULL,
    spawn_x FLOAT NOT NULL DEFAULT 0,
    spawn_y FLOAT NOT NULL DEFAULT 0,
    spawn_z FLOAT NOT NULL DEFAULT 0,
    spawn_yaw_deg FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (track_id, grid_index)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- read by ResultsPackets computeRewards column names match its SELECTs do not rename
CREATE TABLE IF NOT EXISTS reward_rules (
    game_mode TINYINT UNSIGNED NOT NULL DEFAULT 0,
    finish_rank INT NOT NULL COMMENT 'ZERO based, rank 0 is the winner',
    gold_reward INT NOT NULL DEFAULT 0,
    exp_reward INT NOT NULL DEFAULT 0,
    gold_bonus INT NOT NULL DEFAULT 0,
    exp_bonus INT NOT NULL DEFAULT 0,
    PRIMARY KEY (game_mode, finish_rank)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- placeholder values invented not captured from retail replace after a live capture
INSERT IGNORE INTO reward_rules
    (game_mode, finish_rank, gold_reward, exp_reward, gold_bonus, exp_bonus) VALUES
    (0, 0, 100, 50, 50, 25),
    (0, 1,  80, 40, 40, 20),
    (0, 2,  65, 33, 32, 16),
    (0, 3,  50, 25, 25, 12),
    (0, 4,  40, 20, 20, 10),
    (0, 5,  30, 15, 15,  7),
    (0, 6,  20, 10, 10,  5),
    (0, 7,  10,  5,  5,  2);

-- per race per player overlaps race history finish time ms at or under 0 means DNF or retire
CREATE TABLE IF NOT EXISTS race_result (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    race_id BIGINT UNSIGNED NOT NULL,
    player_id INT UNSIGNED NOT NULL COMMENT 'characters.id',
    finish_rank INT NOT NULL DEFAULT -1 COMMENT 'ZERO based, -1 never finished',
    finish_time_ms INT NOT NULL DEFAULT 0,
    team_id INT UNSIGNED NOT NULL DEFAULT 0,
    bonus_item_key INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '4001 gold up 4002 exp up 4003 total up, rest draw nothing',
    gold_reward INT NOT NULL DEFAULT 0,
    exp_reward INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_race_player (race_id, player_id),
    INDEX idx_race_result_player (player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- friends and blocked players already exist from 003 not recreated sub 44F150 caps at 30

-- drives S2C 0x0078 the rows the target sees client caps 100 per target
CREATE TABLE IF NOT EXISTS friend_requests (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    requester_id INT UNSIGNED NOT NULL,
    target_id INT UNSIGNED NOT NULL,
    state TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 pending 1 accepted 2 rejected',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_request (requester_id, target_id),
    KEY idx_target_pending (target_id, state),
    FOREIGN KEY (requester_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (target_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- sub 465490 already doubles quotes do not double them again
CREATE TABLE IF NOT EXISTS player_notes (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    from_id INT UNSIGNED NOT NULL,
    to_id INT UNSIGNED NULL,
    to_name VARCHAR(32) NOT NULL,
    body VARCHAR(160) NOT NULL,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    KEY idx_to_id (to_id),
    KEY idx_to_name (to_name),
    FOREIGN KEY (from_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- client self mutes after 5 sends in 2000ms for 60000ms only protects an unmodified client
CREATE TABLE IF NOT EXISTS chat_mutes (
    character_id INT UNSIGNED NOT NULL PRIMARY KEY,
    muted_until DATETIME NOT NULL,
    reason VARCHAR(64) NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


UPDATE characters
   SET driver_base_key = COALESCE(NULLIF(driver_base_key, 0), equipped_driver_id, 1)
 WHERE driver_base_key = 0;

UPDATE characters c
  JOIN vehicles v ON v.character_id = c.id AND v.equipped = 1
   SET c.selected_kart_instance_id = v.id
 WHERE c.selected_kart_instance_id = 0;

SELECT '007 emu buildout complete' AS Status;
