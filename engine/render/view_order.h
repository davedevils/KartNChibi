#pragma once
#include <bgfx/bgfx.h>

namespace KnC::Render {

// frame views in bgfx order sky swaps camera plane glow reads world
enum ViewOrder : bgfx::ViewId {
    // clear back buffer before anything editor frame shrink viewport
    kFrameClearView = 0,
    kSkyView = 1,
    kSceneView = 2,
    kGlowCaptureView = 3,
    kGlowBrightPassView = 4,
    kGlowFirstBlurView = 5,
    kGlowSecondBlurView = 6,
    kGlowCombineView = 7,
    kOverlayView = 8,
    // Small scenes drawn into a hud rect the view order puts them before the overlay
    kHudSceneFirstView = 9,
};

// How many hud scenes one frame may draw each takes its own view
constexpr int kHudSceneViews = 4;

}
