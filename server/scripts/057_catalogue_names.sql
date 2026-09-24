-- Catalogue columns renamed to the client reader meaning wire and values unchanged
-- Every comment names the client reader address the pages under docs packets opcodes hold the proof

-- kart catalog S2C 0xC0 record offsets
ALTER TABLE kart_catalog RENAME COLUMN unk00 TO visible_flag;
ALTER TABLE kart_catalog RENAME COLUMN unk04 TO badge;
ALTER TABLE kart_catalog RENAME COLUMN class_code TO vehicle_kind;
ALTER TABLE kart_catalog RENAME COLUMN parts_enabled TO model_scheme;
ALTER TABLE kart_catalog RENAME COLUMN unk1c TO required_level;
ALTER TABLE kart_catalog RENAME COLUMN name2 TO display_name_key;
ALTER TABLE kart_catalog RENAME COLUMN name3 TO description_key;

ALTER TABLE kart_catalog MODIFY COLUMN visible_flag INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x00 zero hides the row kart_catalog_by_index 0x44F550 shop_tile_draw_karts 0x41977D';
ALTER TABLE kart_catalog MODIFY COLUMN badge INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x04 tile overlay 1 new 2 hot shop_tile_draw_karts 0x419942';
ALTER TABLE kart_catalog MODIFY COLUMN unk0c TINYINT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x0C one byte no reader in this build container 0x01A22638 shop copy 0xC60D94 car copy 0x33B0 checked';
ALTER TABLE kart_catalog MODIFY COLUMN vehicle_kind INT NOT NULL DEFAULT 0
    COMMENT 'record 0x10 car 0x33B4 kind 2 bike steer kind 5 spinner wheel count vehicle_kind_to_wheel_count 0x49A2A0';
ALTER TABLE kart_catalog MODIFY COLUMN model_scheme INT NOT NULL DEFAULT 1
    COMMENT 'record 0x14 0 catalogue kart 1 factory car kart_catalog_model_scheme_of_key 0x44F640 car_apply_kart_loadout 0x490A70';
ALTER TABLE kart_catalog MODIFY COLUMN unk18 INT NOT NULL DEFAULT 0
    COMMENT 'record 0x18 no reader in this build container shop copy and car copy checked';
ALTER TABLE kart_catalog MODIFY COLUMN required_level INT NOT NULL DEFAULT 0
    COMMENT 'record 0x1C player level gate byte 0x1A20B09 shop_tile_draw_karts 0x4198AE shop_buy_button_gate 0x413BF4';
ALTER TABLE kart_catalog MODIFY COLUMN display_name_key VARCHAR(32) NOT NULL DEFAULT ''
    COMMENT 'record 0x41 def_trans title key shop_detail_panel_draw 0x45E3D0';
ALTER TABLE kart_catalog MODIFY COLUMN description_key VARCHAR(33) NOT NULL DEFAULT ''
    COMMENT 'record 0x62 def_trans description key shop_detail_panel_draw 0x45E3D0';

-- The 17 stat columns carry the physics port meaning the index stays in the name
ALTER TABLE kart_catalog RENAME COLUMN stat00 TO stat00_garage_bar1_a;
ALTER TABLE kart_catalog RENAME COLUMN stat01 TO stat01_garage_bar1_b;
ALTER TABLE kart_catalog RENAME COLUMN stat02 TO stat02_accel_scale;
ALTER TABLE kart_catalog RENAME COLUMN stat03 TO stat03_max_speed;
ALTER TABLE kart_catalog RENAME COLUMN stat04 TO stat04_steering_gain;
ALTER TABLE kart_catalog RENAME COLUMN stat05 TO stat05_mini_turbo;
ALTER TABLE kart_catalog RENAME COLUMN stat07 TO stat07_turn_force;
ALTER TABLE kart_catalog RENAME COLUMN stat08 TO stat08_wheel_spin;
ALTER TABLE kart_catalog RENAME COLUMN stat09 TO stat09_wheel_steer_angle;
ALTER TABLE kart_catalog RENAME COLUMN stat10 TO stat10_drift_charge_rate;
ALTER TABLE kart_catalog RENAME COLUMN stat11 TO stat11_drift_steer;
ALTER TABLE kart_catalog RENAME COLUMN stat12 TO stat12_mini_turbo_threshold;
ALTER TABLE kart_catalog RENAME COLUMN stat13 TO stat13_mini_turbo_hold;
ALTER TABLE kart_catalog RENAME COLUMN stat14 TO stat14_grip;

