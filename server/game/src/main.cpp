/**
 * @file main.cpp
 * @brief GameServer entry point
 */

#include "GameServer.h"
#include "logging/Logger.h"
#include "config/IniConfig.h"
#include "db/Database.h"
#include "net/Packet.h"
#include <asio.hpp>
#include <iostream>
#include <vector>

// Check all required tables exist
bool checkDatabaseHealth(knc::Database& db) {
    const std::vector<std::string> requiredTables = {
        "accounts",
        "characters", 
        "vehicles",
        "items",
        "accessories",
        "servers",
        "active_sessions",
        "bans",
        "game_logs"
    };
    
    LOG_INFO("DB", "Running database health check...");
    
    bool allOk = true;
    for (const auto& table : requiredTables) {
        auto result = db.query("SHOW TABLES LIKE '" + table + "'");
        if (result.empty()) {
            LOG_ERROR("DB", "Missing required table: " + table);
            allOk = false;
        } else {
            LOG_DEBUG("DB", "Table OK: " + table);
        }
    }
    
    if (allOk) {
        LOG_INFO("DB", "Health check passed - all " + std::to_string(requiredTables.size()) + " tables present");
    } else {
        LOG_ERROR("DB", "Health check FAILED - run scripts/init.sql to create missing tables");
    }
    
    return allOk;
}

// Register this GameServer with the LoginServer
bool registerWithLoginServer(const knc::IniConfig& config) {
    std::string loginHost = config.getString("LoginServer.host", "127.0.0.1");
    int loginPort = config.getInt("LoginServer.port", 50017);
    std::string key = config.getString("LoginServer.key", "knc_internal_key_2025");
    
    int serverId = config.getInt("Server.id", 1);
    std::string name = config.getString("Server.name", "KnC Server");
    std::string host = config.getString("Server.host", "127.0.0.1");
    int port = config.getInt("Server.port", 50018);
    int maxPlayers = config.getInt("Server.max_players", 100);
    int serverType = config.getInt("Server.type", 0);
    
    LOG_INFO("MAIN", "Registering with LoginServer " + loginHost + ":" + std::to_string(loginPort));
    
    try {
        asio::io_context io;
        asio::ip::tcp::socket socket(io);
        asio::ip::tcp::resolver resolver(io);
        
        auto endpoints = resolver.resolve(loginHost, std::to_string(loginPort));
        asio::connect(socket, endpoints);
        
        // Build registration packet
        knc::Packet pkt(knc::CMD::I_SERVER_REGISTER);
        pkt.writeString(key);
        pkt.writeInt32(serverId);
        pkt.writeString(name);
        pkt.writeString(host);
        pkt.writeInt32(port);
        pkt.writeInt32(maxPlayers);
        pkt.writeInt32(serverType);
        
        auto data = pkt.serialize();
        asio::write(socket, asio::buffer(data));
        
        // Read response
        uint8_t respHeader[8];
        asio::read(socket, asio::buffer(respHeader, 8));
        
        uint16_t respSize = *reinterpret_cast<uint16_t*>(respHeader);
        if (respSize > 0) {
            std::vector<uint8_t> respData(respSize);
            asio::read(socket, asio::buffer(respData));
            
            if (respData[0] == 1) {
                LOG_INFO("MAIN", "Successfully registered with LoginServer");
                socket.close();
                return true;
            }
        }
        
        LOG_ERROR("MAIN", "LoginServer rejected registration");
        socket.close();
        return false;
        
    } catch (const std::exception& e) {
        LOG_ERROR("MAIN", "Failed to register with LoginServer: " + std::string(e.what()));
        return false;
    }
}

int main(int argc, char* argv[]) {
    // Print header
    knc::PrintServerHeader("KnC GameServer ", "1.0");
    
    std::string configPath = "config/gameserver.ini";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }
    
    // Load INI config
    knc::IniConfig config;
    if (!config.load(configPath)) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }
    
    // Setup logging
    std::string logFile = config.getString("Logging.file", "logs/gameserver.log");
    std::string logLevel = config.getString("Logging.level", "INFO");
    knc::LogLevel level = knc::LogLevel::LVL_INFO;
    if (logLevel == "DEBUG") level = knc::LogLevel::LVL_DEBUG;
    else if (logLevel == "WARN") level = knc::LogLevel::LVL_WARN;
    else if (logLevel == "ERROR") level = knc::LogLevel::LVL_ERROR;
    
    knc::Logger::instance().init(logFile, level);
    
    std::string serverName = config.getString("Server.name", "KnC Server");
    int port = config.getInt("Server.port", 50018);
    
    LOG_INFO("MAIN", "=== " + serverName + " Starting ===");
    
    // Initialize database
    knc::DBConfig dbConfig;
    dbConfig.host = config.getString("Database.host", "localhost");
    dbConfig.port = config.getInt("Database.port", 3306);
    dbConfig.database = config.getString("Database.name", "knc_emu");
    dbConfig.user = config.getString("Database.user", "knc");
    dbConfig.password = config.getString("Database.password", "knc_password");
    dbConfig.poolSize = 5;
    
    LOG_INFO("MAIN", "Connecting to database " + dbConfig.host + ":" + std::to_string(dbConfig.port) + "/" + dbConfig.database);
    
    if (!knc::Database::instance().init(dbConfig)) {
        LOG_ERROR("MAIN", "Database connection failed!");
        return 1;
    }
    
    // Health check - verify all tables exist
    if (!checkDatabaseHealth(knc::Database::instance())) {
        LOG_ERROR("MAIN", "Database health check failed - aborting startup");
        return 1;
    }
    
    // Register with LoginServer
    if (!registerWithLoginServer(config)) {
        LOG_WARN("MAIN", "Running without LoginServer registration - server won't appear in list");
    }
    
    try {
        knc::GameServer server(port);
        LOG_INFO("MAIN", serverName + " listening on port " + std::to_string(port));
        server.run();
    } catch (const std::exception& e) {
        LOG_ERROR("MAIN", std::string("Fatal error: ") + e.what());
        return 1;
    }
    
    // Cleanup
    knc::Database::instance().shutdown();
    
    return 0;
}
