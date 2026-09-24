-- The 17 kart stat columns renamed to the WIRE index meaning values unchanged
-- Migration 057 named columns 2 to 14 by the physics port index which counted from car 0x3440
-- car apply kart loadout 0x490A70 copies the 0xC0 record from byte 0 to car 0x33A4 so record 0xA4
-- lands at car 0x3448 and wire float k is car 0x3448 plus 4 k with its part bonus at car 0xA7940 plus 4 k
-- The port index was the wire index plus two so every 057 name sat two columns too high
-- Column N holds wire float N of the 0xC0 record the value the client reads at car 0x3448 plus 4 N
-- Proof docs reverse physics INPUT AND STATS the 17 stats wire numbering settled

ALTER TABLE kart_catalog RENAME COLUMN stat00_garage_bar1_a TO stat00_body_setup;
ALTER TABLE kart_catalog RENAME COLUMN stat01_garage_bar1_b TO stat01_max_speed;
ALTER TABLE kart_catalog RENAME COLUMN stat02_accel_scale TO stat02_steering_gain;
ALTER TABLE kart_catalog RENAME COLUMN stat03_max_speed TO stat03_mini_turbo_target;
ALTER TABLE kart_catalog RENAME COLUMN stat04_steering_gain TO stat04_boost_lean_lift;
ALTER TABLE kart_catalog RENAME COLUMN stat05_mini_turbo TO stat05_turn_force;
ALTER TABLE kart_catalog RENAME COLUMN stat06 TO stat06_wheel_spin;
ALTER TABLE kart_catalog RENAME COLUMN stat07_turn_force TO stat07_wheel_steer_angle;
ALTER TABLE kart_catalog RENAME COLUMN stat08_wheel_spin TO stat08_drift_charge_rate;
ALTER TABLE kart_catalog RENAME COLUMN stat09_wheel_steer_angle TO stat09_drift_steer;
ALTER TABLE kart_catalog RENAME COLUMN stat10_drift_charge_rate TO stat10_mini_turbo_threshold;
ALTER TABLE kart_catalog RENAME COLUMN stat11_drift_steer TO stat11_mini_turbo_hold;
ALTER TABLE kart_catalog RENAME COLUMN stat12_mini_turbo_threshold TO stat12_grip;
ALTER TABLE kart_catalog RENAME COLUMN stat13_mini_turbo_hold TO stat13;
ALTER TABLE kart_catalog RENAME COLUMN stat14_grip TO stat14_camera_distance;
ALTER TABLE kart_catalog RENAME COLUMN stat15 TO stat15_camera_pitch;
ALTER TABLE kart_catalog RENAME COLUMN stat16 TO stat16_camera_height;

ALTER TABLE kart_catalog MODIFY COLUMN stat00_body_setup FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 0 record 0xA4 car 0x3448 tick 0x49CDDA velocity gain 1 plus x times 0.01 clamped 1 to 1.01 garage bar one with wire 1';
ALTER TABLE kart_catalog MODIFY COLUMN stat01_max_speed FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 1 record 0xA8 car 0x344C tick 0x49CA95 clamp x plus 1 to 1..2 times 320 kmh garage bar one with wire 0';
ALTER TABLE kart_catalog MODIFY COLUMN stat02_steering_gain FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 2 record 0xAC car 0x3450 car_drift_update 0x49B1B2 x times 3 plus 1 clamped to 4 garage bar two';
ALTER TABLE kart_catalog MODIFY COLUMN stat03_mini_turbo_target FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 3 record 0xB0 car 0x3454 car_boost_start 0x496D2B clamp x times 0.2 plus 1 to 1.2 times 120 kmh car_boost_update 0x497065 clamp x plus 1 to 2 times 400 ms garage bar four';
ALTER TABLE kart_catalog MODIFY COLUMN stat04_boost_lean_lift FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 4 record 0xB4 car 0x3458 car_effect_lean_update 0x49B4C0 x times 30 clamped 0 to 30 degrees nose lift on a boost car_visual_update 0x48EBC8 visual only';
ALTER TABLE kart_catalog MODIFY COLUMN stat05_turn_force FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 5 record 0xB8 car 0x345C tick 0x49CB45 clamp x plus 1 to 1..2 over 2 snaps to 1.6 in a drift else 1';
ALTER TABLE kart_catalog MODIFY COLUMN stat06_wheel_spin FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 6 record 0xBC car 0x3460 tick 0x49D8D6 kind 2 wheel spin torque car_effect_lean_update 0x49B54B lean lift scale';
ALTER TABLE kart_catalog MODIFY COLUMN stat07_wheel_steer_angle FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 7 record 0xC0 car 0x3464 tick 0x49D78F kind 2 front wheel angle car_effect_lean_update 0x49B528 side lean scale';
ALTER TABLE kart_catalog MODIFY COLUMN stat08_drift_charge_rate FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 8 record 0xC4 car 0x3468 car_drift_update 0x49AC11 x times 0.5 plus 0.3 clamped 0.3 to 0.8 garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat09_drift_steer FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 9 record 0xC8 car 0x346C tick 0x49D3C9 and car_drift_update 0x49B24D x times 0.6 plus 1.2 clamped 1.2 to 1.8';
ALTER TABLE kart_catalog MODIFY COLUMN stat10_mini_turbo_threshold FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 10 record 0xCC car 0x3470 car_drift_update 0x49AEF5 1 minus x times 0.8 clamped 0.2 to 1 times 10 gauge garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat11_mini_turbo_hold FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 11 record 0xD0 car 0x3474 car_drift_update 0x49AF47 1 minus x times 0.8 clamped 0.2 to 1 times 800 ms garage bar three';
ALTER TABLE kart_catalog MODIFY COLUMN stat12_grip FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 12 record 0xD4 car 0x3478 tick 0x49C9D6 per wheel grip x times surface grip plus extra times 0.2';
ALTER TABLE kart_catalog MODIFY COLUMN stat13 FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 13 record 0xD8 car 0x347C no read site in the exe only stat_bonus_add_part writes its bonus';
ALTER TABLE kart_catalog MODIFY COLUMN stat14_camera_distance FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 14 record 0xDC car 0x3480 camera_update 0x43F529 chase camera distance 9.0 on the shipped rows no part bonus';
ALTER TABLE kart_catalog MODIFY COLUMN stat15_camera_pitch FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 15 record 0xE0 car 0x3484 camera_update 0x43F50A chase camera pitch degrees 37 on the shipped rows';
ALTER TABLE kart_catalog MODIFY COLUMN stat16_camera_height FLOAT NOT NULL DEFAULT 0
    COMMENT 'wire 16 record 0xE4 car 0x3488 camera_update 0x43F510 look point height 3.5 on the shipped rows';

-- The garage bars garage stat bars compute 0x428AB0 normalise over the whole catalogue
-- bar one wire 0 plus 1 bar two wire 2 bar three wire 8 plus 10 plus 11 bar four wire 3
-- fraction times 0 2 capped 0 3 plus 0 6 times 100 clamped 40 to 100
