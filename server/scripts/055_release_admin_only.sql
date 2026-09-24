-- fresh install has only account admin admin gm level 2 dev seed and testplayer removed
DELETE FROM owned_character WHERE character_id IN
    (SELECT c.id FROM characters c JOIN accounts a ON a.id = c.account_id WHERE a.username = 'test');
DELETE FROM owned_kart WHERE character_id IN
    (SELECT c.id FROM characters c JOIN accounts a ON a.id = c.account_id WHERE a.username = 'test');
DELETE FROM vehicles WHERE character_id IN
    (SELECT c.id FROM characters c JOIN accounts a ON a.id = c.account_id WHERE a.username = 'test');
DELETE FROM characters WHERE account_id IN (SELECT id FROM accounts WHERE username = 'test');
DELETE FROM accounts WHERE username = 'test';
