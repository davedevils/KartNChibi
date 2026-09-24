-- a local server has no reason to gate items by level one is the floor everyone starts on
UPDATE shop_definition SET level_req = 1 WHERE level_req > 1;

-- FUN 00450F40 refuses row 513 so case duplicate skins are merged to bring the stream to 493
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_skin_dupes AS
SELECT a.skin_key
  FROM def_kart_skin a
  JOIN def_kart_skin b ON LOWER(a.name) = LOWER(b.name) AND a.skin_key <> b.skin_key
 WHERE b.skin_key BETWEEN 9200 AND 9299 AND a.skin_key >= 10000;

DELETE FROM shop_option     WHERE category = 3 AND base_key IN (SELECT skin_key FROM tmp_skin_dupes);
DELETE FROM shop_definition WHERE category = 3 AND base_key IN (SELECT skin_key FROM tmp_skin_dupes);
DELETE FROM def_kart_skin   WHERE skin_key IN (SELECT skin_key FROM tmp_skin_dupes);

DROP TEMPORARY TABLE IF EXISTS tmp_skin_dupes;
