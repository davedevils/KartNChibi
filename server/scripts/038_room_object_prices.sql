-- prices decoded off chibikart 0x10C rows using the shared price bands migration 024 had broken

DELETE FROM room_object_price;

INSERT INTO room_object_price (object_key, slot_index, currency_key, period_type, period_value, extra)
SELECT object_key, 0, 2006, 1, 1, 0 FROM room_object_def
UNION ALL SELECT object_key, 1, 3006, 1, 7, 0 FROM room_object_def
UNION ALL SELECT object_key, 2, 1006, 0, 0, 0 FROM room_object_def;