ALTER TABLE kart_catalog MODIFY COLUMN stat00_garage_bar1_a FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xA4 garage bar one sum with index 1 garage_stat_bars_compute 0x428AB0 no physics read';
ALTER TABLE kart_catalog MODIFY COLUMN stat01_garage_bar1_b FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xA8 garage bar one sum with index 0 no physics read';
ALTER TABLE kart_catalog MODIFY COLUMN stat02_accel_scale FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xAC velocity gain 1 plus x times 0.01 clamped 1 to 1.01 tick 0x49CDDA garage bar two';
ALTER TABLE kart_catalog MODIFY COLUMN stat03_max_speed FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xB0 clamp x plus 1 to 1..2 times 320 kmh car_physics_tick_local 0x49C0D0 garage bar four';
ALTER TABLE kart_catalog MODIFY COLUMN stat04_steering_gain FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xB4 x times 3 plus 1 clamped to 4 car_drift_update 0x49AA90';
ALTER TABLE kart_catalog MODIFY COLUMN stat05_mini_turbo FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xB8 target speed clamp to 1.2 times 120 kmh and duration clamp to 2 times 400 ms car_boost_start 0x496BE0';
ALTER TABLE kart_catalog MODIFY COLUMN stat06 FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xBC no read site';
ALTER TABLE kart_catalog MODIFY COLUMN stat07_turn_force FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xC0 clamp x plus 1 to 1..2 car_physics_tick_local 0x49C0D0';
ALTER TABLE kart_catalog MODIFY COLUMN stat08_wheel_spin FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xC4 kind 2 wheel spin torque in the tick garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat09_wheel_steer_angle FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xC8 kind 2 front wheel angle in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat10_drift_charge_rate FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xCC x times 0.5 plus 0.3 clamped 0.3 to 0.8 car_drift_update 0x49AC0F garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat11_drift_steer FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xD0 x times 0.6 plus 1.2 clamped 1.2 to 1.8 car_drift_update garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat12_mini_turbo_threshold FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xD4 1 minus x times 0.8 clamped 0.2 to 1 times 10 gauge car_drift_update 0x49AEE1';
ALTER TABLE kart_catalog MODIFY COLUMN stat13_mini_turbo_hold FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xD8 1 minus x times 0.8 clamped 0.2 to 1 times 800 ms car_drift_update 0x49AEE1';
ALTER TABLE kart_catalog MODIFY COLUMN stat14_grip FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xDC per wheel grip x times surface grip times 0.2 tick 0x49C9AF zero stands the kart still';
ALTER TABLE kart_catalog MODIFY COLUMN stat15 FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xE0 no read site';
ALTER TABLE kart_catalog MODIFY COLUMN stat16 FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0xE4 no read site';

-- track catalog S2C 0xC3 record offsets the five floats keep their stored zero
ALTER TABLE track_catalog RENAME COLUMN record_field_0 TO visible_flag;
ALTER TABLE track_catalog RENAME COLUMN unknown_68 TO difficulty;
ALTER TABLE track_catalog RENAME COLUMN required_level TO required_license;
ALTER TABLE track_catalog RENAME COLUMN unknown_76 TO special_mode_only;
ALTER TABLE track_catalog RENAME COLUMN unknown_84 TO fog_near;
ALTER TABLE track_catalog RENAME COLUMN unknown_88 TO fog_far;
ALTER TABLE track_catalog RENAME COLUMN unknown_92 TO lens_flare_x;
ALTER TABLE track_catalog RENAME COLUMN unknown_96 TO lens_flare_y;
ALTER TABLE track_catalog RENAME COLUMN unknown_100 TO lens_flare_z;
ALTER TABLE track_catalog RENAME COLUMN tail_string TO display_name_key;

