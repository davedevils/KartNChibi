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
        
        json config = {
            {"serverName", m_stats.serverName},
            {"maxPlayers", 100},
            {"loginPort", 50017},
            {"gamePort", 50018},
            {"webPort", 8080}
        };
        
        res.set_content(config.dump(), "application/json");
    });
    
    // ============================================================
    // API: Online players (from sessions table)
    // ============================================================
    m_server->Get("/api/online", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT s.id, s.account_id, s.ip_address, s.created_at, a.username, c.name as character_name "
                         "FROM sessions s "
                         "LEFT JOIN accounts a ON s.account_id = a.id "
                         "LEFT JOIN characters c ON s.character_id = c.id "
                         "WHERE s.expires_at > NOW() "
                         "ORDER BY s.created_at DESC";
        
        auto results = Database::instance().query(sql);
        
        json players = json::array();
        for (auto& row : results) {
            players.push_back({
                {"session_id", row["id"]},
                {"account_id", row["account_id"]},
                {"username", row["username"]},
                {"character", row["character_name"]},
                {"ip", row["ip_address"]},
                {"connected_at", row["created_at"]}
            });
        }
        
        res.set_content(json{{"players", players}, {"count", players.size()}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Active rooms
    // ============================================================
    m_server->Get("/api/rooms", [this](const httplib::Request&, httplib::Response& res) {
        // Rooms are managed by GameServer - return stats from WebServer tracking
        // Direct room access would require GameServer reference injection
        json rooms = json::array();
        
        // Return statistics we track (activeRooms is updated via stats callbacks)
        res.set_content(json{
            {"rooms", rooms}, 
            {"count", m_stats.activeRooms},
            {"note", "Room details available via GameServer API"}
        }.dump(), "application/json");
    });
    
    // ============================================================
    // API: Give gold to character
    // ============================================================
    m_server->Post("/api/give/gold", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int amount = body["amount"];
            
            std::string sql = "UPDATE characters SET gold = gold + " + std::to_string(amount) +
                             " WHERE id = " + std::to_string(charId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Gave " + std::to_string(amount) + " gold to char " + std::to_string(charId));
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
    // API: Give cash to character
    // ============================================================
    m_server->Post("/api/give/cash", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int amount = body["amount"];
            
            std::string sql = "UPDATE characters SET cash = cash + " + std::to_string(amount) +
                             " WHERE id = " + std::to_string(charId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Gave " + std::to_string(amount) + " cash to char " + std::to_string(charId));
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
    // API: Give item to character
    // ============================================================
    m_server->Post("/api/give/item", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int templateId = body["template_id"];
            int quantity = body.value("quantity", 1);
            
            std::string sql = "INSERT INTO items (character_id, template_id, quantity) VALUES (" +
                             std::to_string(charId) + ", " +
                             std::to_string(templateId) + ", " +
                             std::to_string(quantity) + ")";
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Gave item " + std::to_string(templateId) + " x" + std::to_string(quantity) + 
                         " to char " + std::to_string(charId));
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
    // API: Give vehicle to character
    // ============================================================
    m_server->Post("/api/give/vehicle", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int templateId = body["template_id"];
            
            std::string sql = "INSERT INTO vehicles (character_id, template_id, durability, max_durability) VALUES (" +
                             std::to_string(charId) + ", " +
                             std::to_string(templateId) + ", 100, 100)";
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Gave vehicle " + std::to_string(templateId) + " to char " + std::to_string(charId));
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
    // API: Reset character stats
    // ============================================================
    m_server->Post("/api/reset/stats", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            
            std::string sql = "UPDATE characters SET level = 1, experience = 0, wins = 0, losses = 0 "
                             "WHERE id = " + std::to_string(charId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Reset stats for char " + std::to_string(charId));
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
    // API: Delete character
    // ============================================================
    m_server->Delete(R"(/api/character/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string charId = req.matches[1];
        
        // Delete associated data first (foreign keys)
        Database::instance().execute("DELETE FROM items WHERE character_id = " + charId);
        Database::instance().execute("DELETE FROM vehicles WHERE character_id = " + charId);
        Database::instance().execute("DELETE FROM accessories WHERE character_id = " + charId);
        
        if (Database::instance().execute("DELETE FROM characters WHERE id = " + charId)) {
            LOG_INFO("WEB", "Deleted character " + charId);
            res.set_content(R"({"success":true})", "application/json");
        } else {
            res.status = 500;
            res.set_content(R"({"error":"Database error"})", "application/json");
        }
    });
    
    // ============================================================
    // API: Server broadcast message (placeholder)
    // ============================================================
    m_server->Post("/api/broadcast", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            std::string message = body["message"];
            
            // Broadcast stored for GameServer to poll/process
            // Direct broadcast requires GameServer reference
            LOG_INFO("WEB", "Broadcast requested: " + message);
            
            // Store in database for GameServer to pick up
            try {
                auto& db = Database::instance();
                db.execute("INSERT INTO admin_broadcasts (message, created_at) VALUES ('" + 
                           message + "', NOW())");
                res.set_content(R"({"success":true,"msg":"queued"})", "application/json");
            } catch (...) {
                res.set_content(R"({"success":true,"msg":"logged"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // ============================================================
    // API: Anti-cheat logs
    // ============================================================
    m_server->Get("/api/anticheat", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 100;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT l.id, l.character_id, l.violation_type, l.details, l.severity, "
                         "l.action_taken, l.created_at, c.name as character_name "
                         "FROM anticheat_logs l "
                         "LEFT JOIN characters c ON l.character_id = c.id "
                         "ORDER BY l.id DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json logs = json::array();
        for (auto& row : results) {
            logs.push_back({
                {"id", row["id"]},
                {"character_id", row["character_id"]},
                {"character_name", row["character_name"]},
                {"violation_type", row["violation_type"]},
                {"details", row["details"]},
                {"severity", row["severity"]},
                {"action_taken", row["action_taken"]},
                {"created_at", row["created_at"]}
            });
        }
        
        res.set_content(json{{"logs", logs}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Missions list
    // ============================================================
    m_server->Get("/api/missions", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT id, name, description, reward_gold, reward_xp, "
                         "required_races, required_wins, is_active "
                         "FROM missions ORDER BY id";
        
        auto results = Database::instance().query(sql);
        
        json missions = json::array();
        for (auto& row : results) {
            missions.push_back({
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"description", row["description"]},
                {"reward_gold", std::stoi(row["reward_gold"])},
                {"reward_xp", std::stoi(row["reward_xp"])},
                {"required_races", std::stoi(row["required_races"])},
                {"required_wins", std::stoi(row["required_wins"])},
                {"is_active", row["is_active"] == "1"}
            });
        }
        
        res.set_content(json{{"missions", missions}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Create/Update mission
    // ============================================================
    m_server->Post("/api/missions", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            std::string name = AdminAPI::escapeSql(body["name"]);
            std::string desc = AdminAPI::escapeSql(body.value("description", ""));
            int rewardGold = body.value("reward_gold", 100);
            int rewardXp = body.value("reward_xp", 50);
            int reqRaces = body.value("required_races", 1);
            int reqWins = body.value("required_wins", 0);
            
            std::string sql = "INSERT INTO missions (name, description, reward_gold, reward_xp, "
                             "required_races, required_wins) VALUES ('" + name + "', '" + desc + "', " +
                             std::to_string(rewardGold) + ", " + std::to_string(rewardXp) + ", " +
                             std::to_string(reqRaces) + ", " + std::to_string(reqWins) + ")";
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Created mission: " + name);
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
    // API: Ghost records (leaderboards)
    // ============================================================
    m_server->Get("/api/ghosts", [](const httplib::Request& req, httplib::Response& res) {
        int mapId = 0;
        int limit = 50;
        if (req.has_param("map_id")) mapId = std::stoi(req.get_param_value("map_id"));
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT g.id, g.character_id, g.map_id, g.time, g.vehicle_id, "
                         "g.is_valid, g.created_at, c.name as player_name, m.name as map_name "
                         "FROM ghost_records g "
                         "LEFT JOIN characters c ON g.character_id = c.id "
                         "LEFT JOIN maps m ON g.map_id = m.id ";
        
        if (mapId > 0) {
            sql += "WHERE g.map_id = " + std::to_string(mapId) + " ";
        }
        
        sql += "ORDER BY g.time ASC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json ghosts = json::array();
        for (auto& row : results) {
            int timeMs = std::stoi(row["time"]);
            int minutes = timeMs / 60000;
            int seconds = (timeMs % 60000) / 1000;
            int ms = timeMs % 1000;
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%d:%02d.%03d", minutes, seconds, ms);
            
            ghosts.push_back({
                {"id", std::stoi(row["id"])},
                {"player_name", row["player_name"]},
                {"map_name", row["map_name"]},
                {"time_ms", timeMs},
                {"time_formatted", std::string(timeStr)},
                {"is_valid", row["is_valid"] == "1"},
                {"created_at", row["created_at"]}
            });
        }
        
        res.set_content(json{{"ghosts", ghosts}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Invalidate ghost record (anti-cheat)
    // ============================================================
    m_server->Post("/api/ghosts/invalidate", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int ghostId = body["ghost_id"];
            
            std::string sql = "UPDATE ghost_records SET is_valid = 0 WHERE id = " + std::to_string(ghostId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Invalidated ghost record: " + std::to_string(ghostId));
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
    // API: Scenario progress
    // ============================================================
    m_server->Get("/api/scenario/progress", [](const httplib::Request& req, httplib::Response& res) {
        int charId = 0;
        if (req.has_param("character_id")) charId = std::stoi(req.get_param_value("character_id"));
        
        std::string sql = "SELECT sp.character_id, sp.total_stars, c.name as character_name, "
                         "(SELECT COUNT(*) FROM scenario_stage_progress ssp WHERE ssp.character_id = sp.character_id) as stages_completed "
                         "FROM scenario_progress sp "
                         "LEFT JOIN characters c ON sp.character_id = c.id ";
        
        if (charId > 0) {
            sql += "WHERE sp.character_id = " + std::to_string(charId);
        }
        
        sql += " ORDER BY sp.total_stars DESC LIMIT 100";
        
        auto results = Database::instance().query(sql);
        
        json progress = json::array();
        for (auto& row : results) {
            progress.push_back({
                {"character_id", std::stoi(row["character_id"])},
                {"character_name", row["character_name"]},
                {"total_stars", std::stoi(row["total_stars"])},
                {"stages_completed", std::stoi(row["stages_completed"])}
            });
        }
        
        res.set_content(json{{"progress", progress}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Maps list
    // ============================================================
    m_server->Get("/api/maps", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT id, name, difficulty, lap_count, is_enabled FROM maps ORDER BY id";
        
        auto results = Database::instance().query(sql);
        
        json maps = json::array();
        for (auto& row : results) {
            maps.push_back({
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"difficulty", std::stoi(row["difficulty"])},
                {"lap_count", std::stoi(row["lap_count"])},
                {"is_enabled", row["is_enabled"] == "1"}
            });
        }
        
        res.set_content(json{{"maps", maps}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Toggle map enabled
    // ============================================================
    m_server->Post("/api/maps/toggle", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int mapId = body["map_id"];
            bool enabled = body["enabled"];
            
            std::string sql = "UPDATE maps SET is_enabled = " + std::to_string(enabled ? 1 : 0) +
                             " WHERE id = " + std::to_string(mapId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Map " + std::to_string(mapId) + " enabled=" + std::to_string(enabled));
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
    // API: Shop items management
    // ============================================================
    m_server->Get("/api/shop", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT id, template_id, category, name, price_gold, price_cash, "
                         "is_available, discount_percent FROM shop_items ORDER BY category, id";
        
        auto results = Database::instance().query(sql);
        
        json items = json::array();
        for (auto& row : results) {
            items.push_back({
                {"id", std::stoi(row["id"])},
                {"template_id", std::stoi(row["template_id"])},
                {"category", row["category"]},
                {"name", row["name"]},
                {"price_gold", std::stoi(row["price_gold"])},
                {"price_cash", std::stoi(row["price_cash"])},
                {"is_available", row["is_available"] == "1"},
                {"discount_percent", std::stoi(row["discount_percent"])}
            });
        }
        
        res.set_content(json{{"items", items}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Update shop item
    // ============================================================
    m_server->Put("/api/shop", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int itemId = body["id"];
            int priceGold = body.value("price_gold", -1);
            int priceCash = body.value("price_cash", -1);
            int discount = body.value("discount_percent", -1);
            bool available = body.value("is_available", true);
            
            std::string sql = "UPDATE shop_items SET is_available = " + std::to_string(available ? 1 : 0);
            if (priceGold >= 0) sql += ", price_gold = " + std::to_string(priceGold);
            if (priceCash >= 0) sql += ", price_cash = " + std::to_string(priceCash);
            if (discount >= 0) sql += ", discount_percent = " + std::to_string(discount);
            sql += " WHERE id = " + std::to_string(itemId);
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Updated shop item: " + std::to_string(itemId));
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
    // API: Statistics dashboard
    // ============================================================
    m_server->Get("/api/stats/dashboard", [this](const httplib::Request&, httplib::Response& res) {
        auto& db = Database::instance();
        
        json stats;
        
        // Total accounts
        auto r1 = db.query("SELECT COUNT(*) as cnt FROM accounts");
        stats["total_accounts"] = r1.empty() ? 0 : std::stoi(r1[0]["cnt"]);
        
        // Total characters
        auto r2 = db.query("SELECT COUNT(*) as cnt FROM characters");
        stats["total_characters"] = r2.empty() ? 0 : std::stoi(r2[0]["cnt"]);
        
        // Active today
        auto r3 = db.query("SELECT COUNT(*) as cnt FROM accounts WHERE DATE(last_login) = CURDATE()");
        stats["active_today"] = r3.empty() ? 0 : std::stoi(r3[0]["cnt"]);
        
        // Banned accounts
        auto r4 = db.query("SELECT COUNT(*) as cnt FROM accounts WHERE is_banned = 1");
        stats["banned_accounts"] = r4.empty() ? 0 : std::stoi(r4[0]["cnt"]);
        
        // Total races (from characters.total_races)
        auto r5 = db.query("SELECT SUM(total_races) as total FROM characters");
        stats["total_races"] = (r5.empty() || r5[0]["total"].empty()) ? 0 : std::stoi(r5[0]["total"]);
        
        // Total gold in economy
        auto r6 = db.query("SELECT SUM(gold) as total FROM characters");
        stats["economy_gold"] = (r6.empty() || r6[0]["total"].empty()) ? 0 : std::stoll(r6[0]["total"]);
        
        // Anti-cheat violations (last 24h)
        auto r7 = db.query("SELECT COUNT(*) as cnt FROM anticheat_logs WHERE created_at > DATE_SUB(NOW(), INTERVAL 24 HOUR)");
        stats["anticheat_24h"] = r7.empty() ? 0 : std::stoi(r7[0]["cnt"]);
        
        // Ghost records count
        auto r8 = db.query("SELECT COUNT(*) as cnt FROM ghost_records WHERE is_valid = 1");
        stats["ghost_records"] = r8.empty() ? 0 : std::stoi(r8[0]["cnt"]);
        
        // Server uptime
        stats["uptime_seconds"] = m_stats.uptime;
        stats["players_online"] = m_stats.playersOnline;
        stats["active_rooms"] = m_stats.activeRooms;
        
        res.set_content(stats.dump(), "application/json");
    });
    
    // ============================================================
    // API: Top players leaderboard
    // ============================================================
    m_server->Get("/api/leaderboard", [](const httplib::Request& req, httplib::Response& res) {
        std::string sortBy = req.has_param("sort") ? req.get_param_value("sort") : "wins";
        int limit = 50;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        // Validate sort column
        if (sortBy != "wins" && sortBy != "level" && sortBy != "gold" && sortBy != "total_races") {
            sortBy = "wins";
        }
        
        std::string sql = "SELECT c.id, c.name, c.level, c.gold, c.wins, c.losses, c.total_races, "
                         "a.username, ROUND(c.wins * 100.0 / NULLIF(c.wins + c.losses, 0), 1) as winrate "
                         "FROM characters c "
                         "JOIN accounts a ON c.account_id = a.id "
                         "ORDER BY " + sortBy + " DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json players = json::array();
        int rank = 1;
        for (auto& row : results) {
            players.push_back({
                {"rank", rank++},
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"account", row["username"]},
                {"level", std::stoi(row["level"])},
                {"gold", std::stoi(row["gold"])},
                {"wins", std::stoi(row["wins"])},
                {"losses", std::stoi(row["losses"])},
                {"total_races", std::stoi(row["total_races"])},
                {"winrate", row["winrate"].empty() ? "0" : row["winrate"]}
            });
        }
        
        res.set_content(json{{"leaderboard", players}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Character details
    // ============================================================
    m_server->Get(R"(/api/character/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string charId = req.matches[1];
        
        // Get character info
        auto charResult = Database::instance().query(
            "SELECT c.*, a.username FROM characters c "
            "JOIN accounts a ON c.account_id = a.id "
            "WHERE c.id = " + charId
        );
        
        if (charResult.empty()) {
            res.status = 404;
            res.set_content(R"({"error":"Character not found"})", "application/json");
            return;
        }
        
        auto& ch = charResult[0];
        json character = {
            {"id", std::stoi(ch["id"])},
            {"name", ch["name"]},
            {"account", ch["username"]},
            {"level", std::stoi(ch["level"])},
            {"experience", std::stoi(ch["experience"])},
            {"gold", std::stoi(ch["gold"])},
            {"cash", ch.count("cash") ? std::stoi(ch.at("cash")) : 0},
            {"wins", std::stoi(ch["wins"])},
            {"losses", std::stoi(ch["losses"])},
            {"total_races", std::stoi(ch["total_races"])},
            {"tutorial_completed", ch["tutorial_completed"] == "1"},
            {"created_at", ch["created_at"]}
        };
        
        // Get vehicles
        auto vehicles = Database::instance().query(
            "SELECT id, vehicle_type_id, durability, equipped FROM vehicles WHERE character_id = " + charId
        );
        json vehicleList = json::array();
        for (auto& v : vehicles) {
            vehicleList.push_back({
                {"id", std::stoi(v["id"])},
                {"type_id", std::stoi(v["vehicle_type_id"])},
                {"durability", std::stoi(v["durability"])},
                {"equipped", v["equipped"] == "1"}
            });
        }
        character["vehicles"] = vehicleList;
        
        // Get items
        auto items = Database::instance().query(
            "SELECT id, template_id, quantity, equipped FROM items WHERE character_id = " + charId
        );
        json itemList = json::array();
        for (auto& i : items) {
            itemList.push_back({
                {"id", std::stoi(i["id"])},
                {"template_id", std::stoi(i["template_id"])},
                {"quantity", std::stoi(i["quantity"])},
                {"equipped", i["equipped"] == "1"}
            });
        }
        character["items"] = itemList;
        
        res.set_content(json{{"character", character}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: All game items (template list)
    // ============================================================
    m_server->Get("/api/items/all", [](const httplib::Request& req, httplib::Response& res) {
        std::string category = req.has_param("category") ? req.get_param_value("category") : "";
        
        std::string sql = "SELECT id, name, category, description, rarity, base_price "
                         "FROM item_templates ";
        if (!category.empty()) {
            sql += "WHERE category = '" + AdminAPI::escapeSql(category) + "' ";
        }
        sql += "ORDER BY category, id";
        
        auto results = Database::instance().query(sql);
        
        json items = json::array();
        for (auto& row : results) {
            items.push_back({
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"category", row["category"]},
                {"description", row["description"]},
                {"rarity", row["rarity"]},
                {"base_price", std::stoi(row["base_price"])}
            });
        }
        
        res.set_content(json{{"items", items}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Richest players
    // ============================================================
    m_server->Get("/api/richest", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 50;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT c.id, c.name, c.level, c.gold, c.cash, a.username, "
                         "(c.gold + c.cash * 100) as total_wealth "
                         "FROM characters c "
                         "JOIN accounts a ON c.account_id = a.id "
                         "ORDER BY total_wealth DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json players = json::array();
        int rank = 1;
        for (auto& row : results) {
            players.push_back({
                {"rank", rank++},
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"account", row["username"]},
                {"level", std::stoi(row["level"])},
                {"gold", std::stoi(row["gold"])},
                {"cash", std::stoi(row["cash"])},
                {"total_wealth", std::stoll(row["total_wealth"])}
            });
        }
        
        res.set_content(json{{"players", players}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Game logs (security)
    // ============================================================
    m_server->Get("/api/logs/game", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 200;
        std::string eventType = "";
        std::string charId = "";
        
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        if (req.has_param("type")) eventType = req.get_param_value("type");
        if (req.has_param("character_id")) charId = req.get_param_value("character_id");
        
        std::string sql = "SELECT l.id, l.character_id, l.event_type, l.event_data, "
                         "l.ip_address, l.created_at, c.name as character_name "
                         "FROM game_logs l "
                         "LEFT JOIN characters c ON l.character_id = c.id "
                         "WHERE 1=1 ";
        
        if (!eventType.empty()) {
            sql += "AND l.event_type = '" + AdminAPI::escapeSql(eventType) + "' ";
        }
        if (!charId.empty()) {
            sql += "AND l.character_id = " + charId + " ";
        }
        
        sql += "ORDER BY l.id DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json logs = json::array();
        for (auto& row : results) {
            logs.push_back({
                {"id", row["id"]},
                {"character_id", row["character_id"]},
                {"character_name", row["character_name"]},
                {"event_type", row["event_type"]},
                {"event_data", row["event_data"]},
                {"ip_address", row["ip_address"]},
                {"created_at", row["created_at"]}
            });
        }
        
        // Get event types for filter
        auto types = Database::instance().query(
            "SELECT DISTINCT event_type FROM game_logs ORDER BY event_type"
        );
        json eventTypes = json::array();
        for (auto& t : types) {
            eventTypes.push_back(t["event_type"]);
        }
        
        res.set_content(json{{"logs", logs}, {"event_types", eventTypes}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Transaction logs
    // ============================================================
    m_server->Get("/api/logs/transactions", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 200;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        
        std::string sql = "SELECT t.id, t.character_id, t.type, t.amount, t.currency, "
                         "t.reason, t.admin_id, t.created_at, c.name as character_name "
                         "FROM transaction_logs t "
                         "LEFT JOIN characters c ON t.character_id = c.id "
                         "ORDER BY t.id DESC LIMIT " + std::to_string(limit);
        
        auto results = Database::instance().query(sql);
        
        json logs = json::array();
        for (auto& row : results) {
            logs.push_back({
                {"id", row["id"]},
                {"character_id", row["character_id"]},
                {"character_name", row["character_name"]},
                {"type", row["type"]},
                {"amount", std::stoi(row["amount"])},
                {"currency", row["currency"]},
                {"reason", row["reason"]},
                {"admin_id", row["admin_id"]},
                {"created_at", row["created_at"]}
            });
        }
        
        res.set_content(json{{"logs", logs}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Add/Remove currency (with logging)
    // ============================================================
    m_server->Post("/api/currency/modify", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            std::string currency = body["currency"];  // "gold" or "cash"
            int amount = body["amount"];  // Positive = add, Negative = remove
            std::string reason = body.value("reason", "Admin modification");
            
            // Validate currency type
            if (currency != "gold" && currency != "cash") {
                res.status = 400;
                res.set_content(R"({"error":"Invalid currency type"})", "application/json");
                return;
            }
            
            // Get current balance
            auto current = Database::instance().query(
                "SELECT " + currency + " FROM characters WHERE id = " + std::to_string(charId)
            );
            
            if (current.empty()) {
                res.status = 404;
                res.set_content(R"({"error":"Character not found"})", "application/json");
                return;
            }
            
            int currentBalance = std::stoi(current[0][currency]);
            int newBalance = currentBalance + amount;
            
            if (newBalance < 0) {
                res.status = 400;
                res.set_content(R"({"error":"Insufficient balance"})", "application/json");
                return;
            }
            
            // Update balance
            std::string sql = "UPDATE characters SET " + currency + " = " + std::to_string(newBalance) +
                             " WHERE id = " + std::to_string(charId);
            
            if (Database::instance().execute(sql)) {
                // Log the transaction
                Database::instance().execute(
                    "INSERT INTO transaction_logs (character_id, type, amount, currency, reason, admin_id) "
                    "VALUES (" + std::to_string(charId) + ", '" + 
                    (amount >= 0 ? "ADD" : "REMOVE") + "', " +
                    std::to_string(std::abs(amount)) + ", '" + currency + "', '" +
                    AdminAPI::escapeSql(reason) + "', 0)"
                );
                
                LOG_INFO("WEB", "Currency modified: char=" + std::to_string(charId) + 
                         " " + currency + " " + (amount >= 0 ? "+" : "") + std::to_string(amount));
                
                res.set_content(json{
                    {"success", true},
                    {"old_balance", currentBalance},
                    {"new_balance", newBalance}
                }.dump(), "application/json");
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
    // API: GM Management (promote/demote)
    // ============================================================
    m_server->Post("/api/gm/set", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int accountId = body["account_id"];
            int gmLevel = body["gm_level"];  // 0=player, 1=GM, 2=Admin, 3=SuperAdmin
            
            if (gmLevel < 0 || gmLevel > 3) {
                res.status = 400;
                res.set_content("{\"error\":\"Invalid GM level, must be 0-3\"}", "application/json");
                return;
            }
            
            std::string sql = "UPDATE accounts SET gm_level = " + std::to_string(gmLevel) +
                             " WHERE id = " + std::to_string(accountId);
            
            if (Database::instance().execute(sql)) {
                // Log the action
                Database::instance().execute(
                    "INSERT INTO game_logs (character_id, event_type, event_data) "
                    "VALUES (0, 'GM_CHANGE', 'Account " + std::to_string(accountId) + 
                    " set to GM level " + std::to_string(gmLevel) + "')"
                );
                
                LOG_INFO("WEB", "GM level changed: account=" + std::to_string(accountId) + 
                         " level=" + std::to_string(gmLevel));
                
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
    // API: Get GM list
    // ============================================================
    m_server->Get("/api/gm/list", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT id, username, email, gm_level, created_at, last_login "
                         "FROM accounts WHERE gm_level > 0 ORDER BY gm_level DESC, username";
        
        auto results = Database::instance().query(sql);
        
        json gms = json::array();
        for (auto& row : results) {
            std::string levelName;
            int level = std::stoi(row["gm_level"]);
            switch (level) {
                case 1: levelName = "Game Master"; break;
                case 2: levelName = "Admin"; break;
                case 3: levelName = "Super Admin"; break;
                default: levelName = "Unknown"; break;
            }
            
            gms.push_back({
                {"id", std::stoi(row["id"])},
                {"username", row["username"]},
                {"email", row["email"]},
                {"gm_level", level},
                {"gm_level_name", levelName},
                {"last_login", row["last_login"]}
            });
        }
        
        res.set_content(json{{"gms", gms}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Send item to character (with logging)
    // ============================================================
    m_server->Post("/api/items/send", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int templateId = body["template_id"];
            int quantity = body.value("quantity", 1);
            std::string reason = body.value("reason", "Admin gift");
            
            // Check character exists
            auto charCheck = Database::instance().query(
                "SELECT id FROM characters WHERE id = " + std::to_string(charId)
            );
            if (charCheck.empty()) {
                res.status = 404;
                res.set_content(R"({"error":"Character not found"})", "application/json");
                return;
            }
            
            // Insert item
            std::string sql = "INSERT INTO items (character_id, template_id, quantity, source) VALUES (" +
                             std::to_string(charId) + ", " +
                             std::to_string(templateId) + ", " +
                             std::to_string(quantity) + ", 'ADMIN')";
            
            if (Database::instance().execute(sql)) {
                // Log the action
                Database::instance().execute(
                    "INSERT INTO game_logs (character_id, event_type, event_data) "
                    "VALUES (" + std::to_string(charId) + ", 'ITEM_SENT', "
                    "'Template " + std::to_string(templateId) + " x" + std::to_string(quantity) + 
                    " - " + AdminAPI::escapeSql(reason) + "')"
                );
                
                LOG_INFO("WEB", "Item sent: char=" + std::to_string(charId) + 
                         " template=" + std::to_string(templateId) + " x" + std::to_string(quantity));
                
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
    // API: Send vehicle to character
    // ============================================================
    m_server->Post("/api/vehicles/send", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int charId = body["character_id"];
            int templateId = body["template_id"];
            std::string reason = body.value("reason", "Admin gift");
            
            std::string sql = "INSERT INTO vehicles (character_id, vehicle_type_id, durability, max_durability, source) "
                             "VALUES (" + std::to_string(charId) + ", " +
                             std::to_string(templateId) + ", 100, 100, 'ADMIN')";
            
            if (Database::instance().execute(sql)) {
                Database::instance().execute(
                    "INSERT INTO game_logs (character_id, event_type, event_data) "
                    "VALUES (" + std::to_string(charId) + ", 'VEHICLE_SENT', "
                    "'Vehicle " + std::to_string(templateId) + " - " + AdminAPI::escapeSql(reason) + "')"
                );
                
                LOG_INFO("WEB", "Vehicle sent: char=" + std::to_string(charId) + 
                         " template=" + std::to_string(templateId));
                
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
    // API: Gacha system - Get available gachas
    // ============================================================
    m_server->Get("/api/gacha", [](const httplib::Request&, httplib::Response& res) {
        std::string sql = "SELECT id, name, description, cost_gold, cost_cash, is_active, "
                         "start_date, end_date FROM gacha_banners "
                         "WHERE is_active = 1 AND (end_date IS NULL OR end_date > NOW()) "
                         "ORDER BY id";
        
        auto results = Database::instance().query(sql);
        
        json gachas = json::array();
        for (auto& row : results) {
            gachas.push_back({
                {"id", std::stoi(row["id"])},
                {"name", row["name"]},
                {"description", row["description"]},
                {"cost_gold", std::stoi(row["cost_gold"])},
                {"cost_cash", std::stoi(row["cost_cash"])},
                {"is_active", row["is_active"] == "1"},
                {"start_date", row["start_date"]},
                {"end_date", row["end_date"]}
            });
        }
        
        res.set_content(json{{"gachas", gachas}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Gacha items (pool)
    // ============================================================
    m_server->Get(R"(/api/gacha/(\d+)/items)", [](const httplib::Request& req, httplib::Response& res) {
        std::string gachaId = req.matches[1];
        
        std::string sql = "SELECT gi.id, gi.template_id, gi.drop_rate, gi.rarity, "
                         "it.name as item_name, it.category "
                         "FROM gacha_items gi "
                         "LEFT JOIN item_templates it ON gi.template_id = it.id "
                         "WHERE gi.gacha_id = " + gachaId + " "
                         "ORDER BY gi.rarity DESC, gi.drop_rate ASC";
        
        auto results = Database::instance().query(sql);
        
        json items = json::array();
        for (auto& row : results) {
            items.push_back({
                {"id", std::stoi(row["id"])},
                {"template_id", std::stoi(row["template_id"])},
                {"item_name", row["item_name"]},
                {"category", row["category"]},
                {"drop_rate", std::stof(row["drop_rate"])},
                {"rarity", row["rarity"]}
            });
        }
        
        res.set_content(json{{"items", items}}.dump(), "application/json");
    });
    
    // ============================================================
    // API: Create/Update gacha banner
    // ============================================================
    m_server->Post("/api/gacha", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            std::string name = AdminAPI::escapeSql(body["name"]);
            std::string desc = AdminAPI::escapeSql(body.value("description", ""));
            int costGold = body.value("cost_gold", 0);
            int costCash = body.value("cost_cash", 100);
            bool isActive = body.value("is_active", true);
            
            std::string sql = "INSERT INTO gacha_banners (name, description, cost_gold, cost_cash, is_active) "
                             "VALUES ('" + name + "', '" + desc + "', " +
                             std::to_string(costGold) + ", " + std::to_string(costCash) + ", " +
                             std::to_string(isActive ? 1 : 0) + ")";
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Created gacha: " + name);
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
    // API: Add item to gacha pool
    // ============================================================
    m_server->Post("/api/gacha/item", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int gachaId = body["gacha_id"];
            int templateId = body["template_id"];
            float dropRate = body.value("drop_rate", 1.0f);
            std::string rarity = body.value("rarity", "common");
            
            std::string sql = "INSERT INTO gacha_items (gacha_id, template_id, drop_rate, rarity) "
                             "VALUES (" + std::to_string(gachaId) + ", " +
                             std::to_string(templateId) + ", " +
                             std::to_string(dropRate) + ", '" + AdminAPI::escapeSql(rarity) + "')";
            
            if (Database::instance().execute(sql)) {
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
    // API: Shop editor - Create/Update item
    // ============================================================
    m_server->Post("/api/shop/item", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            int templateId = body["template_id"];
            std::string category = AdminAPI::escapeSql(body["category"]);
            std::string name = AdminAPI::escapeSql(body["name"]);
            int priceGold = body.value("price_gold", 0);
            int priceCash = body.value("price_cash", 0);
            int discount = body.value("discount_percent", 0);
            bool available = body.value("is_available", true);
            
            // Check if exists
            auto existing = Database::instance().query(
                "SELECT id FROM shop_items WHERE template_id = " + std::to_string(templateId)
            );
            
            std::string sql;
            if (existing.empty()) {
                sql = "INSERT INTO shop_items (template_id, category, name, price_gold, price_cash, "
                     "discount_percent, is_available) VALUES (" +
                     std::to_string(templateId) + ", '" + category + "', '" + name + "', " +
                     std::to_string(priceGold) + ", " + std::to_string(priceCash) + ", " +
                     std::to_string(discount) + ", " + std::to_string(available ? 1 : 0) + ")";
            } else {
                sql = "UPDATE shop_items SET category = '" + category + "', name = '" + name + "', "
                     "price_gold = " + std::to_string(priceGold) + ", price_cash = " + std::to_string(priceCash) + ", "
                     "discount_percent = " + std::to_string(discount) + ", is_available = " + std::to_string(available ? 1 : 0) +
                     " WHERE template_id = " + std::to_string(templateId);
            }
            
            if (Database::instance().execute(sql)) {
                LOG_INFO("WEB", "Shop item saved: " + name);
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
    // API: Delete shop item
    // ============================================================
    m_server->Delete(R"(/api/shop/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string itemId = req.matches[1];
        
        if (Database::instance().execute("DELETE FROM shop_items WHERE id = " + itemId)) {
            LOG_INFO("WEB", "Deleted shop item: " + itemId);
            res.set_content(R"({"success":true})", "application/json");
        } else {
            res.status = 500;
            res.set_content(R"({"error":"Database error"})", "application/json");
        }
    });
    
    // ============================================================
    // API: Mass currency event (give to all)
    // ============================================================
    m_server->Post("/api/event/currency", [this](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req.get_header_value("Authorization"))) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            std::string currency = body["currency"];
            int amount = body["amount"];
            std::string reason = body.value("reason", "Event reward");
            
            if (currency != "gold" && currency != "cash") {
                res.status = 400;
                res.set_content(R"({"error":"Invalid currency"})", "application/json");
                return;
            }
            
            std::string sql = "UPDATE characters SET " + currency + " = " + currency + " + " + std::to_string(amount);
            
            if (Database::instance().execute(sql)) {
                // Log event
                Database::instance().execute(
                    "INSERT INTO game_logs (character_id, event_type, event_data) "
                    "VALUES (0, 'MASS_CURRENCY', 'All players +" + std::to_string(amount) + " " + currency + 
                    " - " + AdminAPI::escapeSql(reason) + "')"
                );
                
                LOG_INFO("WEB", "Mass currency event: +" + std::to_string(amount) + " " + currency);
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
    
    LOG_DEBUG("WEB", "Routes configured (extended v2)");
}

} // namespace knc
