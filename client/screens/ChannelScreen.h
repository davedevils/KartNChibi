// channel screen draws 0x000E rows a click stores channel for menu
#pragma once

#include "ui/WidgetScreen.h"

#include <vector>

namespace KnC::Client {

class ChannelScreen : public WidgetScreen {
public:
    explicit ChannelScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "channel"; }
    void enter() override;
    void update(float dt) override;
    void onKey(int key, int action, int mods) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void pick(int index);
    struct RowButton {
        int channelIndex;
        ButtonWidget* button;
    };
    std::vector<RowButton> m_rows;
    int m_selected = -1;
    float m_time = 0.f;
    bool m_left = false;
};

}