ALTER TABLE track_catalog MODIFY COLUMN visible_flag INT NOT NULL DEFAULT 1
    COMMENT 'record 0x00 zero hides the row track_catalog_by_index 0x453070 track_select_popup_init 0x474A30';
ALTER TABLE track_catalog MODIFY COLUMN unknown_60 INT NOT NULL DEFAULT 0
    COMMENT 'record 0x3C no reader in this build container 0x01A45F50 world 0x1334C and 0x01AF2B5C readers checked';
ALTER TABLE track_catalog MODIFY COLUMN fall_off_timeout_ms INT NOT NULL DEFAULT 500
    COMMENT 'record 0x40 world_fall_timeout_get 0x4871D0 plus 2000 while world 0x13338 is armed';
ALTER TABLE track_catalog MODIFY COLUMN difficulty INT NOT NULL DEFAULT 0
    COMMENT 'record 0x44 track_select_tile_draw 0x475030 draws value plus one stars of six minus one draws none';
ALTER TABLE track_catalog MODIFY COLUMN required_license INT NOT NULL DEFAULT 0
    COMMENT 'record 0x48 license class gate byte 0x1A20B08 in 0x439160 0x439630 0x457FF0 0x4745E0 0x475030';
ALTER TABLE track_catalog MODIFY COLUMN special_mode_only INT NOT NULL DEFAULT 0
    COMMENT 'record 0x4C above zero hides the row when the 0x0013 game mode 0x00BCE210 is 0 or 1 track_select_popup_init 0x474A30';
ALTER TABLE track_catalog MODIFY COLUMN lap_count INT NOT NULL DEFAULT 3
    COMMENT 'record 0x50 world_track_init 0x4875C0 to 0x4B0DA0 clamped 1 to 9';
ALTER TABLE track_catalog MODIFY COLUMN fog_near FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0x54 world_fog_near_get 0x486BC0 into fog_set_stub 0x58B370 no effect in this build';
ALTER TABLE track_catalog MODIFY COLUMN fog_far FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0x58 world_fog_far_get 0x486BF0 the camera far clip camera_far_clip_set 0x43E520 room world uses 5000';
ALTER TABLE track_catalog MODIFY COLUMN lens_flare_x FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0x5C lens flare sun position lens_flare_init 0x4D2A30';
ALTER TABLE track_catalog MODIFY COLUMN lens_flare_y FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0x60 lens flare sun position lens_flare_update 0x4D25E0';
ALTER TABLE track_catalog MODIFY COLUMN lens_flare_z FLOAT NOT NULL DEFAULT 0
    COMMENT 'record 0x64 lens flare sun position negative turns the flare off';
ALTER TABLE track_catalog MODIFY COLUMN display_name_key VARCHAR(35) NOT NULL DEFAULT ''
    COMMENT 'record 0x68 def_trans key and %s_INFO label in 0x439630 0x475030 0x459100 0x410640';

-- def kart part wire S2C 0xC2 record offsets values stay the seed put the category into word 0c
ALTER TABLE def_kart_part_wire RENAME COLUMN word_0c TO required_level;
ALTER TABLE def_kart_part_wire RENAME COLUMN word_34 TO restrict_target;
ALTER TABLE def_kart_part_wire RENAME COLUMN word_38 TO equip_slot;
ALTER TABLE def_kart_part_wire RENAME COLUMN word_3c TO restrict_key;
ALTER TABLE def_kart_part_wire RENAME COLUMN tail0_hex TO ability_pair0_hex;
ALTER TABLE def_kart_part_wire RENAME COLUMN tail1_hex TO ability_pair1_hex;

