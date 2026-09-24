// the logo plates then the intro nif of the stock title stage any key leaves to the login
#pragma once

#include "ui/WidgetScreen.h"

namespace KnC::Client {

class LogoScreen : public WidgetScreen {
public:
    explicit LogoScreen(App& app) : WidgetScreen(app) {}
    // the stock splits this in two stages the plates are stage 1 and the intro is stage 2
    const char* name() const override { return m_stage == Stage::Intro ? "intro" : "logo"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    // Ogp and Rnr are the two company plates of sub 411C00 Intro is the Title INTRO nif of sub 4279B0
    enum class Stage { Ogp, Rnr, Intro };

    // sub 4279B0 loads Title INTRO on an empty scene and starts its controllers
    bool loadIntro();
    void goToIntro();
    void leaveToLogin();
    // the plate alpha of the stock fade in hold fade out pair
    float plateAlpha() const;

    Stage m_stage = Stage::Ogp;
    float m_time = 0.f;
    // the intro clock the nif animation and the stage end read it
    float m_intro = 0.f;
    float m_frameDt = 0.f;
    bool m_left = false;
    bool m_sceneReady = false;
    bool m_sceneTried = false;
    size_t m_model = 0;
};

}
