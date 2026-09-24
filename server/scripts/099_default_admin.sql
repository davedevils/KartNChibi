-- local default only username admin password admin gm level 2 change it on any exposed server

INSERT INTO accounts (username, password_hash, gm_level)
VALUES ('admin', 'pbkdf2$100000$f8627a19283db1927278b1fc5e70d063$e0a2fa7b784f9c6901dfdce15b3551b8f5c4cf4a437122d0e880ad30783e1614', 2)
-- deliberately a no op when the row exists so rerunning must not hand gm back to a demoted account
ON DUPLICATE KEY UPDATE gm_level = gm_level;
