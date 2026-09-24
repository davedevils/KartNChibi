-- gauges read period mode 3 with period value as percent since migration 013 fixed only one row
ALTER TABLE owned_kart
  MODIFY period_mode  INT UNSIGNED NOT NULL DEFAULT 3,
  MODIFY period_value INT UNSIGNED NOT NULL DEFAULT 100;

UPDATE owned_kart SET period_mode = 3, period_value = 100
 WHERE period_mode = 0 OR period_value = 0;
