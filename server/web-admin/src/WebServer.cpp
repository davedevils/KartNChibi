/**
 * @file WebServer.cpp
 * @brief HTTP server for admin panel using cpp-httplib
 */

// Must define Windows version before including httplib
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10
#endif
#endif

#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#include "httplib.h"
#include "WebServer.h"
#include "AdminAPI.h"
#include "logging/Logger.h"
#include "db/Database.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace knc {

WebServer::WebServer() : m_server(std::make_unique<httplib::Server>()) {}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start(int port, bool async) {
    if (m_running) return false;
    
    setupRoutes();
    
    if (async) {
        m_thread = std::thread([this, port]() {
            m_running = true;
            LOG_INFO("WEB", "Admin panel starting on http://localhost:" + std::to_string(port));
            m_server->listen("0.0.0.0", port);
            m_running = false;
        });
        
        // Wait a bit for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return m_running;
    } else {
        m_running = true;
        LOG_INFO("WEB", "Admin panel starting on http://localhost:" + std::to_string(port));
        m_server->listen("0.0.0.0", port);
        m_running = false;
        return true;
    }
}

void WebServer::stop() {
    if (m_running && m_server) {
        m_server->stop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_running = false;
    }
}

void WebServer::updateStats(const ServerStats& stats) {
    m_stats = stats;
}

bool WebServer::checkAuth(const std::string& authHeader) {
    // Format: "Bearer <token>"
    if (authHeader.substr(0, 7) != "Bearer ") return false;
    return authHeader.substr(7) == m_apiToken;
}

void WebServer::setupRoutes() {
    // Serve static files - try multiple paths
    bool mounted = false;
    for (const auto& dir : {m_staticDir, std::string("./static"), std::string("static")}) {
        if (m_server->set_mount_point("/", dir)) {
            LOG_INFO("WEB", "Serving static files from: " + dir);
            mounted = true;
            break;
        }
    }
    if (!mounted) {
        LOG_WARN("WEB", "Could not mount static files directory");
    }
    
    // Explicit index route as fallback
    m_server->Get("/", [this](const httplib::Request&, httplib::Response& res) {
        std::ifstream file(m_staticDir + "/index.html");
        if (!file.is_open()) {
            file.open("./static/index.html");
        }
        if (!file.is_open()) {
            file.open("static/index.html");
        }
        
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.set_content("<h1>KnC Admin</h1><p>Static files not found. API is working at /api/status</p>", "text/html");
        }
    });
    
    // ============================================================
    // API: Status
    // ============================================================
    m_server->Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"players", m_stats.playersOnline},
            {"rooms", m_stats.activeRooms},
            {"accounts", m_stats.totalAccounts},
            {"packetsPerSec", m_stats.packetsPerSec},
            {"uptime", m_stats.uptime},
            {"serverName", m_stats.serverName}
        };
        
        // Get account count from DB
        auto results = Database::instance().query("SELECT COUNT(*) as cnt FROM accounts");
        if (!results.empty()) {
            response["accounts"] = std::stoi(results[0]["cnt"]);
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    // ============================================================
    // API: Accounts
    // ============================================================
    m_server->Get("/api/accounts", [this](const httplib::Request& req, httplib::Response& res) {
        int limit = 50;
        int offset = 0;
        
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        if (req.has_param("offset")) offset = std::stoi(req.get_param_value("offset"));
        
        std::string sql = "SELECT id, username, email, created_at, last_login, is_banned, ban_reason "
                         "FROM accounts ORDER BY id DESC LIMIT " + std::to_string(limit) + 
                         " OFFSET " + std::to_string(offset);
        
        auto results = Database::instance().query(sql);
        
        json accounts = json::array();
        for (auto& row : results) {
            accounts.push_back({
                {"id", std::stoi(row["id"])},
                {"username", row["username"]},
                {"email", row["email"]},
                {"created_at", row["created_at"]},
                {"last_login", row["last_login"]},
                {"is_banned", row["is_banned"] == "1"},
                {"ban_reason", row["ban_reason"]}
            });
        }
        
        res.set_content(json{{"accounts", accounts}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Characters
    // ============================================================
    m_server->Get("/api/characters", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 50;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT c.id, c.name, c.level, c.gold, c.wins, c.losses, a.username "
                         "FROM characters c JOIN accounts a ON c.account_id = a.id "
                         "ORDER BY c.level DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json characters = json::array();
        for (auto& row : results) {
            characters.push_back({
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"level", std::stoi(row["level"])},
                {"gold", std::stoi(row["gold"])},
                {"wins", std::stoi(row["wins"])},
                {"losses", std::stoi(row["losses"])},
                {"account", row["username"]}
            });
        }
        
        res.set_content(json{{"characters", characters}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Ban account
    // ============================================================
    m_server->Post("/api/ban", [this](const httplib::Request& req, httplib::Response& res) {
        // Check auth
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int accountId = body["account_id"];
            std::string reason = body.value("reason", "No reason given");
            bool permanent = body.value("permanent", false);
            
            std::string sql = "UPDATE accounts SET is_banned = 1, ban_reason = '" + reason + "' "
                             "WHERE id = " + std::to_string(accountId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Banned account ID " + std::to_string(accountId) + ": " + reason);
                res.set_content(R"({"success":true})", "application/json");
            } else {
                res.status = 500;
                res.set_content(R"({"error":"Database error"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // ============================================================
    // API: Unban account
    // ============================================================
    m_server->Delete(R"(/api/ban/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string accountId = req.matches[1];
        std::string sql = "UPDATE accounts SET is_banned = 0, ban_reason = NULL WHERE id = " + accountId;
        
        if (Database::instance().execute(sql)) {
            LOG_INFO("WEB", "Unbanned account ID " + accountId);
            res.set_content(R"({"success":true})", "application/json");
        } else {
            res.status = 500;
            res.set_content(R"({"error":"Database error"})", "application/json");
        }
    });
    
    // ============================================================
    // API: Recent logs
    // ============================================================
    m_server->Get("/api/logs", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 100;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT id, character_id, event_type, event_data, created_at "
                         "FROM game_logs ORDER BY id DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json logs = json::array();
        for (auto& row : results) {
            logs.push_back({
                {"id", row["id"]},
                {"character_id", row["character_id"]},
                {"event_type", row["event_type"]},
                {"event_data", row["event_data"]},
                {"created_at", row["created_at"]}
            });
        }
        
        res.set_content(json{{"logs", logs}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Server config
    // ============================================================
    m_server->Get("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        // Return safe config info (not passwords)
        json config = {
            {"serverName", m_stats.serverName},
            {"maxPlayers", 100},
            {"loginPort", 50017},
            {"gamePort", 50018},
            {"webPort", 8080}
        };
        
        res.set_content(config.dump(), "application/json");
    });
    
    LOG_DEBUG("WEB", "Routes configured");
}

} // namespace knc
