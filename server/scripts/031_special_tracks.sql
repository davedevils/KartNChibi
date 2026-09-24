-- special track catalog captured live from chibikart the availability flag gates the track picker not the track itself

-- rows that point at no map at all predate the catalog and emit garbage
DELETE FROM track_catalog WHERE map_id IS NULL;

INSERT INTO maps (id, name, display_name, theme_folder, track_folder, category) VALUES
    (200, 'random',    'Random',    NULL,      NULL, 'normal'),
    (201, 'mission',   'Mission',   'Mission', NULL, 'scenario'),
    (202, 'battle_01', 'Battle 1',  NULL,      NULL, 'item')
ON DUPLICATE KEY UPDATE display_name = VALUES(display_name);

INSERT INTO track_catalog
    (track_id, map_id, theme_id, folder_name, tail_string, record_field_0, lap_count)
VALUES
    (10000000, 200, 10000000, 'random',    'TRACK_RANDOM_INFO', 1, 3),
    (20000000, 201, 20000000, '',          '',                  0, 3),
    (30000000, 202, 30000000, 'battle_01', '',                  0, 3)
ON DUPLICATE KEY UPDATE
    folder_name    = VALUES(folder_name),
    tail_string    = VALUES(tail_string),
    record_field_0 = VALUES(record_field_0);

-- match their availability flags on the tracks they gate
UPDATE track_catalog SET record_field_0 = 0
    WHERE track_id IN (53, 54, 55, 56, 63, 73);
UPDATE track_catalog SET record_field_0 = 1
    WHERE track_id NOT IN (53, 54, 55, 56, 63, 73, 20000000, 30000000)
      AND track_id < 20000000;
