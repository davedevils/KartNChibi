-- repairs ghost records where FUN 00425CB0 bailed dword 1 is the driver or kart key both zero
UPDATE ghost_record
   SET char_block = CONCAT(LEFT(char_block, 4), UNHEX('0A000000'), SUBSTRING(char_block, 9))
 WHERE HEX(SUBSTRING(char_block, 5, 4)) = '00000000';

UPDATE ghost_record
   SET kart_block = CONCAT(LEFT(kart_block, 4), UNHEX('1A270000'), SUBSTRING(kart_block, 9))
 WHERE HEX(SUBSTRING(kart_block, 5, 4)) = '00000000';
