/**
 * @file main.cpp
 * @brief KnC Launcher - Windows native with WebView2
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <wrl.h>
#include <WebView2.h>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <ctime>
#include <cstdint>
#include "Launcher.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Microsoft::WRL;

// Forward declarations
std::wstring GenerateHTML();

// Simple logging
static std::ofstream g_logFile;

void LogInit() {
    g_logFile.open("launcher.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    g_logFile << "\n=== Launcher started " << std::ctime(&time);
}

void Log(const std::string& msg) {
    if (g_logFile.is_open()) {
        g_logFile << "[LOG] " << msg << std::endl;
        g_logFile.flush();
    }
    OutputDebugStringA(("[KnC] " + msg + "\n").c_str());
}

// Base64 encoding for image
std::string Base64Encode(const std::vector<unsigned char>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

std::string LoadImageAsBase64(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Log("Cannot open image: " + path);
        return "";
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Log("Loaded image: " + path + " (" + std::to_string(data.size()) + " bytes)");
    return "data:image/jpeg;base64," + Base64Encode(data);
}

// Extract image from NKZIP pak file to temp and return file:// URL
std::string LoadImageFromPak(const std::string& pakPath, const std::string& innerPath) {
    std::ifstream pakFile(pakPath, std::ios::binary);
    if (!pakFile.is_open()) {
        Log("Cannot open pak file: " + pakPath);
        return "";
    }
    
    // Read NKZIP header
    char magic[16];
    pakFile.read(magic, sizeof(magic));
    if (std::string(magic, 5) != "NKZIP") {
        Log("Invalid NKZIP file: " + pakPath);
        return "";
    }
    
    // Skip version (16 bytes) + dataBytes (4 bytes)
    pakFile.seekg(16 + 4, std::ios::cur);
    
    // Read file count
    uint32_t fileCount;
    pakFile.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    Log("PAK contains " + std::to_string(fileCount) + " files");
    
    // Normalize inner path for comparison (convert backslashes to forward slashes)
    std::string normalizedInner = innerPath;
    for (char& c : normalizedInner) {
        if (c == '\\') c = '/';
    }
    
    // Search for the file
    for (uint32_t i = 0; i < fileCount; i++) {
        uint32_t fileSize;
        char fileName[260];
        
        pakFile.read(reinterpret_cast<char*>(&fileSize), sizeof(fileSize));
        pakFile.read(fileName, sizeof(fileName));
        
        // Normalize file name for comparison
        std::string normalizedName(fileName);
        for (char& c : normalizedName) {
            if (c == '\\') c = '/';
        }
        
        if (normalizedName == normalizedInner) {
            // Found the file, read its data
            std::vector<unsigned char> data(fileSize);
            pakFile.read(reinterpret_cast<char*>(data.data()), fileSize);
            Log("Loaded from PAK: " + innerPath + " (" + std::to_string(fileSize) + " bytes)");
            
            // Save to temp file
            char tempPath[MAX_PATH];
            GetTempPathA(MAX_PATH, tempPath);
            std::string tempFile = std::string(tempPath) + "knc_login_bg.png";
            
            std::ofstream outFile(tempFile, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(reinterpret_cast<char*>(data.data()), data.size());
                outFile.close();
                Log("Saved to temp: " + tempFile);
                
                // Return virtual host URL (mapped via SetVirtualHostNameToFolderMapping)
                return "https://knc.local/knc_login_bg.png";
            }
            return "";
        } else {
            // Skip this file's data
            pakFile.seekg(fileSize, std::ios::cur);
        }
    }
    
    Log("File not found in PAK: " + innerPath);
    return "";
}

// Window dimensions
constexpr int WIDTH = 800;
constexpr int HEIGHT = 620;

// Globals
HWND g_hwnd = nullptr;
ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (g_controller) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                g_controller->put_Bounds(bounds);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_APP + 1: {
            // Execute JS on UI thread
            std::wstring* js = (std::wstring*)lParam;
            if (js && g_webview) {
                Log("Executing JS on UI thread");
                g_webview->ExecuteScript(js->c_str(), nullptr);
                delete js;
            }
            break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void InitWebView(HWND hwnd) {
    // Use a hidden folder for WebView2 data instead of creating visible folder next to exe
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring userDataFolder = std::wstring(tempPath) + L"KnCLauncher_WebView2";
    
    // Create the folder and set it as hidden
    CreateDirectoryW(userDataFolder.c_str(), nullptr);
    SetFileAttributesW(userDataFolder.c_str(), FILE_ATTRIBUTE_HIDDEN);
    
    CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) return result;
                
                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result)) return result;
                            
                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);
                            
                            // Settings
                            ComPtr<ICoreWebView2Settings> settings;
                            g_webview->get_Settings(&settings);
                            settings->put_AreDefaultContextMenusEnabled(FALSE);
                            settings->put_AreDevToolsEnabled(TRUE);
                            
                            // Allow file access for background image from temp
                            ComPtr<ICoreWebView2_3> webview3;
                            if (SUCCEEDED(g_webview->QueryInterface(IID_PPV_ARGS(&webview3)))) {
                                wchar_t tempPath[MAX_PATH];
                                GetTempPathW(MAX_PATH, tempPath);
                                webview3->SetVirtualHostNameToFolderMapping(
                                    L"knc.local", tempPath,
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                Log("Mapped knc.local to temp folder");
                            }
                            
                            // Resize
                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            g_controller->put_Bounds(bounds);
                            
                            // Handle messages from JS
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR msgRaw;
                                        HRESULT hr = args->TryGetWebMessageAsString(&msgRaw);
                                        if (FAILED(hr) || !msgRaw) {
                                            Log("Failed to get message from WebView");
                                            return S_OK;
                                        }
                                        std::wstring msg(msgRaw);
                                        CoTaskMemFree(msgRaw);
                                        
                                        // Log all received messages
                                        std::string msgStr;
                                        for (wchar_t c : msg) msgStr += (char)c;
                                        Log("Received message: " + msgStr);
                                        
                                        if (msg == L"GET_SAVED_USER") {
                                            // Load saved username from ini
                                            std::ifstream cfg("launcher.ini");
                                            std::string savedUser;
                                            if (cfg.is_open()) {
                                                std::string line;
                                                while (std::getline(cfg, line)) {
                                                    if (line.find("SavedUser=") == 0) {
                                                        savedUser = line.substr(10);
                                                        break;
                                                    }
                                                }
                                            }
                                            if (!savedUser.empty()) {
                                                std::wstring js = L"setSavedUser('" + std::wstring(savedUser.begin(), savedUser.end()) + L"')";
                                                g_webview->ExecuteScript(js.c_str(), nullptr);
                                            }
                                        }
                                        else if (msg == L"QUIT") {
                                            PostQuitMessage(0);
                                        }
                                        else if (msg == L"MINIMIZE") {
                                            ShowWindow(g_hwnd, SW_MINIMIZE);
                                        }
                                        else if (msg == L"REGISTER") {
                                            // Open register URL from ini in default browser
                                            std::ifstream cfg("launcher.ini");
                                            std::string registerUrl;
                                            if (cfg.is_open()) {
                                                std::string line;
                                                while (std::getline(cfg, line)) {
                                                    if (line.find("RegisterURL=") == 0) {
                                                        registerUrl = line.substr(12);
                                                        break;
                                                    }
                                                }
                                            }
                                            if (!registerUrl.empty()) {
                                                Log("Opening register URL: " + registerUrl);
                                                ShellExecuteA(nullptr, "open", registerUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                            } else {
                                                Log("No RegisterURL in launcher.ini");
                                                g_webview->ExecuteScript(L"setStatus('&#10008; Register URL not configured','error')", nullptr);
                                            }
                                        }
                                        else if (msg == L"FIND_ACCOUNT") {
                                            // Open find account URL from ini in default browser
                                            std::ifstream cfg("launcher.ini");
                                            std::string findUrl;
                                            if (cfg.is_open()) {
                                                std::string line;
                                                while (std::getline(cfg, line)) {
                                                    if (line.find("FindAccountURL=") == 0) {
                                                        findUrl = line.substr(15);
                                                        break;
                                                    }
                                                }
                                            }
                                            if (!findUrl.empty()) {
                                                Log("Opening find account URL: " + findUrl);
                                                ShellExecuteA(nullptr, "open", findUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                            } else {
                                                Log("No FindAccountURL in launcher.ini");
                                                g_webview->ExecuteScript(L"setStatus('&#10008; Find Account URL not configured','error')", nullptr);
                                            }
                                        }
                                        else if (msg == L"DRAG_START") {
                                            // Start drag - nothing to do here
                                        }
                                        else if (msg.find(L"DRAG:") == 0) {
                                            // Parse DRAG:dx:dy
                                            std::wstring data = msg.substr(5);
                                            size_t sep = data.find(L':');
                                            int dx = std::stoi(data.substr(0, sep));
                                            int dy = std::stoi(data.substr(sep + 1));
                                            
                                            RECT rect;
                                            GetWindowRect(g_hwnd, &rect);
                                            SetWindowPos(g_hwnd, nullptr, 
                                                rect.left + dx, rect.top + dy, 0, 0,
                                                SWP_NOSIZE | SWP_NOZORDER);
                                        }
                                        else if (msg == L"LAUNCH") {
                                            Log("Launching game...");
                                            g_webview->ExecuteScript(L"showLaunching()", nullptr);
                                            
                                            // Small delay to show the message, then launch
                                            std::thread([]() {
                                                Sleep(500);
                                                if (knc::Launcher::instance().launchGame()) {
                                                    Log("Game launched successfully, closing launcher");
                                                    // Post close message to UI thread
                                                    PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
                                                } else {
                                                    Log("Failed to launch game!");
                                                    PostMessageW(g_hwnd, WM_APP + 1, 0, (LPARAM)new std::wstring(L"showLaunchError('Could not start KnC.exe')"));
                                                }
                                            }).detach();
                                        }
                                        else if (msg.find(L"LOGIN:") == 0) {
                                            // Parse LOGIN:user:pass:remember
                                            std::wstring data = msg.substr(6);
                                            size_t sep1 = data.find(L':');
                                            size_t sep2 = data.find(L':', sep1 + 1);
                                            std::wstring wuser = data.substr(0, sep1);
                                            std::wstring wpass = data.substr(sep1 + 1, sep2 - sep1 - 1);
                                            bool remember = (data.length() > sep2 + 1 && data[sep2 + 1] == L'1');
                                            
                                            // Convert wstring to string (ASCII only)
                                            std::string user, pass;
                                            for (wchar_t c : wuser) user += (char)c;
                                            for (wchar_t c : wpass) pass += (char)c;
                                            
                                            Log("Login attempt for user: " + user);
                                            
                                            // Save username if remember is checked
                                            if (remember) {
                                                std::ofstream cfg("launcher.ini");
                                                cfg << "ServerIP=" << knc::Launcher::instance().getServerIp() << "\n";
                                                cfg << "ServerPort=" << knc::Launcher::instance().getServerPort() << "\n";
                                                cfg << "GamePath=KnC.exe\n";
                                                cfg << "SavedUser=" << user << "\n";
                                            }
                                            
                                            // Login in background
                                            std::thread([user, pass]() {
                                                try {
                                                    Log("Starting login thread...");
                                                    auto result = knc::Launcher::instance().login(user, pass);
                                                    Log("Login result: " + std::string(result.success ? "SUCCESS" : "FAILED") + " - " + result.message);
                                                    
                                                    // Escape single quotes in message
                                                    std::string safeMsg;
                                                    for (char c : result.message) {
                                                        if (c == '\'') safeMsg += "\\'";
                                                        else safeMsg += c;
                                                    }
                                                    
                                                    // Build JS to execute
                                                    std::wstring js;
                                                    if (result.message.find("Cannot connect") != std::string::npos ||
                                                        result.message.find("No response") != std::string::npos ||
                                                        result.message.find("Failed to create") != std::string::npos) {
                                                        Log("Server appears to be offline");
                                                        js = L"showOffline()";
                                                    } else {
                                                        js = L"showResult(" + 
                                                            std::wstring(result.success ? L"true" : L"false") + 
                                                            L",'" + std::wstring(safeMsg.begin(), safeMsg.end()) + L"')";
                                                    }
                                                    
                                                    // Post to UI thread via Windows message
                                                    Log("Posting JS to UI thread: " + std::string(js.begin(), js.end()));
                                                    PostMessageW(g_hwnd, WM_APP + 1, 0, (LPARAM)new std::wstring(js));
                                                    
                                                } catch (const std::exception& e) {
                                                    Log("Exception in login thread: " + std::string(e.what()));
                                                    PostMessageW(g_hwnd, WM_APP + 1, 0, (LPARAM)new std::wstring(L"showOffline()"));
                                                } catch (...) {
                                                    Log("Unknown exception in login thread");
                                                    PostMessageW(g_hwnd, WM_APP + 1, 0, (LPARAM)new std::wstring(L"showOffline()"));
                                                }
                                            }).detach();
                                        }
                                        
                                        return S_OK;
                                    }
                                ).Get(), nullptr
                            );
                            
                                            // Navigate to HTML
                                            Log("WebView2 ready, loading HTML...");
                                            std::wstring html = GenerateHTML();
                                            g_webview->NavigateToString(html.c_str());
                                            Log("HTML loaded");
                            
                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );
}

// Generate HTML with embedded background image
std::wstring GenerateHTML() {
    // Try to load background from pak001.dat first (path starts with ./)
    std::string bgImage = LoadImageFromPak("pak001.dat", "./Data/Eng/Image/Login/Login01.png");
    if (bgImage.empty()) {
        // Fallback: try external bg.jpg
        bgImage = LoadImageAsBase64("bg.jpg");
    }
    if (bgImage.empty()) {
        // Fallback: try other locations
        bgImage = LoadImageAsBase64("resources/bg.jpg");
    }
    if (bgImage.empty()) {
        // Use gradient fallback
        bgImage = "linear-gradient(135deg,#c66b3d,#8b4513)";
        Log("Using fallback gradient background");
    }
    
    // Use 100% cover for dynamic scaling (1024x768 image)
    std::wstring bgStyle;
    if (bgImage.find("https://") == 0 || bgImage.find("file://") == 0 || bgImage.find("data:") == 0) {
        bgStyle = L"url('" + std::wstring(bgImage.begin(), bgImage.end()) + L"') center/cover no-repeat";
    } else {
        bgStyle = std::wstring(bgImage.begin(), bgImage.end());
    }
    
    std::wstring html = LR"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
@import url('https://fonts.googleapis.com/css2?family=Fredoka+One&family=Poppins:wght@400;600&display=swap');
*{margin:0;padding:0;box-sizing:border-box;user-select:none}
body{font-family:'Poppins',sans-serif;height:100vh;overflow:hidden;display:flex;flex-direction:column}
.titlebar{height:32px;background:linear-gradient(90deg,#2d1b4e,#1a1a2e);display:flex;align-items:center;justify-content:space-between;padding:0 8px;cursor:move}
.titlebar-title{color:#fff;font-size:13px;font-weight:500;opacity:0.9}
.titlebar-btns{display:flex;gap:4px}
.titlebar-btn{width:28px;height:24px;border:none;background:transparent;color:#fff;font-size:14px;cursor:pointer;border-radius:4px;display:flex;align-items:center;justify-content:center}
.titlebar-btn:hover{background:rgba(255,255,255,0.1)}
.titlebar-btn.close:hover{background:#e81123}
.main{flex:1;background:)HTML";
    
    html += bgStyle;
    
    html += LR"HTML(;position:relative;display:flex;align-items:center;justify-content:center}
.main::before{content:'';position:absolute;inset:0;background:rgba(0,0,0,0.15)}
.box{position:relative;background:linear-gradient(145deg,rgba(255,200,50,.85),rgba(255,170,30,.85));border-radius:16px;padding:20px 28px;box-shadow:0 15px 40px rgba(0,0,0,.4),0 0 0 2px rgba(139,90,43,.3),inset 0 1px 0 rgba(255,255,255,.3);min-width:320px;animation:slideIn .4s ease-out}
@keyframes slideIn{from{transform:translateY(-20px) scale(.95);opacity:0}to{transform:translateY(0) scale(1);opacity:1}}
.logo{text-align:center;margin-bottom:12px}
.logo h1{font-family:'Fredoka One',cursive;font-size:32px;color:#e94560;text-shadow:2px 2px 0 #fff,-1px -1px 0 #8b4513,1px -1px 0 #8b4513;letter-spacing:1px}
.logo h1 span{color:#00d4ff;font-size:24px}
.title{display:flex;align-items:center;gap:8px;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid rgba(139,90,43,.25)}
.title h2{font-family:'Fredoka One',cursive;color:#8b4513;font-size:18px;text-shadow:1px 1px 0 rgba(255,255,255,.5)}
.icon{width:22px;height:22px;display:inline-flex;align-items:center;justify-content:center;background:#8b4513;color:#fff;border-radius:5px;font-size:13px}
.field{margin-bottom:10px}
.field label{display:flex;align-items:center;gap:5px;color:#5d3a1a;font-weight:600;margin-bottom:4px;font-size:12px}
.field input{width:100%;padding:8px 12px;border:2px solid #8b4513;border-radius:8px;font-size:13px;background:#fff;transition:all .2s}
.field input:focus{outline:none;border-color:#e94560;box-shadow:0 0 8px rgba(233,69,96,.25)}
.remember{display:flex;align-items:center;gap:6px;margin:10px 0;color:#5d3a1a;font-size:12px}
.remember input{width:16px;height:16px;accent-color:#e94560}
.buttons{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
.btn{padding:8px 14px;border:none;border-radius:8px;font-family:'Fredoka One',cursive;font-size:12px;cursor:pointer;transition:all .15s;text-transform:uppercase;letter-spacing:0.5px}
.btn-primary{background:linear-gradient(145deg,#e94560,#c73e54);color:#fff;box-shadow:0 3px 0 #8b2a3a,0 4px 10px rgba(0,0,0,.3)}
.btn-primary:hover{transform:translateY(-1px);box-shadow:0 4px 0 #8b2a3a,0 6px 12px rgba(0,0,0,.35)}
.btn-primary:active{transform:translateY(1px);box-shadow:0 1px 0 #8b2a3a}
.btn-primary:disabled{opacity:0.6;cursor:not-allowed;transform:none}
.btn-secondary{background:linear-gradient(145deg,#6c757d,#5a6268);color:#fff;box-shadow:0 3px 0 #3d4246}
.btn-secondary:hover{transform:translateY(-1px)}
.btn-secondary:active{transform:translateY(1px)}
.btn-info{background:linear-gradient(145deg,#17a2b8,#138496);color:#fff;box-shadow:0 3px 0 #117a8b}
.btn-success{background:linear-gradient(145deg,#28a745,#218838);color:#fff;box-shadow:0 3px 0 #1e7e34}
.btn-success:hover{transform:translateY(-1px)}
.btn-success:active{transform:translateY(1px)}
.status{text-align:center;padding:10px;margin-top:15px;border-radius:8px;font-weight:500;font-size:14px;display:none}
.status.show{display:block;animation:fadeIn .3s}
.status.error{background:rgba(220,53,69,.2);color:#721c24;border:2px solid rgba(220,53,69,.4)}
.status.success{background:rgba(40,167,69,.2);color:#155724;border:2px solid rgba(40,167,69,.4)}
.status.info{background:rgba(23,162,184,.2);color:#0c5460;border:2px solid rgba(23,162,184,.4)}
@keyframes fadeIn{from{opacity:0}to{opacity:1}}
.spinner{display:inline-block;width:20px;height:20px;border:3px solid rgba(139,69,19,.3);border-top-color:#e94560;border-radius:50%;animation:spin .6s linear infinite;vertical-align:middle;margin-right:8px}
@keyframes spin{to{transform:rotate(360deg)}}
.version{position:absolute;bottom:10px;right:15px;color:rgba(255,255,255,.5);font-size:12px;text-shadow:1px 1px 2px rgba(0,0,0,.5)}
</style>
</head>
<body>
<div class="titlebar" id="titlebar">
<div class="titlebar-title">Kart n' Crazy Launcher</div>
<div class="titlebar-btns">
<button class="titlebar-btn" onclick="window.chrome.webview.postMessage('MINIMIZE')">&#8211;</button>
<button class="titlebar-btn close" onclick="window.chrome.webview.postMessage('QUIT')">&#10005;</button>
</div>
</div>
<div class="main">
<div class="box">
<div class="logo"><h1>KART <span>n'</span> CRAZY</h1></div>
<div class="title"><span class="icon">&#128274;</span><h2>LOGIN</h2></div>
<form id="form" onsubmit="return doLogin()">
<div class="field"><label><span class="icon" style="width:22px;height:22px;font-size:13px">&#128100;</span> ID</label><input type="text" id="user" placeholder="Enter your ID" required></div>
<div class="field"><label><span class="icon" style="width:22px;height:22px;font-size:13px">&#128273;</span> Password</label><input type="password" id="pass" placeholder="Enter password" required></div>
<div class="remember"><input type="checkbox" id="rem"><label for="rem">Remember ID</label><span style="margin-left:auto;font-size:20px">&#127937;</span></div>
<div class="buttons">
<button type="button" class="btn btn-success" onclick="window.chrome.webview.postMessage('REGISTER')">Register</button>
<button type="submit" class="btn btn-primary" id="playBtn">&#9654; PLAY</button>
<button type="button" class="btn btn-secondary" onclick="window.chrome.webview.postMessage('QUIT')">Cancel</button>
<button type="button" class="btn btn-info" onclick="window.chrome.webview.postMessage('FIND_ACCOUNT')">Find ID/PW</button>
</div>
</form>
<div class="status" id="status"></div>
</div>
<div class="version">v1.0.0</div>
</div>
<script>
let isDragging=false,startX,startY;
const tb=document.getElementById('titlebar');
tb.addEventListener('mousedown',e=>{if(e.target.closest('.titlebar-btns'))return;isDragging=true;startX=e.screenX;startY=e.screenY});
document.addEventListener('mousemove',e=>{if(isDragging){window.chrome.webview.postMessage('DRAG:'+(e.screenX-startX)+':'+(e.screenY-startY));startX=e.screenX;startY=e.screenY}});
document.addEventListener('mouseup',()=>{isDragging=false});

// Request saved username from C++
window.chrome.webview.postMessage('GET_SAVED_USER');

function setSavedUser(user){
if(user){document.getElementById('user').value=user;document.getElementById('rem').checked=true}
}

function setStatus(msg,type){
const s=document.getElementById('status');
s.innerHTML=msg;
s.className='status show '+type;
}

function doLogin(){
try{
const user=document.getElementById('user').value;
const pass=document.getElementById('pass').value;
const rem=document.getElementById('rem').checked;
document.getElementById('playBtn').disabled=true;
setStatus('<span class="spinner"></span>Connecting to server...','info');
window.chrome.webview.postMessage('LOGIN:'+user+':'+pass+':'+(rem?'1':'0'));
}catch(e){
setStatus('&#10008; Error: '+e.message,'error');
document.getElementById('playBtn').disabled=false;
}
return false;
}

function showResult(ok,msg){
document.getElementById('playBtn').disabled=false;
if(ok){
setStatus('&#10004; '+msg+' - Launching game...','success');
setTimeout(()=>window.chrome.webview.postMessage('LAUNCH'),1000);
}else{
setStatus('&#10008; '+msg,'error');
}
}

function showOffline(){
document.getElementById('playBtn').disabled=false;
setStatus('&#10008; Server offline - Cannot connect. Check if server is running.','error');
}

function showLaunching(){
setStatus('<span class="spinner"></span>Launching game...','info');
}

function showLaunchError(msg){
document.getElementById('playBtn').disabled=false;
setStatus('&#10008; Launch failed: '+msg,'error');
}
</script>
</body>
</html>
)HTML";
    
    return html;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    LogInit();
    Log("Starting launcher...");
    
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    
    knc::Launcher::instance().init();
    Log("Launcher initialized");
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"KnCLauncher";
    // Load application icon
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
    if (!wc.hIcon) {
        // Fallback: try loading from file
        wc.hIcon = (HICON)LoadImageW(nullptr, L"KnC.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    RegisterClassExW(&wc);
    
    // Center on screen
    int x = (GetSystemMetrics(SM_CXSCREEN) - WIDTH) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - HEIGHT) / 2;
    
    // Create borderless window (WS_POPUP for no title bar)
    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED,
        L"KnCLauncher", L"Kart n' Crazy",
        WS_POPUP,
        x, y, WIDTH, HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );
    
    // Make window slightly transparent at edges for modern look
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    // Initialize WebView2
    InitWebView(g_hwnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    CoUninitialize();
    return (int)msg.wParam;
}

#else
// Linux/macOS placeholder
#include <iostream>
int main() {
    std::cout << "Linux/macOS launcher not yet implemented. Use Windows version.\n";
    return 1;
}
#endif
