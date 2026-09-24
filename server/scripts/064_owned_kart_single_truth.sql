-- 064 one kart truth the legacy vehicles table goes away

-- vehicles was written by character creation only while every shop gacha and mission grant
-- writes owned kart so a kart bought in the shop was missing from the garage the race
-- the room join and the car craft which all read vehicles
-- owned kart already carries the wire instance id the base key the skins and the durability
-- the only thing it lacked is the stat upgrades a player buys on one kart

-- the upgrades are a delta not a copy the effective stat is the catalogue value plus the delta
-- so a grant needs no stat block and a kart is never born with an empty one
ALTER TABLE owned_kart
  ADD COLUMN IF NOT EXISTS up_speed INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat speed',
  ADD COLUMN IF NOT EXISTS up_accel INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat accel',
  ADD COLUMN IF NOT EXISTS up_handling INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat handling',
  ADD COLUMN IF NOT EXISTS up_drift INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat drift',
  ADD COLUMN IF NOT EXISTS up_boost INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat boost',
  ADD COLUMN IF NOT EXISTS up_weight INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat weight',
  ADD COLUMN IF NOT EXISTS up_special INT NOT NULL DEFAULT 0
    COMMENT 'garage upgrade steps added over vehicle templates stat special';

-- every legacy row gets an owned twin first so nothing a character owns is lost
INSERT IGNORE INTO owned_kart (character_id, base_key, period_mode, period_value, active_flag)
SELECT v.character_id, COALESCE(NULLIF(v.template_id, 0), v.vehicle_type_id), 0, 0, 1
  FROM vehicles v
  JOIN characters c ON c.id = v.character_id
  JOIN vehicle_templates vt ON vt.id = COALESCE(NULLIF(v.template_id, 0), v.vehicle_type_id);

-- what the player paid for above the catalogue becomes the delta a negative seed is not an upgrade
UPDATE owned_kart k
  JOIN vehicles v ON v.character_id = k.character_id
                 AND COALESCE(NULLIF(v.template_id, 0), v.vehicle_type_id) = k.base_key
  JOIN vehicle_templates vt ON vt.id = k.base_key
   SET k.up_speed    = GREATEST(0, COALESCE(v.stat_speed, 0)    - vt.stat_speed),
       k.up_accel    = GREATEST(0, COALESCE(v.stat_accel, 0)    - vt.stat_accel),
       k.up_handling = GREATEST(0, COALESCE(v.stat_handling, 0) - vt.stat_handling),
       k.up_drift    = GREATEST(0, COALESCE(v.stat_drift, 0)    - vt.stat_drift),
       k.up_boost    = GREATEST(0, COALESCE(v.stat_boost, 0)    - vt.stat_boost),
       k.up_weight   = GREATEST(0, COALESCE(v.stat_weight, 0)   - vt.stat_weight),
       k.up_special  = GREATEST(0, COALESCE(v.stat_special, 0)  - vt.stat_special);

-- the equipped flag of the legacy row becomes the selection the profile blob already reads
UPDATE characters c
  JOIN vehicles v ON v.character_id = c.id AND v.equipped = 1
  JOIN owned_kart k ON k.character_id = c.id
                   AND k.base_key = COALESCE(NULLIF(v.template_id, 0), v.vehicle_type_id)
   SET c.selected_kart_instance_id = k.id;

-- a selection that names no owned row leaves the lobby stand empty so fall back to the first kart
UPDATE characters c
  JOIN (SELECT character_id, MIN(id) AS kart_id FROM owned_kart GROUP BY character_id) f
    ON f.character_id = c.id
   SET c.selected_kart_instance_id = f.kart_id
 WHERE c.selected_kart_instance_id = 0
    OR NOT EXISTS (SELECT 1 FROM owned_kart o
                    WHERE o.id = c.selected_kart_instance_id
                      AND o.character_id = c.id);

-- vehicle customization had no reader at all the car craft preset holds the real paint and parts
-- it is also the only foreign key that pinned vehicles in place
DROP TABLE IF EXISTS vehicle_customization;

-- one truth now every reader is on owned kart
DROP TABLE IF EXISTS vehicles;
