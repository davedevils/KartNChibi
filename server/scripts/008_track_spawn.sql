-- sub 48A800 sub 49BEB0 sub 486C20 prove spawn z already includes the plus 0-5 grid lift never add kGridSpawnLift again

-- forest 01 is track id 1 from Forest Forest 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (1, 0, -206.671, -344.216, 28.634, 0),
    (1, 1, -206.671, -337.741, 28.634, 0),
    (1, 2, -206.671, -331.227, 28.634, 0),
    (1, 3, -206.671, -324.772, 28.634, 0),
    (1, 4, -206.671, -318.304, 28.634, 0),
    (1, 5, -206.671, -311.819, 28.634, 0),
    (1, 6, -206.671, -305.346, 28.634, 0),
    (1, 7, -206.671, -298.868, 28.634, 0),
    (1, 8, -201.636, -340.965, 28.73, 0),
    (1, 9, -201.636, -334.49, 28.73, 0),
    (1, 10, -201.636, -327.976, 28.73, 0),
    (1, 11, -201.636, -321.521, 28.73, 0),
    (1, 12, -201.636, -315.053, 28.73, 0),
    (1, 13, -201.636, -308.568, 28.73, 0),
    (1, 14, -201.636, -302.095, 28.73, 0),
    (1, 15, -201.636, -295.617, 28.73, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- forest 02 is track id 2 from Forest Forest 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (2, 0, -436.22, -220.667, 0.56, 0),
    (2, 1, -436.22, -214.193, 0.56, 0),
    (2, 2, -436.22, -207.679, 0.56, 0),
    (2, 3, -436.22, -201.224, 0.56, 0),
    (2, 4, -436.22, -194.756, 0.56, 0),
    (2, 5, -436.22, -188.271, 0.56, 0),
    (2, 6, -436.22, -181.798, 0.56, 0),
    (2, 7, -436.22, -175.319, 0.56, 0),
    (2, 8, -431.185, -217.417, 0.56, 0),
    (2, 9, -431.185, -210.942, 0.56, 0),
    (2, 10, -431.185, -204.428, 0.56, 0),
    (2, 11, -431.185, -197.973, 0.56, 0),
    (2, 12, -431.185, -191.505, 0.56, 0),
    (2, 13, -431.185, -185.02, 0.56, 0),
    (2, 14, -431.185, -178.547, 0.56, 0),
    (2, 15, -431.185, -172.069, 0.56, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- forest 03 is track id 3 from Forest Forest 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (3, 0, -170.251, -10.654, -4.938, 0),
    (3, 1, -170.251, -4.179, -4.938, 0),
    (3, 2, -170.251, 2.334, -4.938, 0),
    (3, 3, -170.251, 8.79, -4.938, 0),
    (3, 4, -170.251, 15.257, -4.938, 0),
    (3, 5, -170.251, 21.742, -4.938, 0),
    (3, 6, -170.251, 28.215, -4.938, 0),
    (3, 7, -170.251, 34.694, -4.938, 0),
    (3, 8, -165.216, -7.403, -4.938, 0),
    (3, 9, -165.216, -0.929, -4.938, 0),
    (3, 10, -165.216, 5.585, -4.938, 0),
    (3, 11, -165.216, 12.04, -4.938, 0),
    (3, 12, -165.216, 18.508, -4.938, 0),
    (3, 13, -165.216, 24.993, -4.938, 0),
    (3, 14, -165.216, 31.466, -4.938, 0),
    (3, 15, -165.216, 37.945, -4.938, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- forest 04 is track id 4 from Forest Forest 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (4, 0, 131.333, 317.362, 8.034, 0),
    (4, 1, 131.333, 323.836, 8.034, 0),
    (4, 2, 131.333, 330.35, 8.034, 0),
    (4, 3, 131.333, 336.805, 8.034, 0),
    (4, 4, 131.333, 343.273, 8.034, 0),
    (4, 5, 131.333, 349.758, 8.034, 0),
    (4, 6, 131.333, 356.231, 8.034, 0),
    (4, 7, 131.333, 362.71, 8.034, 0),
    (4, 8, 136.369, 320.613, 8.034, 0),
    (4, 9, 136.369, 327.087, 8.034, 0),
    (4, 10, 136.369, 333.601, 8.034, 0),
    (4, 11, 136.369, 340.056, 8.034, 0),
    (4, 12, 136.369, 346.524, 8.034, 0),
    (4, 13, 136.369, 353.009, 8.034, 0),
    (4, 14, 136.369, 359.482, 8.034, 0),
    (4, 15, 136.369, 365.961, 8.034, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- desert 01 is track id 10 from Desert Desert 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (10, 0, 476.475, -163.689, 1.362, 0),
    (10, 1, 476.475, -157.214, 1.362, 0),
    (10, 2, 476.475, -150.701, 1.362, 0),
    (10, 3, 476.475, -144.245, 1.362, 0),
    (10, 4, 476.475, -137.777, 1.362, 0),
    (10, 5, 476.475, -131.292, 1.362, 0),
    (10, 6, 476.475, -124.82, 1.362, 0),
    (10, 7, 476.475, -118.341, 1.362, 0),
    (10, 8, 481.51, -160.438, 1.276, 0),
    (10, 9, 481.51, -153.963, 1.276, 0),
    (10, 10, 481.51, -147.45, 1.276, 0),
    (10, 11, 481.51, -140.994, 1.276, 0),
    (10, 12, 481.51, -134.527, 1.276, 0),
    (10, 13, 481.51, -128.041, 1.276, 0),
    (10, 14, 481.51, -121.569, 1.276, 0),
    (10, 15, 481.51, -115.09, 1.276, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- desert 02 is track id 11 from Desert Desert 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (11, 0, -102.101, -373.603, -32.184, 0),
    (11, 1, -102.101, -367.128, -32.184, 0),
    (11, 2, -102.101, -360.614, -32.184, 0),
    (11, 3, -102.101, -354.159, -32.184, 0),
    (11, 4, -102.101, -347.691, -32.184, 0),
    (11, 5, -102.101, -341.206, -32.184, 0),
    (11, 6, -102.101, -334.733, -32.184, 0),
    (11, 7, -102.101, -328.255, -32.184, 0),
    (11, 8, -97.065, -370.352, -32.088, 0),
    (11, 9, -97.065, -363.877, -32.088, 0),
    (11, 10, -97.065, -357.363, -32.088, 0),
    (11, 11, -97.065, -350.908, -32.088, 0),
    (11, 12, -97.065, -344.44, -32.088, 0),
    (11, 13, -97.065, -337.955, -32.088, 0),
    (11, 14, -97.065, -331.483, -32.088, 0),
    (11, 15, -97.065, -325.004, -32.088, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- desert 03 is track id 12 from Desert Desert 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (12, 0, -283.126, -188.768, 0.593, 0),
    (12, 1, -283.126, -184.408, 0.593, 0),
    (12, 2, -283.126, -179.636, 0.593, 0),
    (12, 3, -283.126, -174.725, 0.593, 0),
    (12, 4, -283.126, -170.669, 0.593, 0),
    (12, 5, -283.126, -165.897, 0.593, 0),
    (12, 6, -283.126, -161.033, 0.593, 0),
    (12, 7, -283.126, -155.72, 0.593, 0),
    (12, 8, -278.366, -186.57, 0.593, 0),
    (12, 9, -278.366, -182.209, 0.593, 0),
    (12, 10, -278.366, -177.437, 0.593, 0),
    (12, 11, -278.366, -172.527, 0.593, 0),
    (12, 12, -278.366, -168.47, 0.593, 0),
    (12, 13, -278.366, -163.698, 0.593, 0),
    (12, 14, -278.366, -158.787, 0.593, 0),
    (12, 15, -278.366, -153.521, 0.593, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- desert 04 is track id 13 from Desert Desert 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (13, 0, 373.823, 619.879, 0.923, 0),
    (13, 1, 373.823, 624.239, 0.923, 0),
    (13, 2, 373.823, 629.011, 0.923, 0),
    (13, 3, 373.823, 633.922, 0.923, 0),
    (13, 4, 373.823, 637.978, 0.923, 0),
    (13, 5, 373.823, 642.75, 0.923, 0),
    (13, 6, 373.823, 647.614, 0.923, 0),
    (13, 7, 373.823, 652.927, 0.923, 0),
    (13, 8, 378.583, 622.078, 0.923, 0),
    (13, 9, 378.583, 626.438, 0.923, 0),
    (13, 10, 378.583, 631.21, 0.923, 0),
    (13, 11, 378.583, 636.12, 0.923, 0),
    (13, 12, 378.583, 640.177, 0.923, 0),
    (13, 13, 378.583, 644.949, 0.923, 0),
    (13, 14, 378.583, 649.86, 0.923, 0),
    (13, 15, 378.583, 655.126, 0.923, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- snow 01 is track id 20 from Snow Snow 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (20, 0, 171.044, 598.001, 107.509, 0),
    (20, 1, 171.044, 592.688, 107.509, 0),
    (20, 2, 171.044, 587.824, 107.509, 0),
    (20, 3, 171.044, 583.052, 107.509, 0),
    (20, 4, 171.044, 578.996, 107.509, 0),
    (20, 5, 171.044, 574.085, 107.509, 0),
    (20, 6, 171.044, 569.313, 107.509, 0),
    (20, 7, 171.044, 564.953, 107.509, 0),
    (20, 8, 175.804, 600.199, 107.509, 0),
    (20, 9, 175.804, 594.933, 107.509, 0),
    (20, 10, 175.804, 590.023, 107.509, 0),
    (20, 11, 175.804, 585.251, 107.509, 0),
    (20, 12, 175.804, 581.194, 107.509, 0),
    (20, 13, 175.804, 576.283, 107.509, 0),
    (20, 14, 175.804, 571.511, 107.509, 0),
    (20, 15, 175.804, 567.151, 107.509, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- snow 02 is track id 21 from Snow Snow 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (21, 0, 58.309, -497.704, -64.74, 0),
    (21, 1, 58.309, -503.017, -64.74, 0),
    (21, 2, 58.309, -507.88, -64.74, 0),
    (21, 3, 58.309, -512.652, -64.74, 0),
    (21, 4, 58.309, -516.709, -64.74, 0),
    (21, 5, 58.309, -521.62, -64.74, 0),
    (21, 6, 58.309, -526.392, -63.936, 0),
    (21, 7, 58.309, -530.752, -63.936, 0),
    (21, 8, 63.07, -495.505, -64.74, 0),
    (21, 9, 63.07, -500.771, -64.74, 0),
    (21, 10, 63.07, -505.682, -64.74, 0),
    (21, 11, 63.07, -510.454, -64.74, 0),
    (21, 12, 63.07, -514.51, -64.74, 0),
    (21, 13, 63.07, -519.421, -64.74, 0),
    (21, 14, 63.07, -524.193, -64.74, 0),
    (21, 15, 63.07, -528.553, -64.74, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- snow 03 is track id 22 from Snow Snow 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (22, 0, 9.698, -7.578, 0.665, 0),
    (22, 1, 9.698, -12.892, 0.665, 0),
    (22, 2, 9.698, -17.755, 0.665, 0),
    (22, 3, 9.698, -22.527, 0.665, 0),
    (22, 4, 9.698, -26.583, 0.665, 0),
    (22, 5, 9.698, -31.494, 0.665, 0),
    (22, 6, 9.698, -36.266, 0.665, 0),
    (22, 7, 9.698, -40.626, 0.665, 0),
    (22, 8, 14.458, -5.38, 0.665, 0),
    (22, 9, 14.458, -10.646, 0.665, 0),
    (22, 10, 14.458, -15.557, 0.665, 0),
    (22, 11, 14.458, -20.328, 0.665, 0),
    (22, 12, 14.458, -24.385, 0.665, 0),
    (22, 13, 14.458, -29.296, 0.665, 0),
    (22, 14, 14.458, -34.068, 0.665, 0),
    (22, 15, 14.458, -38.428, 0.665, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- snow 04 is track id 23 from Snow Snow 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (23, 0, 45.103, -17.891, 0.476, 0),
    (23, 1, 45.103, -13.531, 0.476, 0),
    (23, 2, 45.103, -8.759, 0.476, 0),
    (23, 3, 45.103, -3.849, 0.476, 0),
    (23, 4, 45.103, 0.208, 0.476, 0),
    (23, 5, 45.103, 4.98, 0.476, 0),
    (23, 6, 45.103, 9.843, 0.476, 0),
    (23, 7, 45.103, 15.157, 0.476, 0),
    (23, 8, 49.863, -15.693, 0.476, 0),
    (23, 9, 49.863, -11.333, 0.476, 0),
    (23, 10, 49.863, -6.561, 0.476, 0),
    (23, 11, 49.863, -1.65, 0.476, 0),
    (23, 12, 49.863, 2.406, 0.476, 0),
    (23, 13, 49.863, 7.178, 0.476, 0),
    (23, 14, 49.863, 12.089, 0.476, 0),
    (23, 15, 49.863, 17.355, 0.476, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- palace 01 is track id 30 from Palace Palace 01 with 16 rows no yaw column so stored as 0
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (30, 0, 307.624, -152.579, -23.253, 0),
    (30, 1, 307.624, -148.219, -23.253, 0),
    (30, 2, 307.624, -143.447, -23.253, 0),
    (30, 3, 307.624, -138.536, -23.253, 0),
    (30, 4, 307.624, -134.48, -23.253, 0),
    (30, 5, 307.624, -129.708, -23.253, 0),
    (30, 6, 307.624, -124.845, -23.253, 0),
    (30, 7, 307.624, -119.531, -23.253, 0),
    (30, 8, 312.384, -150.381, -23.253, 0),
    (30, 9, 312.384, -146.021, -23.253, 0),
    (30, 10, 312.384, -141.249, -23.253, 0),
    (30, 11, 312.384, -136.338, -23.253, 0),
    (30, 12, 312.384, -132.282, -23.253, 0),
    (30, 13, 312.384, -127.51, -23.253, 0),
    (30, 14, 312.384, -122.599, -23.253, 0),
    (30, 15, 312.384, -117.333, -23.253, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- palace 02 is track id 31 from Palace Palace 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (31, 0, 414.46, -1.204, 0.5, 0),
    (31, 1, 414.46, 4.895, 0.5, 0),
    (31, 2, 414.46, 10.995, 0.5, 0),
    (31, 3, 414.46, 17.095, 0.5, 0),
    (31, 4, 414.46, 23.194, 0.5, 0),
    (31, 5, 414.46, 29.294, 0.5, 0),
    (31, 6, 414.46, 35.394, 0.5, 0),
    (31, 7, 414.46, 41.493, 0.5, 0),
    (31, 8, 418.003, 1.845, 0.5, 0),
    (31, 9, 418.003, 7.945, 0.5, 0),
    (31, 10, 418.003, 14.045, 0.5, 0),
    (31, 11, 418.003, 20.144, 0.5, 0),
    (31, 12, 418.003, 26.244, 0.5, 0),
    (31, 13, 418.003, 32.344, 0.5, 0),
    (31, 14, 418.003, 38.443, 0.5, 0),
    (31, 15, 418.003, 44.543, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- palace 03 is track id 32 from Palace Palace 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (32, 0, 219.247, -557.037, 0.5, 0),
    (32, 1, 219.247, -550.784, 0.5, 0),
    (32, 2, 219.247, -544.531, 0.5, 0),
    (32, 3, 219.247, -538.278, 0.5, 0),
    (32, 4, 219.247, -532.025, 0.5, 0),
    (32, 5, 219.247, -525.772, 0.5, 0),
    (32, 6, 219.247, -519.519, 0.5, 0),
    (32, 7, 219.247, -513.266, 0.5, 0),
    (32, 8, 223.076, -553.911, 0.5, 0),
    (32, 9, 223.076, -547.658, 0.5, 0),
    (32, 10, 223.076, -541.404, 0.5, 0),
    (32, 11, 223.076, -535.151, 0.5, 0),
    (32, 12, 223.076, -528.898, 0.5, 0),
    (32, 13, 223.076, -522.645, 0.5, 0),
    (32, 14, 223.076, -516.392, 0.5, 0),
    (32, 15, 223.076, -510.139, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- palace 04 is track id 33 from Palace Palace 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (33, 0, 219.247, -557.037, 0.5, 0),
    (33, 1, 219.247, -550.784, 0.5, 0),
    (33, 2, 219.247, -544.531, 0.5, 0),
    (33, 3, 219.247, -538.278, 0.5, 0),
    (33, 4, 219.247, -532.025, 0.5, 0),
    (33, 5, 219.247, -525.772, 0.5, 0),
    (33, 6, 219.247, -519.519, 0.5, 0),
    (33, 7, 219.247, -513.266, 0.5, 0),
    (33, 8, 223.076, -553.911, 0.5, 0),
    (33, 9, 223.076, -547.658, 0.5, 0),
    (33, 10, 223.076, -541.404, 0.5, 0),
    (33, 11, 223.076, -535.151, 0.5, 0),
    (33, 12, 223.076, -528.898, 0.5, 0),
    (33, 13, 223.076, -522.645, 0.5, 0),
    (33, 14, 223.076, -516.392, 0.5, 0),
    (33, 15, 223.076, -510.139, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- palace 05 is track id 34 from Palace Palace 05 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (34, 0, 99.45, 418.163, 0.5, 0),
    (34, 1, 99.45, 424.313, 0.5, 0),
    (34, 2, 99.45, 430.463, 0.5, 0),
    (34, 3, 99.45, 436.613, 0.5, 0),
    (34, 4, 99.45, 442.763, 0.5, 0),
    (34, 5, 99.45, 448.913, 0.5, 0),
    (34, 6, 99.45, 455.063, 0.5, 0),
    (34, 7, 99.45, 461.214, 0.5, 0),
    (34, 8, 102.906, 421.238, 0.5, 0),
    (34, 9, 102.906, 427.388, 0.5, 0),
    (34, 10, 102.906, 433.538, 0.5, 0),
    (34, 11, 102.906, 439.688, 0.5, 0),
    (34, 12, 102.906, 445.838, 0.5, 0),
    (34, 13, 102.906, 451.988, 0.5, 0),
    (34, 14, 102.906, 458.139, 0.5, 0),
    (34, 15, 102.906, 464.289, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- cookie 01 is track id 40 from Cookie Cookie 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (40, 0, -78.3, 43.442, -8.021, 0),
    (40, 1, -78.141, 49.484, -8.372, 0),
    (40, 2, -77.983, 55.525, -8.626, 0),
    (40, 3, -77.824, 61.566, -8.65, 0),
    (40, 4, -77.665, 67.607, -8.674, 0),
    (40, 5, -77.506, 73.649, -8.721, 0),
    (40, 6, -77.347, 79.69, -8.576, 0),
    (40, 7, -77.188, 85.731, -8.406, 0),
    (40, 8, -72.129, 46.303, -8.328, 0),
    (40, 9, -71.97, 52.344, -8.59, 0),
    (40, 10, -71.811, 58.385, -8.792, 0),
    (40, 11, -71.653, 64.427, -8.831, 0),
    (40, 12, -71.494, 70.468, -8.873, 0),
    (40, 13, -71.335, 76.509, -8.913, 0),
    (40, 14, -71.176, 82.55, -8.722, 0),
    (40, 15, -71.017, 88.591, -8.483, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- cookie 02 is track id 41 from Cookie Cookie 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (41, 0, 581.685, 224.922, 0.554, 0),
    (41, 1, 581.685, 231.397, 0.554, 0),
    (41, 2, 581.685, 237.91, 0.554, 0),
    (41, 3, 581.685, 244.366, 0.554, 0),
    (41, 4, 581.685, 250.833, 0.554, 0),
    (41, 5, 581.685, 257.318, 0.554, 0),
    (41, 6, 581.685, 263.791, 0.554, 0),
    (41, 7, 581.685, 270.27, 0.554, 0),
    (41, 8, 586.72, 228.173, 0.554, 0),
    (41, 9, 586.72, 234.647, 0.554, 0),
    (41, 10, 586.72, 241.161, 0.554, 0),
    (41, 11, 586.72, 247.616, 0.554, 0),
    (41, 12, 586.72, 254.084, 0.554, 0),
    (41, 13, 586.72, 260.569, 0.554, 0),
    (41, 14, 586.72, 267.042, 0.554, 0),
    (41, 15, 586.72, 273.521, 0.554, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- cookie 03 is track id 42 from Cookie Cookie 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (42, 0, 54.406, -380.184, 0.486, 0),
    (42, 1, 54.406, -373.709, 0.486, 0),
    (42, 2, 54.406, -367.196, 0.486, 0),
    (42, 3, 54.406, -360.74, 0.486, 0),
    (42, 4, 54.406, -354.273, 0.486, 0),
    (42, 5, 54.406, -347.788, 0.486, 0),
    (42, 6, 54.406, -341.315, 0.486, 0),
    (42, 7, 54.406, -334.836, 0.486, 0),
    (42, 8, 59.442, -376.933, 0.486, 0),
    (42, 9, 59.442, -370.458, 0.486, 0),
    (42, 10, 59.442, -363.945, 0.486, 0),
    (42, 11, 59.442, -357.489, 0.486, 0),
    (42, 12, 59.442, -351.022, 0.486, 0),
    (42, 13, 59.442, -344.537, 0.486, 0),
    (42, 14, 59.442, -338.064, 0.486, 0),
    (42, 15, 59.442, -331.585, 0.486, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- cookie 04 is track id 43 from Cookie Cookie 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (43, 0, 220.802, -240.261, 0.435, 0),
    (43, 1, 220.802, -233.786, 0.435, 0),
    (43, 2, 220.802, -227.273, 0.435, 0),
    (43, 3, 220.802, -220.817, 0.435, 0),
    (43, 4, 220.802, -214.349, 0.435, 0),
    (43, 5, 220.802, -207.864, 0.435, 0),
    (43, 6, 220.802, -201.392, 0.435, 0),
    (43, 7, 220.802, -194.913, 0.435, 0),
    (43, 8, 225.837, -237.01, 0.531, 0),
    (43, 9, 225.837, -230.535, 0.531, 0),
    (43, 10, 225.837, -224.022, 0.531, 0),
    (43, 11, 225.837, -217.566, 0.531, 0),
    (43, 12, 225.837, -211.099, 0.531, 0),
    (43, 13, 225.837, -204.613, 0.531, 0),
    (43, 14, 225.837, -198.141, 0.531, 0),
    (43, 15, 225.837, -191.662, 0.531, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- toy 01 is track id 50 from Toy Toy 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (50, 0, -175.374, -66.482, 15.794, 0),
    (50, 1, -175.374, -60.007, 15.794, 0),
    (50, 2, -175.374, -53.494, 15.794, 0),
    (50, 3, -175.374, -47.038, 15.794, 0),
    (50, 4, -175.374, -40.571, 15.794, 0),
    (50, 5, -175.374, -34.085, 15.794, 0),
    (50, 6, -175.374, -27.613, 15.794, 0),
    (50, 7, -175.374, -21.134, 15.794, 0),
    (50, 8, -170.338, -63.231, 15.795, 0),
    (50, 9, -170.338, -56.756, 15.795, 0),
    (50, 10, -170.338, -50.243, 15.795, 0),
    (50, 11, -170.338, -43.787, 15.795, 0),
    (50, 12, -170.338, -37.32, 15.795, 0),
    (50, 13, -170.338, -30.835, 15.795, 0),
    (50, 14, -170.338, -24.362, 15.795, 0),
    (50, 15, -170.338, -17.883, 15.795, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- toy 02 is track id 51 from Toy Toy 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (51, 0, 8.174, -406.923, 1.137, 0),
    (51, 1, 8.174, -400.448, 1.137, 0),
    (51, 2, 8.174, -393.935, 1.137, 0),
    (51, 3, 8.174, -387.479, 1.137, 0),
    (51, 4, 8.174, -381.012, 1.137, 0),
    (51, 5, 8.174, -374.527, 1.137, 0),
    (51, 6, 8.174, -368.054, 1.137, 0),
    (51, 7, 8.174, -361.575, 1.137, 0),
    (51, 8, 13.209, -403.672, 1.233, 0),
    (51, 9, 13.209, -397.197, 1.233, 0),
    (51, 10, 13.209, -390.684, 1.233, 0),
    (51, 11, 13.209, -384.228, 1.233, 0),
    (51, 12, 13.209, -377.761, 1.233, 0),
    (51, 13, 13.209, -371.276, 1.233, 0),
    (51, 14, 13.209, -364.803, 1.233, 0),
    (51, 15, 13.209, -358.324, 1.233, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- toy 03 is track id 52 from Toy Toy 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (52, 0, 184.733, 619.03, 0.5, 0),
    (52, 1, 184.733, 625.505, 0.5, 0),
    (52, 2, 184.733, 632.019, 0.5, 0),
    (52, 3, 184.733, 638.474, 0.5, 0),
    (52, 4, 184.733, 644.942, 0.5, 0),
    (52, 5, 184.733, 651.427, 0.5, 0),
    (52, 6, 184.733, 657.9, 0.5, 0),
    (52, 7, 184.733, 664.378, 0.5, 0),
    (52, 8, 189.768, 622.281, 0.5, 0),
    (52, 9, 189.768, 628.756, 0.5, 0),
    (52, 10, 189.768, 635.27, 0.5, 0),
    (52, 11, 189.768, 641.725, 0.5, 0),
    (52, 12, 189.768, 648.193, 0.5, 0),
    (52, 13, 189.768, 654.678, 0.5, 0),
    (52, 14, 189.768, 661.151, 0.5, 0),
    (52, 15, 189.768, 667.629, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- toy 04 is track id 53 from Toy Toy 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (53, 0, -215.899, 348.168, 0.5, 0),
    (53, 1, -215.899, 354.431, 0.5, 0),
    (53, 2, -215.899, 360.695, 0.5, 0),
    (53, 3, -215.899, 366.959, 0.5, 0),
    (53, 4, -215.899, 373.223, 0.5, 0),
    (53, 5, -215.899, 379.487, 0.5, 0),
    (53, 6, -215.899, 385.751, 0.5, 0),
    (53, 7, -215.899, 392.015, 0.5, 0),
    (53, 8, -212.417, 351.3, 0.5, 0),
    (53, 9, -212.417, 357.563, 0.5, 0),
    (53, 10, -212.417, 363.827, 0.5, 0),
    (53, 11, -212.417, 370.091, 0.5, 0),
    (53, 12, -212.417, 376.355, 0.5, 0),
    (53, 13, -212.417, 382.619, 0.5, 0),
    (53, 14, -212.417, 388.883, 0.5, 0),
    (53, 15, -212.417, 395.146, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- devil 01 is track id 60 from Devil Devil 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (60, 0, 124.544, -288.264, 0.5, 0),
    (60, 1, 124.544, -282.076, 0.5, 0),
    (60, 2, 124.544, -275.888, 0.5, 0),
    (60, 3, 124.544, -269.701, 0.5, 0),
    (60, 4, 124.544, -263.513, 0.5, 0),
    (60, 5, 124.544, -257.325, 0.5, 0),
    (60, 6, 124.544, -251.137, 0.5, 0),
    (60, 7, 124.544, -244.949, 0.5, 0),
    (60, 8, 127.845, -285.17, 0.5, 0),
    (60, 9, 127.845, -278.982, 0.5, 0),
    (60, 10, 127.845, -272.794, 0.5, 0),
    (60, 11, 127.845, -266.607, 0.5, 0),
    (60, 12, 127.845, -260.419, 0.5, 0),
    (60, 13, 127.845, -254.231, 0.5, 0),
    (60, 14, 127.845, -248.043, 0.5, 0),
    (60, 15, 127.845, -241.855, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- devil 02 is track id 61 from Devil Devil 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (61, 0, -229.072, 196.584, 0.5, 0),
    (61, 1, -229.072, 202.625, 0.5, 0),
    (61, 2, -229.072, 208.665, 0.5, 0),
    (61, 3, -229.072, 214.705, 0.5, 0),
    (61, 4, -229.072, 220.746, 0.5, 0),
    (61, 5, -229.072, 226.786, 0.5, 0),
    (61, 6, -229.072, 232.826, 0.5, 0),
    (61, 7, -229.072, 238.867, 0.5, 0),
    (61, 8, -225.605, 199.604, 0.5, 0),
    (61, 9, -225.605, 205.645, 0.5, 0),
    (61, 10, -225.605, 211.685, 0.5, 0),
    (61, 11, -225.605, 217.725, 0.5, 0),
    (61, 12, -225.605, 223.766, 0.5, 0),
    (61, 13, -225.605, 229.806, 0.5, 0),
    (61, 14, -225.605, 235.847, 0.5, 0),
    (61, 15, -225.605, 241.887, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- devil 03 is track id 62 from Devil Devil 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (62, 0, -0.162, -187.944, 0.5, 0),
    (62, 1, -0.162, -181.716, 0.5, 0),
    (62, 2, -0.162, -175.488, 0.5, 0),
    (62, 3, -0.162, -169.261, 0.5, 0),
    (62, 4, -0.162, -163.033, 0.5, 0),
    (62, 5, -0.162, -156.805, 0.5, 0),
    (62, 6, -0.162, -150.578, 0.5, 0),
    (62, 7, -0.162, -144.35, 0.5, 0),
    (62, 8, 3.572, -184.83, 0.5, 0),
    (62, 9, 3.572, -178.602, 0.5, 0),
    (62, 10, 3.572, -172.374, 0.5, 0),
    (62, 11, 3.572, -166.147, 0.5, 0),
    (62, 12, 3.572, -159.919, 0.5, 0),
    (62, 13, 3.572, -153.692, 0.5, 0),
    (62, 14, 3.572, -147.464, 0.5, 0),
    (62, 15, 3.572, -141.236, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- devil 04 is track id 63 from Devil Devil 04 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (63, 0, 54.435, 145.992, 0.5, 0),
    (63, 1, 54.435, 152.045, 0.5, 0),
    (63, 2, 54.435, 158.099, 0.5, 0),
    (63, 3, 54.435, 164.153, 0.5, 0),
    (63, 4, 54.435, 170.207, 0.5, 0),
    (63, 5, 54.435, 176.261, 0.5, 0),
    (63, 6, 54.435, 182.315, 0.5, 0),
    (63, 7, 54.435, 188.369, 0.5, 0),
    (63, 8, 57.743, 149.019, 0.5, 0),
    (63, 9, 57.743, 155.072, 0.5, 0),
    (63, 10, 57.743, 161.126, 0.5, 0),
    (63, 11, 57.743, 167.18, 0.5, 0),
    (63, 12, 57.743, 173.234, 0.5, 0),
    (63, 13, 57.743, 179.288, 0.5, 0),
    (63, 14, 57.743, 185.342, 0.5, 0),
    (63, 15, 57.743, 191.395, 0.5, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- devil 07 is track id 67 from Devil Devil 07 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (67, 0, -508.773, 288.834, 236.4, 5),
    (67, 1, -508.773, 293.789, 236.4, 5),
    (67, 2, -508.773, 298.743, 236.4, 5),
    (67, 3, -508.773, 303.698, 236.4, 5),
    (67, 4, -508.773, 308.652, 236.4, 5),
    (67, 5, -508.773, 313.606, 236.4, 5),
    (67, 6, -508.773, 318.561, 236.4, 5),
    (67, 7, -508.773, 323.515, 236.4, 5),
    (67, 8, -502.859, 291.312, 236.4, 5),
    (67, 9, -502.859, 296.266, 236.4, 5),
    (67, 10, -502.859, 301.22, 236.4, 5),
    (67, 11, -502.859, 306.175, 236.4, 5),
    (67, 12, -502.859, 311.129, 236.4, 5),
    (67, 13, -502.859, 316.083, 236.4, 5),
    (67, 14, -502.859, 321.038, 236.4, 5),
    (67, 15, -502.859, 325.992, 236.4, 5)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- swamp 01 is track id 70 from Swamp Swamp 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (70, 0, 122.556, -15.244, 1.007, 0),
    (70, 1, 122.556, -8.769, 1.007, 0),
    (70, 2, 122.556, -2.256, 1.007, 0),
    (70, 3, 122.556, 4.2, 1.007, 0),
    (70, 4, 122.556, 10.667, 1.007, 0),
    (70, 5, 122.556, 17.152, 1.007, 0),
    (70, 6, 122.556, 23.625, 1.007, 0),
    (70, 7, 122.556, 30.104, 1.007, 0),
    (70, 8, 127.592, -11.993, 1.007, 0),
    (70, 9, 127.592, -5.519, 1.007, 0),
    (70, 10, 127.592, 0.995, 1.007, 0),
    (70, 11, 127.592, 7.45, 1.007, 0),
    (70, 12, 127.592, 13.918, 1.007, 0),
    (70, 13, 127.592, 20.403, 1.007, 0),
    (70, 14, 127.592, 26.876, 1.007, 0),
    (70, 15, 127.592, 33.355, 1.007, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- swamp 02 is track id 71 from Swamp Swamp 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (71, 0, -109.071, 188.722, 0.907, 0),
    (71, 1, -109.071, 195.197, 0.907, 0),
    (71, 2, -109.071, 201.711, 0.907, 0),
    (71, 3, -109.071, 208.166, 0.907, 0),
    (71, 4, -109.071, 214.634, 0.907, 0),
    (71, 5, -109.071, 221.119, 0.907, 0),
    (71, 6, -109.071, 227.592, 0.907, 0),
    (71, 7, -109.071, 234.07, 0.907, 0),
    (71, 8, -104.036, 191.973, 0.907, 0),
    (71, 9, -104.036, 198.448, 0.907, 0),
    (71, 10, -104.036, 204.962, 0.907, 0),
    (71, 11, -104.036, 211.417, 0.907, 0),
    (71, 12, -104.036, 217.885, 0.907, 0),
    (71, 13, -104.036, 224.37, 0.907, 0),
    (71, 14, -104.036, 230.843, 0.907, 0),
    (71, 15, -104.036, 237.321, 0.907, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- swamp 03 is track id 72 from Swamp Swamp 03 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (72, 0, 66.259, -25.873, 7.958, 0),
    (72, 1, 66.259, -19.399, 7.958, 0),
    (72, 2, 66.259, -12.885, 7.958, 0),
    (72, 3, 66.259, -6.43, 7.958, 0),
    (72, 4, 66.259, 0.038, 7.958, 0),
    (72, 5, 66.259, 6.523, 7.958, 0),
    (72, 6, 66.259, 12.996, 7.958, 0),
    (72, 7, 66.259, 19.475, 7.958, 0),
    (72, 8, 71.295, -22.622, 8.097, 0),
    (72, 9, 71.295, -16.148, 8.097, 0),
    (72, 10, 71.295, -9.634, 8.097, 0),
    (72, 11, 71.295, -3.179, 8.097, 0),
    (72, 12, 71.295, 3.289, 8.097, 0),
    (72, 13, 71.295, 9.774, 8.097, 0),
    (72, 14, 71.295, 16.247, 8.097, 0),
    (72, 15, 71.295, 22.725, 8.097, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- race 01 is track id 80 from Race Race 01 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (80, 0, -321.305, 247.576, 1.16, 0),
    (80, 1, -321.305, 242.262, 1.16, 0),
    (80, 2, -321.305, 237.399, 1.16, 0),
    (80, 3, -321.305, 232.627, 1.16, 0),
    (80, 4, -321.305, 228.57, 1.16, 0),
    (80, 5, -321.305, 223.66, 1.16, 0),
    (80, 6, -321.305, 218.888, 1.16, 0),
    (80, 7, -321.305, 214.528, 1.16, 0),
    (80, 8, -316.545, 249.774, 1.16, 0),
    (80, 9, -316.545, 244.508, 1.16, 0),
    (80, 10, -316.545, 239.597, 1.16, 0),
    (80, 11, -316.545, 234.825, 1.16, 0),
    (80, 12, -316.545, 230.769, 1.16, 0),
    (80, 13, -316.545, 225.858, 1.16, 0),
    (80, 14, -316.545, 221.086, 1.16, 0),
    (80, 15, -316.545, 216.726, 1.16, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- race 02 is track id 81 from Race Race 02 with 16 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (81, 0, -10.365, 154.464, 1.16, 0),
    (81, 1, -10.365, 158.824, 1.16, 0),
    (81, 2, -10.365, 163.596, 1.16, 0),
    (81, 3, -10.365, 168.507, 1.16, 0),
    (81, 4, -10.365, 172.563, 1.16, 0),
    (81, 5, -10.365, 177.335, 1.16, 0),
    (81, 6, -10.365, 182.198, 1.16, 0),
    (81, 7, -10.365, 187.512, 1.16, 0),
    (81, 8, -5.605, 156.662, 1.16, 0),
    (81, 9, -5.605, 161.022, 1.16, 0),
    (81, 10, -5.605, 165.794, 1.16, 0),
    (81, 11, -5.605, 170.705, 1.16, 0),
    (81, 12, -5.605, 174.761, 1.16, 0),
    (81, 13, -5.605, 179.533, 1.16, 0),
    (81, 14, -5.605, 184.444, 1.16, 0),
    (81, 15, -5.605, 189.71, 1.16, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- license 01 is track id 90 from License License 01 with 8 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (90, 0, 91.456, -574.171, -1.328, 0),
    (90, 1, 91.456, -574.171, -1.328, 0),
    (90, 2, 91.456, -574.171, -1.328, 0),
    (90, 3, 91.456, -574.171, -1.328, 0),
    (90, 4, 91.456, -574.171, -1.328, 0),
    (90, 5, 91.456, -574.171, -1.328, 0),
    (90, 6, 91.456, -574.171, -1.328, 0),
    (90, 7, 91.456, -574.171, -1.328, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- license 02 is track id 91 from License License 02 nif with 8 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (91, 0, 542.656, -176.453, 527.956, 0),
    (91, 1, 526.765, -196.355, 527.944, 0),
    (91, 2, 526.765, -196.355, 527.944, 0),
    (91, 3, 526.765, -196.355, 527.944, 0),
    (91, 4, 526.765, -196.355, 527.944, 0),
    (91, 5, 526.765, -196.355, 527.944, 0),
    (91, 6, 526.765, -196.355, 527.944, 0),
    (91, 7, 526.765, -196.355, 527.944, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- mission 01 is track id 100 from Mission Mission 01 with 1 row
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (100, 0, 870.398, -47.741, -88.451, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- mission 02 is track id 101 from Mission Mission 02 with 1 row
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (101, 0, 52.59, 0.051, 27.343, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- mission 03 is track id 102 from Mission Mission 03 with 8 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (102, 0, -724.622, -502.531, 0.109, 270),
    (102, 1, -705.108, -502.531, 0.109, 270),
    (102, 2, -714.934, -493.061, 0.106, 270),
    (102, 3, -714.934, -493.061, 0.106, 270),
    (102, 4, -714.934, -493.061, 0.106, 270),
    (102, 5, -714.934, -493.061, 0.106, 270),
    (102, 6, -714.934, -493.061, 0.106, 270),
    (102, 7, -714.934, -493.061, 0.106, 270)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- mission 04 is track id 103 from Mission Mission 04 with 9 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (103, 0, -0.068, 491.89, -11.9, 0),
    (103, 1, -0.068, 471.377, -11.9, 0),
    (103, 2, -0.068, 471.377, -11.9, 0),
    (103, 3, -0.068, 471.377, -11.9, 0),
    (103, 4, -0.068, 471.377, -11.9, 0),
    (103, 5, -0.068, 471.377, -11.9, 0),
    (103, 6, -0.068, 471.377, -11.9, 0),
    (103, 7, -0.068, 471.377, -11.9, 0),
    (103, 8, -0.068, 471.377, -11.9, 0)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- mission 05 is track id 104 from Mission Mission 05 with 9 rows
INSERT INTO track_spawn (track_id, grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg) VALUES
    (104, 0, 336.319, 75.309, 86.088, 270),
    (104, 1, 312.625, 75.309, 86.088, 270),
    (104, 2, 312.625, 75.309, 86.088, 270),
    (104, 3, 312.625, 75.309, 86.088, 270),
    (104, 4, 312.625, 75.309, 86.088, 270),
    (104, 5, 312.625, 75.309, 86.088, 270),
    (104, 6, 312.625, 75.309, 86.088, 270),
    (104, 7, 312.625, 75.309, 86.088, 270),
    (104, 8, 312.625, 75.309, 86.088, 270)
ON DUPLICATE KEY UPDATE
    spawn_x = VALUES(spawn_x), spawn_y = VALUES(spawn_y),
    spawn_z = VALUES(spawn_z), spawn_yaw_deg = VALUES(spawn_yaw_deg);

-- license 03 maps id 92 has no start ini of its own so no rows are written for it