ALTER TABLE def_kart_part_wire MODIFY COLUMN visible INT UNSIGNED NOT NULL DEFAULT 1
    COMMENT 'record 0x00 zero hides the row shop_tile_draw_parts 0x4199D0';
ALTER TABLE def_kart_part_wire MODIFY COLUMN badge INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x04 tile overlay 1 new 2 hot shop_tile_draw_parts 0x4199D0';
ALTER TABLE def_kart_part_wire MODIFY COLUMN required_level INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x0C player level gate byte 0x1A20B09 shop_tile_draw_parts 0x419B9E migration 013 stored the category here';
ALTER TABLE def_kart_part_wire MODIFY COLUMN model_name VARCHAR(32) NOT NULL DEFAULT ''
    COMMENT 'record 0x10 part_icons_load 0x4423F0 Parts %s png and the accessory model path 0x48C680';
ALTER TABLE def_kart_part_wire MODIFY COLUMN restrict_target INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x34 0 compares restrict_key with the preview driver key 1 with the preview kart key part_restrict_check 0x415310';
ALTER TABLE def_kart_part_wire MODIFY COLUMN equip_slot INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x38 shop tab and equip slot 0 colour 1 plate 2 body 3 face 4 head 5 glass 6 back 8 kart item shop_part_tab_filter 0x418C80';
ALTER TABLE def_kart_part_wire MODIFY COLUMN restrict_key INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'record 0x3C driver or kart key the part is limited to 0xFFFFFFFF none part_restrict_check 0x415310';
ALTER TABLE def_kart_part_wire MODIFY COLUMN display_name_key VARCHAR(32) NOT NULL DEFAULT ''
    COMMENT 'record 0x40 def_trans title key shop_detail_panel_draw 0x45E3D0';
ALTER TABLE def_kart_part_wire MODIFY COLUMN description_key VARCHAR(33) NOT NULL DEFAULT ''
    COMMENT 'record 0x61 def_trans description key shop_detail_panel_draw 0x45E3D0';
ALTER TABLE def_kart_part_wire MODIFY COLUMN ability_pair0_hex VARCHAR(136) NOT NULL DEFAULT ''
    COMMENT 'record 0x84 ability id then percent as 16 hex chars part_ability_pairs_draw 0x42B1B0';
ALTER TABLE def_kart_part_wire MODIFY COLUMN ability_pair1_hex VARCHAR(136) NOT NULL DEFAULT ''
    COMMENT 'record 0x8C ability id then percent as 16 hex chars part_ability_pairs_draw 0x42B1B0';

-- vehicle templates feed the live 0xC0 sender the names stay the comments carry the wire offsets
ALTER TABLE vehicle_templates MODIFY COLUMN name VARCHAR(64) NOT NULL
    COMMENT 'wire 0x20 model asset name car_apply_kart_loadout 0x490A70 chassis path';
ALTER TABLE vehicle_templates MODIFY COLUMN is_factory_car TINYINT(1) NOT NULL DEFAULT 0
    COMMENT 'wire 0x14 model scheme 1 factory car built from parts 0 catalogue kart';
ALTER TABLE vehicle_templates MODIFY COLUMN title_key VARCHAR(48) NOT NULL DEFAULT ''
    COMMENT 'wire 0x41 def_trans title key shop_detail_panel_draw 0x45E3D0';
ALTER TABLE vehicle_templates MODIFY COLUMN info_key VARCHAR(48) NOT NULL DEFAULT ''
    COMMENT 'wire 0x62 def_trans description key shop_detail_panel_draw 0x45E3D0';
ALTER TABLE vehicle_templates MODIFY COLUMN required_level INT UNSIGNED DEFAULT 1
    COMMENT 'not on the wire the sender writes 0 at 0x1C the player level gate byte 0x1A20B09';
