-- record field 0 zero broke license and tutorial stage lookups since they share the ghost track catalogue
UPDATE track_catalog SET record_field_0 = 1;

UPDATE track_catalog SET tail_string = 'License 1' WHERE track_id = 0 AND tail_string = '';
UPDATE track_catalog SET tail_string = 'License 2' WHERE track_id = 1 AND tail_string = '';
UPDATE track_catalog SET tail_string = 'License 3' WHERE track_id = 2 AND tail_string = '';
UPDATE track_catalog SET tail_string = 'Devil 7'   WHERE track_id = 56 AND tail_string = '';
UPDATE track_catalog SET tail_string = 'Palace 4'  WHERE track_id = 73 AND tail_string = '';
UPDATE track_catalog SET tail_string = CONCAT('Mission ', track_id - 20000000)
 WHERE track_id BETWEEN 20000001 AND 20000005 AND tail_string = '';
