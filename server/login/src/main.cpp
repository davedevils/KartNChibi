#include "LoginServer.h"
#include "RegisterWeb.h"

#include <cstdlib>
#include "logging/Logger.h"
#include "config/IniConfig.h"
#include "db/Database.h"
#include <iostream>
#include <vector>
#include <map>

// Table creation SQL for auto-setup
const std::map<std::string, std::string> TABLE_SCHEMAS = {
    {"accounts", R"(
        CREATE TABLE accounts (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(32) NOT NULL UNIQUE,
            password_hash VARCHAR(255) NOT NULL,
            email VARCHAR(255),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP NULL,
            is_banned TINYINT(1) DEFAULT 0,
            ban_reason VARCHAR(255),
            ban_expires_at TIMESTAMP NULL,
            INDEX idx_username (username)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"characters", R"(
        CREATE TABLE characters (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            account_id INT UNSIGNED NOT NULL,
            name VARCHAR(32) NOT NULL UNIQUE,
            level INT UNSIGNED DEFAULT 1,
            experience INT UNSIGNED DEFAULT 0,
            gold INT UNSIGNED DEFAULT 10000,
            cash INT UNSIGNED DEFAULT 0,
            wins INT UNSIGNED DEFAULT 0,
            losses INT UNSIGNED DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_played TIMESTAMP NULL,
            INDEX idx_account (account_id),
            INDEX idx_name (name)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"owned_kart", R"(
        CREATE TABLE owned_kart (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            character_id INT UNSIGNED NOT NULL,
            base_key INT UNSIGNED NOT NULL,
            skin_primary INT UNSIGNED NOT NULL DEFAULT 9007,
            skin_secondary INT UNSIGNED NOT NULL DEFAULT 9100,
            skin_tertiary INT UNSIGNED NOT NULL DEFAULT 0,
            custom3 INT UNSIGNED NOT NULL DEFAULT 0,
            custom4 INT UNSIGNED NOT NULL DEFAULT 0,
            custom5 INT UNSIGNED NOT NULL DEFAULT 0,
            applied_item_a INT UNSIGNED NOT NULL DEFAULT 0,
            applied_item_b INT UNSIGNED NOT NULL DEFAULT 0,
            price_key INT UNSIGNED NOT NULL DEFAULT 0,
            period_mode INT UNSIGNED NOT NULL DEFAULT 3,
            period_value INT UNSIGNED NOT NULL DEFAULT 100,
            active_flag INT UNSIGNED NOT NULL DEFAULT 1,
            up_speed INT NOT NULL DEFAULT 0,
            up_accel INT NOT NULL DEFAULT 0,
            up_handling INT NOT NULL DEFAULT 0,
            up_drift INT NOT NULL DEFAULT 0,
            up_boost INT NOT NULL DEFAULT 0,
            up_weight INT NOT NULL DEFAULT 0,
            up_special INT NOT NULL DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE KEY uq_owned_kart (character_id, base_key),
            INDEX idx_owned_kart_char (character_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"items", R"(
        CREATE TABLE items (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            character_id INT UNSIGNED NOT NULL,
            item_type_id INT UNSIGNED NOT NULL,
            quantity INT UNSIGNED DEFAULT 1,
            slot INT DEFAULT 0,
            equipped TINYINT(1) DEFAULT 0,
            INDEX idx_character (character_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"accessories", R"(
        CREATE TABLE accessories (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            character_id INT UNSIGNED NOT NULL,
            accessory_type_id INT UNSIGNED NOT NULL,
            slot INT DEFAULT 0,
            bonus1 INT DEFAULT 0,
            bonus2 INT DEFAULT 0,
            bonus3 INT DEFAULT 0,
            equipped TINYINT(1) DEFAULT 0,
            INDEX idx_character (character_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"servers", R"(
        CREATE TABLE servers (
            id INT UNSIGNED PRIMARY KEY,
            name VARCHAR(64) NOT NULL,
            host VARCHAR(64) NOT NULL,
            port INT UNSIGNED NOT NULL,
            type VARCHAR(16) DEFAULT 'game',
            max_players INT UNSIGNED DEFAULT 100,
            is_online TINYINT(1) DEFAULT 0,
            INDEX idx_type (type)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"online_players", R"(
        CREATE TABLE online_players (
            account_id INT UNSIGNED NOT NULL PRIMARY KEY,
            character_id INT UNSIGNED NOT NULL DEFAULT 0,
            game_session INT UNSIGNED NOT NULL DEFAULT 0,
            since TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            last_seen TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"active_sessions", R"(
        CREATE TABLE active_sessions (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            account_id INT UNSIGNED NOT NULL,
            character_id INT UNSIGNED NOT NULL,
            token VARCHAR(64) NOT NULL,
            client_ip VARCHAR(45) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_account (account_id),
            INDEX idx_ip (client_ip),
            INDEX idx_token (token)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"login_ticket", R"(
        CREATE TABLE login_ticket (
            account_id INT UNSIGNED NOT NULL PRIMARY KEY,
            token VARCHAR(64) NOT NULL,
            character_id INT UNSIGNED NOT NULL DEFAULT 0,
            issued_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_login_ticket_token (token)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"bans", R"(
        CREATE TABLE bans (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            ip_address VARCHAR(45),
            account_id INT UNSIGNED,
            reason VARCHAR(255),
            banned_by VARCHAR(32),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            expires_at TIMESTAMP NULL,
            INDEX idx_ip (ip_address),
            INDEX idx_account (account_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"},
    {"game_logs", R"(
        CREATE TABLE game_logs (
            id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            log_type VARCHAR(32) NOT NULL,
            player_id INT UNSIGNED,
            data TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_type (log_type),
            INDEX idx_player (player_id),
            INDEX idx_created (created_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"}
};

// checks required tables exist creates missing ones
bool checkAndSetupDatabase(knc::Database& db) {
    LOG_INFO("DB", "Running database health check...");
    
    int created = 0;
    int existing = 0;
    
    for (const auto& [tableName, createSql] : TABLE_SCHEMAS) {
        auto result = db.query("SHOW TABLES LIKE '" + tableName + "'");
        if (result.empty()) {
            LOG_WARN("DB", "Missing table: " + tableName + " - creating...");
            if (db.execute(createSql)) {
                LOG_INFO("DB", "Created table: " + tableName);
                created++;
            } else {
                LOG_ERROR("DB", "Failed to create table: " + tableName);
                return false;
            }
        } else {
            LOG_DEBUG("DB", "Table OK: " + tableName);
            existing++;
        }
    }
    
    LOG_INFO("DB", "Health check complete - " + std::to_string(existing) + " existing, " + 
             std::to_string(created) + " created");
    return true;
}

int main(int argc, char* argv[]) {
    knc::PrintServerHeader("KnC LoginServer", "1.0");

    std::string configPath = "config/loginserver.ini";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    // load config first for log settings
    knc::IniConfig config;
    if (!config.load(configPath)) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }
    
    std::string logFile = config.getString("Logging.file", "logs/login.log");
    std::string logLevel = config.getString("Logging.level", "INFO");
    knc::LogLevel level = knc::LogLevel::LVL_INFO;
    if (logLevel == "DEBUG") level = knc::LogLevel::LVL_DEBUG;
    else if (logLevel == "WARN") level = knc::LogLevel::LVL_WARN;
    else if (logLevel == "ERROR") level = knc::LogLevel::LVL_ERROR;
    
    knc::Logger::instance().init(logFile, level);
    LOG_INFO("MAIN", "=== KnC LoginServer Starting ===");

    // the signup page runs in this process via vendored header only cpp httplib off unless KNC WEB PORT is set
    {
        const char* wp = std::getenv("KNC_WEB_PORT");
        const uint16_t webPort = wp ? static_cast<uint16_t>(std::atoi(wp)) : 0;
        if (webPort != 0) {
            const char* sn = std::getenv("KNC_SITE_NAME");
            knc::StartRegisterWeb(webPort, sn && *sn ? sn : "Kart N Chibi");
        }
    }
    
    knc::DBConfig dbConfig;
    dbConfig.host = config.getString("Database.host", "localhost");
    dbConfig.port = config.getInt("Database.port", 3306);
    dbConfig.database = config.getString("Database.name", "knc_emu");
    dbConfig.user = config.getString("Database.user", "knc");
    dbConfig.password = config.getString("Database.password", "knc_password");
    dbConfig.poolSize = config.getInt("Database.pool_size", 5);
    
    LOG_INFO("MAIN", "Connecting to database " + dbConfig.host + ":" + 
             std::to_string(dbConfig.port) + "/" + dbConfig.database);
    
    if (!knc::Database::instance().init(dbConfig)) {
        LOG_ERROR("MAIN", "Database connection failed!");
        return 1;
    }
    
    if (!checkAndSetupDatabase(knc::Database::instance())) {
        LOG_ERROR("MAIN", "Database setup failed - aborting startup");
        return 1;
    }

    int port = config.getInt("Server.port", 50017);

    try {
        knc::LoginServer server(port);
        LOG_INFO("MAIN", "LoginServer listening on port " + std::to_string(port));
        server.run();
    } catch (const std::exception& e) {
        LOG_ERROR("MAIN", std::string("Fatal error: ") + e.what());
        return 1;
    }
    
    knc::Database::instance().shutdown();
    
    return 0;
}
