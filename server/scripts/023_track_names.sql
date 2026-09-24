-- client draws the tail string as the track name and appends INFO for the description

-- several tracks such as License 01 through Mission 04 have no name key in def trans index

-- record field 0 is the row enable flag FUN 00453140 skips a row whose flag is zero
UPDATE track_catalog SET record_field_0 = 1 WHERE tail_string <> '';
UPDATE track_catalog SET record_field_0 = 0 WHERE tail_string = '';
