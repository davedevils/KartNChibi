/**
 * @file WebServer.h
 * @brief HTTP server for admin panel using cpp-httplib
 */

#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

// Forward declare httplib types
namespace httplib {
    class Server;
}

namespace knc {

// Server statistics (shared with game servers)
struct ServerStats {
    int playersOnline = 0;
    int activeRooms = 0;
    int totalAccounts = 0;
    int packetsPerSec = 0;
    int64_t uptime = 0;
    std::string serverName = "KnC Server";
};

class WebServer {
public:
    WebServer();
    ~WebServer();
    
    // Start server on port (blocking if async=false)
    bool start(int port, bool async = true);
    void stop();
    bool isRunning() const { return m_running; }
    
    // Update stats (called by game server)
    void updateStats(const ServerStats& stats);
    ServerStats getStats() const { return m_stats; }
    
    // Set API token for authentication
    void setApiToken(const std::string& token) { m_apiToken = token; }

private:
    void setupRoutes();
    bool checkAuth(const std::string& authHeader);
    
    std::unique_ptr<httplib::Server> m_server;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    ServerStats m_stats;
    std::string m_apiToken = "admin123";  // Default token
    std::string m_staticDir = "static";
};

} // namespace knc
