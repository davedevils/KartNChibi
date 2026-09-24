-- kart skins were left zero so no model drew def kart skin category 0 is color 1 is name

-- column is unsigned so the negative terminator stores as its bit pattern
UPDATE owned_kart
SET skin_primary   = 9001,
    skin_secondary = 9101,
    skin_tertiary  = 4294967295
WHERE skin_primary = 0 OR skin_secondary = 0;
