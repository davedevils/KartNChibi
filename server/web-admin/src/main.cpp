/**
 * @file main.cpp
 * @brief Standalone Web Admin server
 */

#include "WebServer.h"
#include "logging/Logger.h"
#include "config/Config.h"
#include "db/Database.h"
#include <iostream>
#include <csignal>

knc::WebServer* g_server = nullptr;

void signalHandler(int) {
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // Parse command line
    std::string configPath = "config/webadmin.json";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }
    
    // Initialize logger
    knc::Logger::instance().init("logs/webadmin.log", knc::LogLevel::LVL_DEBUG);
    LOG_INFO("MAIN", "=== KnC Web Admin Starting ===");
    
    // Load config
    if (!knc::Config::instance().load(configPath)) {
        LOG_WARN("MAIN", "Config not found, using defaults");
    }
    
    auto& config = knc::Config::instance();
    
    // Initialize database
    knc::DBConfig dbConfig;
    dbConfig.host = config.get<std::string>("database.host", "localhost");
    dbConfig.port = config.get("database.port", 3306);
    dbConfig.database = config.get<std::string>("database.name", "knc_emu");
    dbConfig.user = config.get<std::string>("database.user", "knc");
    dbConfig.password = config.get<std::string>("database.password", "knc_password");
    dbConfig.poolSize = config.get("database.pool_size", 3);
    
    LOG_INFO("MAIN", "Connecting to database...");
    if (!knc::Database::instance().init(dbConfig)) {
        LOG_ERROR("MAIN", "Database connection failed!");
        return 1;
    }
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Create and start web server
    knc::WebServer server;
    g_server = &server;
    
    // Configure
    server.setApiToken(config.get<std::string>("api.token", "admin123"));
    
    // Update initial stats
    knc::ServerStats stats;
    stats.serverName = config.get<std::string>("server.name", "KnC Server");
    server.updateStats(stats);
    
    int port = config.get("server.port", 8080);
    LOG_INFO("MAIN", "Starting web admin on port " + std::to_string(port));
    
    // Start (blocking)
    server.start(port, false);
    
    // Cleanup
    LOG_INFO("MAIN", "Shutting down...");
    knc::Database::instance().shutdown();
    
    return 0;
}

