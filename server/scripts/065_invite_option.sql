-- 065 the client invite option the server never stored

-- C2S 0x0130 carries one float the client option 11 sent on lobby entry and on every options close
-- zero means the player still wants room invites anything else means the client drops them silently
-- without this column the random invite picked a player who had already opted out and looked broken
ALTER TABLE accounts
  ADD COLUMN IF NOT EXISTS invite_opt_out TINYINT(1) NOT NULL DEFAULT 0
    COMMENT 'client option 11 of C2S 0x0130 1 means no room invite may reach this account';
