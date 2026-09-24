// The preview of one entry a texture a model on the scene renderer a clip or a text
#pragma once

#include "dat_archive.h"
#include "dat_audio.h"

#include "engine/render/light_rig.h"
#include "engine/render/nif_character_model.h"
#include "engine/render/scene_renderer.h"
#include "engine/render/texture_cache.h"

#include <string>
#include <vector>

namespace KnC::Tools {

// Z up like the client yaw 0 stands on the minus X side looking along plus X
class OrbitCamera {
public:
    void frame_bound(const KnC::Render::ModelBound& bound);
    void turn(float yaw, float pitch);
    // Absolute angles radians a capture switch names them
    void set_angles(float yaw, float pitch);
    void dolly(float factor);
    void view(float out[16], float eye[3]) const;

private:
    float target_[3] = {0.f, 0.f, 0.f};
    float yaw_ = 0.6f;
    float pitch_ = -0.35f;
    float distance_ = 1.f;
    float closest_ = 0.01f;
};

struct ModelInfo {
    std::string name;
    bool   character = false;
    size_t parts = 0;
    size_t particle_systems = 0;
    size_t triangles = 0;
    size_t skeleton_nodes = 0;
    size_t clips = 0;
    float  radius = 0.f;
};

enum class PreviewKind { None, Image, Model, Audio, Text, Unsupported };

class Preview {
public:
    void attach(KnC::Render::SceneRenderer* renderer, Archive* archive, AudioPlayer* audio);
    // Reads World Light nif of the open archive the model preview lights with it
    void load_light_rig();
    void show(int index);
    void clear();
    void shutdown();

    PreviewKind kind() const { return kind_; }
    const std::string& info() const { return info_; }
    const std::string& error() const { return error_; }

    const KnC::Render::CachedTexture& image() const { return image_; }
    float image_zoom = 1.f;
    float image_pan[2] = {0.f, 0.f};

    const ModelInfo& model() const { return model_; }
    OrbitCamera& camera() { return camera_; }
    void reset_camera() { camera_.frame_bound(bound_); }

    // Writable so the read only text box can take it
    char* text_buffer() { return text_.data(); }
    size_t text_size() const { return text_.size() + 1; }
    bool text_truncated() const { return text_truncated_; }

private:
    bool load_image(int index);
    bool load_model(int index);
    bool load_audio(int index);
    void load_text(int index);
    void load_kfm_text(int index);
    std::vector<KnC::Render::CharacterClipRequest> find_clips(const std::string& folder,
                                                              const std::string& stem);
    std::string texture_from_pak(const std::string& folder, const std::string& wanted);

    KnC::Render::SceneRenderer* renderer_ = nullptr;
    Archive*     archive_ = nullptr;
    AudioPlayer* audio_ = nullptr;
    KnC::Render::TextureCache textures_{KnC::Render::TextureAlpha::Straight};
    KnC::Render::LightRig rig_;
    bool rig_ok_ = false;
    PreviewKind kind_ = PreviewKind::None;
    std::string info_;
    std::string error_;
    std::string text_;
    bool text_truncated_ = false;
    KnC::Render::CachedTexture image_;
    ModelInfo model_;
    KnC::Render::ModelBound bound_;
    OrbitCamera camera_;
};

}
