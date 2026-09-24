// a modal box with server text closes on click enter or escape
#pragma once

#include "Screen.h"

#include <functional>
#include <string>

namespace KnC::Client {

class App;

class MessagePopup : public Screen {
public:
    MessagePopup(App& app, std::string text, std::function<void()> onClose);
    // the confirm box of sub 470520 OK and Cancel halves OK runs onOk then closes Cancel only closes
    MessagePopup(App& app, std::string text, std::function<void()> onOk, std::function<void()> onCancel, bool confirm);
    const char* name() const override { return m_confirm ? "confirm" : "message"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void close();
    void confirmOk();
    App& m_app;
    std::string m_text;
    std::function<void()> m_onClose;
    std::function<void()> m_onOk;
    bool m_confirm = false;
    bool m_closed = false;
};

}
