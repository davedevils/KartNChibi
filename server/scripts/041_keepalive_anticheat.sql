-- 0x12E file check ships empty since FUN 00403A10 s checksum algorithm is unconfirmed

CREATE TABLE IF NOT EXISTS client_file_manifest (
    id                INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    relative_path     VARCHAR(11) NOT NULL COMMENT 'cstr, client dest is 12 bytes, e.g. pak001.dat',
    expected_checksum INT UNSIGNED NOT NULL COMMENT 'compared against the client FUN_00403a10(file) result',
    UNIQUE KEY ux_client_file_manifest_path (relative_path)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
