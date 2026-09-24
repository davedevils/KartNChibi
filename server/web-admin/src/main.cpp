
#include "WebServer.h"
#include "logging/Logger.h"
#include "config/Config.h"
#include "db/Database.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

knc::WebServer* g_server = nullptr;

void signalHandler(int) {
    if (g_server) {
        g_server->stop();
    }
}

// old leaked default rejected outright no server may boot with it
static const char* const kLeakedApiToken = "admin123";

// reads the admin api token from the environment first then the config
static std::string resolveApiToken(knc::Config& config) {
    if (const char* env = std::getenv("ADMIN_TOKEN")) {
        if (*env) return std::string(env);
    }
    return config.get<std::string>("api.token", "");
}

int main(int argc, char* argv[]) {
    std::string configPath = "config/webadmin.json";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }
    
    knc::Logger::instance().init("logs/webadmin.log", knc::LogLevel::LVL_DEBUG);
    LOG_INFO("MAIN", "=== KnC Web Admin Starting ===");

    if (!knc::Config::instance().load(configPath)) {
        LOG_WARN("MAIN", "Config not found, using defaults");
    }
    
    auto& config = knc::Config::instance();

    // fail closed if the admin token is missing or still the old default
    std::string apiToken = resolveApiToken(config);
    if (apiToken.empty() || apiToken == kLeakedApiToken) {
        LOG_ERROR("MAIN", "ADMIN_TOKEN is not set or still the old default, refusing to start");
        return 1;
    }

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
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    knc::WebServer server;
    g_server = &server;

    server.setApiToken(apiToken);

    knc::ServerStats stats;
    stats.serverName = config.get<std::string>("server.name", "KnC Server");
    server.updateStats(stats);
    
    int port = config.get("server.port", 8080);
    LOG_INFO("MAIN", "Starting web admin on port " + std::to_string(port));
    
    // starts the server blocking
    server.start(port, false);
    
    LOG_INFO("MAIN", "Shutting down...");
    knc::Database::instance().shutdown();
    
    return 0;
}

