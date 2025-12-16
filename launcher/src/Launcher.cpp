#include "Launcher.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shellapi.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "shell32.lib")
    typedef SOCKET socket_t;
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR_VAL -1
    #define CLOSE_SOCKET close
#endif

namespace knc {

// encryption key for Network2.ini 
static const std::string NETWORK_KEY = "WindySoftKnCOnGame";

static std::string encryptNetwork(const std::string& input) {
    std::string output;
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        int v = static_cast<unsigned char>(input[i]);
        if (v <= 31 || v > 122) {
            output[i] = static_cast<char>(v);
        } else {
            output[i] = static_cast<char>((v - 32 + NETWORK_KEY[i % NETWORK_KEY.length()] - 32) % 91 + 32);
        }
    }
    return output;
}

bool Launcher::init() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif
    loadConfig();
    return true;
}

void Launcher::loadConfig() {
    std::ifstream cfg("launcher.ini");
    if (!cfg.is_open()) {
        saveConfig();
        return;
    }
    
    std::string line;
    while (std::getline(cfg, line)) {
        if (line.find("ServerIP=") == 0) {
            m_serverIp = line.substr(9);
        } else if (line.find("ServerPort=") == 0) {
            m_serverPort = std::stoi(line.substr(11));
        } else if (line.find("GamePath=") == 0) {
            m_gamePath = line.substr(9);
        }
    }
}

void Launcher::saveConfig() {
    std::ofstream cfg("launcher.ini");
    if (!cfg.is_open()) {
        return;
    }
    cfg << "ServerIP=" << m_serverIp << "\n";
    cfg << "ServerPort=" << m_serverPort << "\n";
    cfg << "GamePath=" << m_gamePath << "\n";
}

LoginResult Launcher::login(const std::string& username, const std::string& password) {
    LoginResult result;
    result.message = "Connecting...";
    
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        result.message = "Failed to create socket";
        return result;
    }
    
    // 5sec timeout
#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = {5, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_serverPort);
    inet_pton(AF_INET, m_serverIp.c_str(), &addr.sin_addr);
    
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL) {
        CLOSE_SOCKET(sock);
        result.message = "Cannot connect to server";
        return result;
    }
    
    // packet: [size:2][cmd:1][flag:1][reserved:4][user\0][pass\0]
    std::vector<uint8_t> payload;
    for (char c : username) {
        payload.push_back((uint8_t)c);
    }
    payload.push_back(0);
    for (char c : password) {
        payload.push_back((uint8_t)c);
    }
    payload.push_back(0);
    
    std::vector<uint8_t> packet;
    uint16_t sz = (uint16_t)payload.size();
    packet.push_back(sz & 0xFF);
    packet.push_back((sz >> 8) & 0xFF);
    packet.push_back(0xFE);  // cmd
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.insert(packet.end(), payload.begin(), payload.end());
    
    send(sock, (const char*)packet.data(), (int)packet.size(), 0);
    
    // recv response [size:2][0xFE][flag][reserved:4][success:1][token\0][msg\0]
    uint8_t buffer[512];
    int attempts = 0;
    bool found = false;
    
    while (attempts < 5 && !found) {
        int received = recv(sock, (char*)buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        
        if (received >= 8 && buffer[2] == 0xFE) {
            found = true;
            if (received >= 9) {
                bool success = buffer[8] == 0x01;
                std::string token, msg;
                
                if (received > 9) {
                    const char* tokenStart = (const char*)&buffer[9];
                    size_t tokenLen = strnlen(tokenStart, received - 9);
                    token = std::string(tokenStart, tokenLen);
                    
                    size_t msgOff = 9 + tokenLen + 1;
                    if (msgOff < (size_t)received) {
                        const char* msgStart = (const char*)&buffer[msgOff];
                        msg = std::string(msgStart, strnlen(msgStart, received - msgOff));
                    }
                }
                
                result.success = success;
                result.message = msg.empty() ? (success ? "Login successful!" : "Invalid credentials") : msg;
                
                if (success && !token.empty()) {
                    m_sessionToken = token;
                    m_username = username;
                }
            }
        } else {
            attempts++;
        }
    }
    
    CLOSE_SOCKET(sock);
    
    if (!found) {
        result.message = "No valid response from server";
    }
    
    return result;
}

LoginResult Launcher::registerAccount(const std::string& username, const std::string& password) {
    LoginResult result;
    result.message = "Registration not implemented yet";
    (void)username;
    (void)password;
    return result;
}

bool Launcher::updateNetworkIni() {
    std::ofstream net("Network2.ini.dec");
    if (!net.is_open()) {
        net.open(m_gamePath.substr(0, m_gamePath.find_last_of("/\\") + 1) + "Network2.ini.dec");
        if (!net.is_open()) {
            return false;
        }
    }
    net << m_serverIp << "\n" << m_serverPort << "\n";
    return true;
}

bool Launcher::launchGame() {
#ifdef _WIN32
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    
    std::string absPath;
    if (m_gamePath.find(':') != std::string::npos || m_gamePath[0] == '\\') {
        absPath = m_gamePath;
    } else {
        absPath = std::string(currentDir) + "\\" + m_gamePath;
    }
    
    std::string gameDir;
    size_t slash = absPath.find_last_of("/\\");
    if (slash != std::string::npos) {
        gameDir = absPath.substr(0, slash);
    } else {
        gameDir = currentDir;
    }
    
    // write encrypted Network2.ini
    std::string ipEnc = encryptNetwork(m_serverIp);
    std::string portEnc = encryptNetwork(std::to_string(m_serverPort));
    std::ofstream net(gameDir + "\\Network2.ini", std::ios::binary);
    if (net.is_open()) {
        net << ipEnc << "\r\n" << portEnc << "\r\n";
        net.close();
    }
    
    // client args: serviceid=X userid=NAME token=TOKEN
    std::string params;
    if (!m_sessionToken.empty() && !m_username.empty()) {
        params = "serviceid=1 userid=" + m_username + " token=" + m_sessionToken;
    }
    
    std::wstring wPath(absPath.begin(), absPath.end());
    std::wstring wDir(gameDir.begin(), gameDir.end());
    std::wstring wParams(params.begin(), params.end());
    
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = wPath.c_str();
    sei.lpParameters = params.empty() ? nullptr : wParams.c_str();
    sei.lpDirectory = wDir.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    if (ShellExecuteExW(&sei) && sei.hProcess) {
        CloseHandle(sei.hProcess);
        return true;
    }
    
    return false;
#else
    updateNetworkIni();
    pid_t pid = fork();
    if (pid == 0) {
        execlp(m_gamePath.c_str(), m_gamePath.c_str(), nullptr);
        _exit(1);
    }
    return pid > 0;
#endif
}

} // namespace knc
