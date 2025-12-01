#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include <string>

// Forward declarations
struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;

namespace knc {

class WebViewHost {
public:
    bool init(HWND hwnd, const std::wstring& htmlPath);
    void resize(int width, int height);
    void navigate(const std::wstring& url);
    void executeScript(const std::wstring& script);
    
    // Message handler for JS -> C++
    void setMessageHandler(std::function<void(const std::string&)> handler) {
        m_messageHandler = handler;
    }

private:
    HWND m_hwnd = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;
    std::function<void(const std::string&)> m_messageHandler;
};

} // namespace knc

