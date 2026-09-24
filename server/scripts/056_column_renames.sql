-- Track catalog and kart catalog columns renamed to their proven meaning wire and values unchanged

ALTER TABLE track_catalog RENAME COLUMN wire_slot_48 TO tuning_engine_setup_bits;
ALTER TABLE track_catalog RENAME COLUMN wire_slot_52 TO tuning_engine_force_bits;
ALTER TABLE track_catalog RENAME COLUMN light_fog_2 TO tuning_turn_force;

-- light fog 0 and 1 are never written to the wire grep on trackCatalogEntry proves it
ALTER TABLE track_catalog DROP COLUMN light_fog_0;
ALTER TABLE track_catalog DROP COLUMN light_fog_1;

ALTER TABLE track_catalog MODIFY COLUMN tuning_engine_setup_bits INT UNSIGNED NOT NULL DEFAULT 1053609165
    COMMENT 'record offset 0x30 raw float bits engine and steering setup scale client global 0x5EB6F0';
ALTER TABLE track_catalog MODIFY COLUMN tuning_engine_force_bits INT UNSIGNED NOT NULL DEFAULT 1058642330
    COMMENT 'record offset 0x34 raw float bits engine force durability scale client global 0x5EB6F4';
ALTER TABLE track_catalog MODIFY COLUMN tuning_turn_force FLOAT NOT NULL DEFAULT 90
    COMMENT 'record offset 0x38 turn force baseline additive term client global 0x5EB6F8';

-- kept unknown columns get a comment naming their record offset only
ALTER TABLE track_catalog MODIFY COLUMN unknown_60 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x3C not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_68 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x44 not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_76 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x4C not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_84 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x54 not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_88 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x58 not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_92 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x5C not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_96 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x60 not read by the client';
ALTER TABLE track_catalog MODIFY COLUMN unknown_100 INT NOT NULL DEFAULT 0
    COMMENT 'record offset 0x64 not read by the client';

-- The 17 stat columns become stat00 to stat16 matching the tick index numbering
ALTER TABLE kart_catalog RENAME COLUMN stat0_torque_trim TO stat00;
ALTER TABLE kart_catalog RENAME COLUMN stat1_speed TO stat01;
ALTER TABLE kart_catalog RENAME COLUMN stat2_accel TO stat02;
ALTER TABLE kart_catalog RENAME COLUMN stat3_boost TO stat03;
ALTER TABLE kart_catalog RENAME COLUMN stat4_boost_pitch TO stat04;
ALTER TABLE kart_catalog RENAME COLUMN stat5_handling TO stat05;
ALTER TABLE kart_catalog RENAME COLUMN stat6_pitch_scale TO stat06;
ALTER TABLE kart_catalog RENAME COLUMN stat7_roll_scale TO stat07;
ALTER TABLE kart_catalog RENAME COLUMN stat8_drift TO stat08;
ALTER TABLE kart_catalog RENAME COLUMN stat9_steer TO stat09;
ALTER TABLE kart_catalog RENAME COLUMN stat10_drift_threshold TO stat10;
ALTER TABLE kart_catalog RENAME COLUMN stat11_drift_charge TO stat11;

-- The comment carries the proven tick meaning from INPUT AND STATS and CLIENT PHYSICS MAP
ALTER TABLE kart_catalog MODIFY COLUMN stat00 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 0 no read site found';
ALTER TABLE kart_catalog MODIFY COLUMN stat01 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 1 no read site found';
ALTER TABLE kart_catalog MODIFY COLUMN stat02 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 2 no read site found';
ALTER TABLE kart_catalog MODIFY COLUMN stat03 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 3 max speed proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat04 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 4 steering gain proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat05 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 5 mini turbo target speed proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat06 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 6 no read site found';
ALTER TABLE kart_catalog MODIFY COLUMN stat07 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 7 turn force proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat08 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 8 wheel spin proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat09 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 9 wheel steer angle proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat10 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 10 drift charge rate proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat11 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 11 drift steer proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat12 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 12 mini turbo threshold proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat13 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 13 mini turbo hold time proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat14 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 14 grip proven in the tick';
ALTER TABLE kart_catalog MODIFY COLUMN stat15 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 15 no read site found';
ALTER TABLE kart_catalog MODIFY COLUMN stat16 FLOAT NOT NULL DEFAULT 0 COMMENT 'index 16 no read site found';

-- unk00 unk04 unk0c unk18 unk1c keep their names no proven meaning a comment records the wire offset
ALTER TABLE kart_catalog MODIFY COLUMN unk00 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'wire offset 0x00 no reader found';
ALTER TABLE kart_catalog MODIFY COLUMN unk04 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'wire offset 0x04 no reader found';
ALTER TABLE kart_catalog MODIFY COLUMN unk0c TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'wire offset 0x0C one byte no reader found';
ALTER TABLE kart_catalog MODIFY COLUMN unk18 INT NOT NULL DEFAULT 0 COMMENT 'wire offset 0x15 no isolated reader';
ALTER TABLE kart_catalog MODIFY COLUMN unk1c INT NOT NULL DEFAULT 0 COMMENT 'wire offset 0x19 no isolated reader';
