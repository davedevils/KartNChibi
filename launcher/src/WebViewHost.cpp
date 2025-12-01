/**
 * @file WebViewHost.cpp
 * @brief WebView2 hosting for modern UI
 */

#include "WebViewHost.h"
#include <WebView2.h>
#include <wrl/event.h>
#include <string>

using namespace Microsoft::WRL;

namespace knc {

bool WebViewHost::init(HWND hwnd, const std::wstring& htmlPath) {
    m_hwnd = hwnd;
    
    // Create WebView2 environment
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, htmlPath](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) return result;
                
                // Create controller
                env->CreateCoreWebView2Controller(m_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, htmlPath](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result)) return result;
                            
                            m_controller = controller;
                            m_controller->get_CoreWebView2(&m_webview);
                            
                            // Configure WebView
                            ICoreWebView2Settings* settings;
                            m_webview->get_Settings(&settings);
                            settings->put_AreDefaultContextMenusEnabled(FALSE);
                            settings->put_AreDevToolsEnabled(FALSE);
                            settings->put_IsZoomControlEnabled(FALSE);
                            
                            // Make background transparent
                            ICoreWebView2Controller2* controller2;
                            m_controller->QueryInterface(&controller2);
                            if (controller2) {
                                COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
                                controller2->put_DefaultBackgroundColor(transparent);
                            }
                            
                            // Resize to window
                            RECT bounds;
                            GetClientRect(m_hwnd, &bounds);
                            m_controller->put_Bounds(bounds);
                            
                            // Handle messages from JS
                            m_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR messageRaw;
                                        args->TryGetWebMessageAsString(&messageRaw);
                                        
                                        // Convert to string and call handler
                                        std::wstring wstr(messageRaw);
                                        std::string str(wstr.begin(), wstr.end());
                                        
                                        if (m_messageHandler) {
                                            m_messageHandler(str);
                                        }
                                        
                                        CoTaskMemFree(messageRaw);
                                        return S_OK;
                                    }
                                ).Get(), nullptr
                            );
                            
                            // Navigate to HTML
                            m_webview->Navigate(htmlPath.c_str());
                            
                            return S_OK;
                        }
                    ).Get()
                );
                
                return S_OK;
            }
        ).Get()
    );
    
    // Process messages until WebView is ready
    MSG msg;
    while (!m_webview && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return m_webview != nullptr;
}

void WebViewHost::resize(int width, int height) {
    if (m_controller) {
        RECT bounds = {0, 0, width, height};
        m_controller->put_Bounds(bounds);
    }
}

void WebViewHost::navigate(const std::wstring& url) {
    if (m_webview) {
        m_webview->Navigate(url.c_str());
    }
}

void WebViewHost::executeScript(const std::wstring& script) {
    if (m_webview) {
        m_webview->ExecuteScript(script.c_str(), nullptr);
    }
}

} // namespace knc

