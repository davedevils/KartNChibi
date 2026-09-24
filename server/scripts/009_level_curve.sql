-- server owned level ladder only authority for S2C 0x000A and blob offsets 0x4C0 and 0x4C4 numbers are synthetic not retail

CREATE TABLE IF NOT EXISTS level_curve (
    level   TINYINT UNSIGNED NOT NULL PRIMARY KEY,
    cum_exp INT UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO level_curve (level, cum_exp) VALUES
    (1,0), (2,500), (3,1700), (4,3800), (5,7000),
    (6,11500), (7,17500), (8,25200), (9,34800), (10,46500),
    (11,60500), (12,77000), (13,96200), (14,118300), (15,143500),
    (16,172000), (17,204000), (18,239700), (19,279300), (20,323000),
    (21,371000), (22,423500), (23,480700), (24,542800), (25,610000),
    (26,682500), (27,760500), (28,844200), (29,933800), (30,1029500),
    (31,1131500), (32,1240000), (33,1355200), (34,1477300), (35,1606500),
    (36,1743000), (37,1887000), (38,2038700), (39,2198300), (40,2366000),
    (41,2542000), (42,2726500), (43,2919700), (44,3121800), (45,3333000),
    (46,3553500), (47,3783500), (48,4023200), (49,4272800), (50,4532500)
ON DUPLICATE KEY UPDATE cum_exp = VALUES(cum_exp);

UPDATE characters c
  JOIN (
        SELECT c2.id AS cid, MAX(lc.level) AS lvl
          FROM characters c2
          JOIN level_curve lc ON lc.cum_exp <= c2.experience
         GROUP BY c2.id
       ) d ON d.cid = c.id
   SET c.level = GREATEST(c.level, d.lvl);

UPDATE characters c
  JOIN level_curve lc ON lc.level = c.level
   SET c.exp_floor = lc.cum_exp,
       c.exp_next  = COALESCE(
           (SELECT MIN(n.cum_exp) FROM level_curve n WHERE n.level > c.level),
           lc.cum_exp + 1);

-- client FDIVP must never see a zero span
UPDATE characters SET exp_next = exp_floor + 1 WHERE exp_next <= exp_floor;
UPDATE characters SET exp_floor = experience WHERE exp_floor > experience;
