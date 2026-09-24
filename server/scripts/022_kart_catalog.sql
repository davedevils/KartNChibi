-- kart catalog was seeded empty causing race stat misses scale is unproven so stat12 is never zero

DELETE FROM kart_catalog;

INSERT INTO kart_catalog (
  kart_id, unk00, unk04, unk0c, class_code, parts_enabled, unk18, unk1c,
  model_name, name2, name3,
  stat0_torque_trim, stat1_speed, stat2_accel, stat3_boost, stat4_boost_pitch,
  stat5_handling, stat6_pitch_scale, stat7_roll_scale, stat8_drift, stat9_steer,
  stat10_drift_threshold, stat11_drift_charge, stat12, stat13, stat14, stat15, stat16)
SELECT
  vt.id, 0, 0, 0, 0, 1, 0, 0,
  vt.name,
  COALESCE(vt.title_key, ''),
  COALESCE(vt.info_key, ''),
  1.0,
  COALESCE(vt.stat_speed, 50) / 100.0,
  COALESCE(vt.stat_accel, 50) / 100.0,
  COALESCE(vt.stat_boost, 30) / 100.0,
  0.5,
  COALESCE(vt.stat_handling, 50) / 100.0,
  1.0, 1.0,
  COALESCE(vt.stat_drift, 40) / 100.0,
  1.0, 0.5, 0.5,
  1.0, 0.0, 0.0, 0.0, 0.0
FROM vehicle_templates vt
WHERE COALESCE(vt.is_enabled, 1) = 1
ORDER BY vt.id
LIMIT 64;
