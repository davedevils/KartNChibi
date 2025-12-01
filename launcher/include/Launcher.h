#pragma once
#include <string>

namespace knc {

struct LoginResult {
    bool success = false;
    std::string token;
    std::string message = "Unknown error";
    std::string gameServerIp;
    int gameServerPort = 0;
};

class Launcher {
public:
    static Launcher& instance() {
        static Launcher inst;
        return inst;
    }
    
    bool init();
    LoginResult login(const std::string& username, const std::string& password);
    LoginResult registerAccount(const std::string& username, const std::string& password);
    bool launchGame();
    
    void setServer(const std::string& ip, int port) { m_serverIp = ip; m_serverPort = port; }
    std::string getServerIp() const { return m_serverIp; }
    int getServerPort() const { return m_serverPort; }

private:
    Launcher() = default;
    
    std::string m_serverIp = "127.0.0.1";
    int m_serverPort = 50017;
    std::string m_gamePath = "KnC.exe";
    std::string m_sessionToken;
    std::string m_username;
    
    void loadConfig();
    void saveConfig();
    bool updateNetworkIni();
};

} // namespace knc
