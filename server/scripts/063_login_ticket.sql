-- 063 the ticket the login server needs to answer a channel return

-- the stock client leaves the game server with C2S 0x0019 mode 4 reconnects to the login port
-- waits one second then sends a bare C2S 0x00A7 with the blob head it was given at login
-- sub 480500 echoes the u32 at blob 0x000 and the wide string at blob 0x004 and nothing else
-- active sessions cannot answer it the game server deletes every row of the account on 0x00A7
CREATE TABLE IF NOT EXISTS login_ticket (
    account_id   INT UNSIGNED NOT NULL PRIMARY KEY,
    token        VARCHAR(64)  NOT NULL,
    character_id INT UNSIGNED NOT NULL DEFAULT 0,
    issued_at    TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_login_ticket_token (token)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
