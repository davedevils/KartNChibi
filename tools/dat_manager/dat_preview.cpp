#include "dat_preview.h"

#include "engine/formats/kfm_reader.h"
#include "engine/formats/nif_reader.h"
#include "engine/render/nif_prop_model.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
using namespace KnC::Render;

namespace KnC::Tools {

namespace {

constexpr size_t kTextPreviewBytes = 2048;

// Sphere on the rest pose box the camera frames it like a prop bound
ModelBound character_bound(const CharacterModel& model) {
    ModelBound bound;
    bound.radius = 0.f;
    for (int axis = 0; axis < 3; ++axis) {
        bound.center[axis] = 0.5f * (model.bounds_min[axis] + model.bounds_max[axis]);
        const float reach = 0.5f * (model.bounds_max[axis] - model.bounds_min[axis]);
        bound.radius += reach * reach;
    }
    bound.radius = std::sqrt(bound.radius);
    return bound;
}

std::string count_text(const char* label, size_t count) {
    return std::string(label) + " " + std::to_string(count);
}

// A model with emitters and no mesh frames the emitter rest positions with room around
ModelBound emitter_bound(const PropModel& model) {
    ModelBound bound;
    bound.center[0] = bound.center[1] = bound.center[2] = 0.f;
    for (const ParticleSystemDefinition& system : model.particle_systems)
        for (int axis = 0; axis < 3; ++axis) bound.center[axis] += system.rest[12 + axis];
    const float count = static_cast<float>(model.particle_systems.size());
    for (int axis = 0; axis < 3; ++axis) bound.center[axis] /= count;
    float spread = 0.f;
    for (const ParticleSystemDefinition& system : model.particle_systems) {
        float squared = 0.f;
        for (int axis = 0; axis < 3; ++axis) {
            const float delta = system.rest[12 + axis] - bound.center[axis];
            squared += delta * delta;
        }
        spread = std::max(spread, std::sqrt(squared));
    }
    bound.radius = spread + 2.f;
    return bound;
}

// One code point as UTF 8 surrogate halves are dropped
void append_utf8(std::string& out, uint32_t point) {
    if (point >= 0xD800 && point <= 0xDFFF) return;
    if (point < 0x80) {
        out.push_back(static_cast<char>(point));
    } else if (point < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (point >> 6)));
        out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
    }
}

}

void OrbitCamera::frame_bound(const ModelBound& bound) {
    for (int axis = 0; axis < 3; ++axis) target_[axis] = bound.center[axis];
    const float radius = bound.radius > 0.f ? bound.radius : 1.f;
    distance_ = radius * 2.6f;
    closest_ = radius * 0.05f;
    yaw_ = 0.6f;
    pitch_ = -0.35f;
}

void OrbitCamera::turn(float yaw, float pitch) {
    yaw_ += yaw;
    pitch_ = std::clamp(pitch_ + pitch, -1.53f, 1.53f);
}

void OrbitCamera::set_angles(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -1.53f, 1.53f);
}

void OrbitCamera::dolly(float factor) { distance_ = std::max(closest_, distance_ * factor); }

void OrbitCamera::view(float out[16], float eye[3]) const {
    const float flat = std::cos(pitch_);
    eye[0] = target_[0] - distance_ * flat * std::cos(yaw_);
    eye[1] = target_[1] - distance_ * flat * std::sin(yaw_);
    eye[2] = target_[2] - distance_ * std::sin(pitch_);
    bx::mtxLookAt(out, bx::Vec3(eye[0], eye[1], eye[2]), bx::Vec3(target_[0], target_[1], target_[2]),
                  bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
}

void Preview::attach(SceneRenderer* renderer, Archive* archive, AudioPlayer* audio) {
    renderer_ = renderer;
    archive_ = archive;
    audio_ = audio;
    // An empty scene sets the backdrop colour before any model is shown
    renderer_->upload(MapScene());
}

void Preview::load_light_rig() {
    rig_ok_ = false;
    const int index = archive_->find_path("Data/Public/World/Light.nif");
    if (index < 0) {
        std::printf("[preview] no World Light nif in the pak ambient only\n");
        return;
    }
    std::string error;
    const std::string path = archive_->cache_path(index);
    if (path.empty() || !KnC::Render::load_light_rig(path, rig_, error)) {
        std::printf("[preview] light rig %s ambient only\n", error.c_str());
        return;
    }
    rig_ok_ = true;
}

void Preview::clear() {
    if (kind_ == PreviewKind::Model && renderer_ != nullptr) {
        renderer_->upload(MapScene());
        renderer_->set_character_clip(-1);
    }
    textures_.destroy_all();
    image_ = CachedTexture();
    if (audio_ != nullptr) audio_->unload();
    kind_ = PreviewKind::None;
    info_.clear();
    error_.clear();
    text_.clear();
    text_truncated_ = false;
    model_ = ModelInfo();
    image_zoom = 1.f;
    image_pan[0] = image_pan[1] = 0.f;
}

void Preview::shutdown() {
    clear();
    textures_.destroy_all();
}

void Preview::show(int index) {
    clear();
    if (archive_ == nullptr || index < 0 || index >= static_cast<int>(archive_->entries().size())) return;
    const KnC::PakEntry& entry = archive_->entries()[index];
    const FileKind kind = file_kind(entry.filename);
    std::printf("[preview] %s\n", entry.path.c_str());
    if (kind_is_image(kind)) {
        kind_ = load_image(index) ? PreviewKind::Image : PreviewKind::Unsupported;
    } else if (kind == FileKind::Nif) {
        kind_ = load_model(index) ? PreviewKind::Model : PreviewKind::Unsupported;
    } else if (kind_is_audio(kind)) {
        kind_ = load_audio(index) ? PreviewKind::Audio : PreviewKind::Unsupported;
    } else if (kind_is_text(kind)) {
        load_text(index);
        kind_ = PreviewKind::Text;
    } else if (kind == FileKind::Kfm) {
        load_kfm_text(index);
        kind_ = PreviewKind::Text;
    } else if (kind == FileKind::Kf) {
        info_ = "KF clip open the body nif of its folder to play it";
        kind_ = PreviewKind::Unsupported;
    } else {
        info_ = "No preview for this kind";
        kind_ = PreviewKind::Unsupported;
    }
}

bool Preview::load_image(int index) {
    const std::string path = archive_->cache_path(index);
    if (path.empty()) {
        error_ = "cannot extract the image";
        return false;
    }
    image_ = textures_.acquire(path);
    if (!bgfx::isValid(image_.handle)) {
        error_ = "cannot decode the image";
        return false;
    }
    info_ = "Dimensions " + std::to_string(image_.width) + "x" + std::to_string(image_.height) +
            (image_.translucent ? " with alpha" : "");
    return true;
}

std::string Preview::texture_from_pak(const std::string& folder, const std::string& wanted) {
    if (wanted.empty()) return wanted;
    const std::string name = fs::path(wanted).filename().string();
    const int index = archive_->find_named_near(folder, name);
    if (index < 0) return wanted;
    const std::string path = archive_->cache_path(index);
    return path.empty() ? wanted : path;
}

std::vector<CharacterClipRequest> Preview::find_clips(const std::string& folder, const std::string& stem) {
    std::vector<CharacterClipRequest> clips;
    const int kfm_index = archive_->find_path(folder + "/" + stem + ".kfm");
    if (kfm_index >= 0) {
        KnC::KfmFile kfm;
        std::string error;
        const std::string kfm_path = archive_->cache_path(kfm_index);
        if (!kfm_path.empty() && KnC::read_kfm(kfm_path, kfm, error)) {
            for (const KnC::KfmSequence& sequence : kfm.sequences) {
                const std::string relative = Archive::clean_path(KnC::kfm_relative_path(sequence.kf_path));
                int kf = archive_->find_path(folder + "/" + relative);
                if (kf < 0) kf = archive_->find_named_near(folder, Archive::name_of(relative));
                if (kf < 0) continue;
                CharacterClipRequest clip;
                clip.kf_path = archive_->cache_path(kf);
                clip.name = sequence.name;
                clip.sequence_id = static_cast<int32_t>(sequence.sequence_id);
                clip.sequence_index = sequence.animation_index;
                if (!clip.kf_path.empty()) clips.push_back(clip);
            }
        } else {
            std::printf("[preview] kfm %s\n", error.c_str());
        }
        if (!clips.empty()) return clips;
    }
    // No KFM every KF of the folder named after the body binds
    const std::string prefix = lower_text(stem) + "_";
    for (int index : archive_->entries_in_folder(folder)) {
        const KnC::PakEntry& entry = archive_->entries()[index];
        if (file_kind(entry.filename) != FileKind::Kf) continue;
        if (lower_text(entry.filename).rfind(prefix, 0) != 0) continue;
        CharacterClipRequest clip;
        clip.kf_path = archive_->cache_path(index);
        if (!clip.kf_path.empty()) clips.push_back(clip);
    }
    return clips;
}

bool Preview::load_model(int index) {
    const std::string nif_path = archive_->cache_path(index);
    if (nif_path.empty()) {
        error_ = "cannot extract the nif";
        return false;
    }
    KnC::NifScene nif;
    std::string error;
    if (!KnC::read_nif_scene(nif_path, nif, error)) {
        error_ = error;
        return false;
    }
    const std::string clean = Archive::clean_path(archive_->entries()[index].path);
    const std::string folder = Archive::folder_of(clean);
    const std::string stem = Archive::stem_of(Archive::name_of(clean));
    const std::string texture_dir = fs::path(nif_path).parent_path().string();

    MapScene scene;
    int first_clip = -1;
    if (nif_has_skin(nif)) {
        CharacterModelRequest request;
        request.nif_path = nif_path;
        request.texture_dir = texture_dir;
        request.clips = find_clips(folder, stem);
        CharacterModel character;
        if (!build_character_model(nif, request, character, error)) {
            error_ = error;
            return false;
        }
        for (SkinnedPart& part : character.parts) part.texture_path = texture_from_pak(folder, part.texture_path);
        bound_ = character_bound(character);
        model_.character = true;
        model_.name = character.name;
        model_.parts = character.parts.size();
        for (const SkinnedPart& part : character.parts) model_.triangles += part.indices.size() / 3;
        model_.skeleton_nodes = character.rig.skeleton.nodes.size();
        model_.clips = character.rig.clips.size();
        first_clip = character.rig.clips.empty() ? -1 : 0;
        scene.tile_id = character.name;
        CharacterInstance instance;
        instance.model_index = 0;
        instance.clip = first_clip;
        scene.character_models.push_back(std::move(character));
        scene.character_instances.push_back(instance);
    } else {
        NifModelRequest request;
        request.nif_path = nif_path;
        request.texture_dir = texture_dir;
        PropModel model;
        build_prop_model(nif, request, model);
        for (PropPart& part : model.parts) part.texture_path = texture_from_pak(folder, part.texture_path);
        for (ParticleSystemDefinition& system : model.particle_systems) {
            system.texture_path = texture_from_pak(folder, system.texture_path);
            for (std::string& path : system.flip_textures) path = texture_from_pak(folder, path);
        }
        bound_ = model.bound;
        if (bound_.radius <= 0.f && !model.particle_systems.empty()) bound_ = emitter_bound(model);
        model_.name = model.name;
        model_.parts = model.parts.size();
        model_.particle_systems = model.particle_systems.size();
        for (const PropPart& part : model.parts) model_.triangles += part.indices.size() / 3;
        scene.tile_id = model.name;
        scene.prop_models.push_back(std::move(model));
        PropInstance instance;
        instance.model_index = 0;
        instance.layer = SceneLayer::Props;
        scene.prop_instances.push_back(instance);
    }
    if (model_.parts == 0 && model_.particle_systems == 0) {
        error_ = "no geometry in this nif";
        return false;
    }
    if (bound_.radius <= 0.f) bound_.radius = 1.f;
    model_.radius = bound_.radius;
    for (int axis = 0; axis < 3; ++axis) {
        scene.bounds_min[axis] = bound_.center[axis] - bound_.radius;
        scene.bounds_max[axis] = bound_.center[axis] + bound_.radius;
    }
    if (rig_ok_) apply_light_rig(rig_, scene);
    renderer_->upload(scene);
    renderer_->set_far_plane(std::max(kClientFarPlane, bound_.radius * 8.f));
    renderer_->set_character_clip(-1);
    renderer_->animation().set_seconds(0.f);
    renderer_->animation().set_running(true);
    camera_.frame_bound(bound_);

    info_ = count_text("Parts", model_.parts) + "  " + count_text("Triangles", model_.triangles);
    if (model_.character)
        info_ += "  " + count_text("Bones", model_.skeleton_nodes) + "  " + count_text("Clips", model_.clips);
    else if (model_.particle_systems != 0)
        info_ += "  " + count_text("Particle systems", model_.particle_systems);
    char radius[48];
    std::snprintf(radius, sizeof(radius), "  Radius %.2f", model_.radius);
    info_ += radius;
    std::printf("[preview] %s %s\n", model_.name.c_str(), info_.c_str());
    return true;
}

bool Preview::load_audio(int index) {
    std::string error;
    if (!audio_->load(archive_->read(index), error)) {
        error_ = error;
        return false;
    }
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "Duration %.2f s  Rate %u Hz  Channels %u", audio_->duration(),
                  audio_->sample_rate(), audio_->channels());
    info_ = buffer;
    return true;
}

void Preview::load_text(int index) {
    const std::vector<uint8_t> bytes = archive_->read(index);
    const size_t shown = std::min(bytes.size(), kTextPreviewBytes);
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        // UTF 16 little endian with its mark turns into UTF 8 for the text box
        text_.clear();
        for (size_t at = 2; at + 1 < shown; at += 2)
            append_utf8(text_, static_cast<uint32_t>(bytes[at]) | (static_cast<uint32_t>(bytes[at + 1]) << 8));
    } else {
        text_.assign(reinterpret_cast<const char*>(bytes.data()), shown);
    }
    for (char& c : text_) {
        if (c == '\r') c = ' ';
        else if (c == '\0' || (static_cast<unsigned char>(c) < 32 && c != '\n' && c != '\t')) c = '.';
    }
    text_truncated_ = bytes.size() > shown;
    info_ = "Text " + std::to_string(shown) + " of " + std::to_string(bytes.size()) + " bytes shown";
}

void Preview::load_kfm_text(int index) {
    const std::string path = archive_->cache_path(index);
    KnC::KfmFile kfm;
    std::string error;
    if (path.empty() || !KnC::read_kfm(path, kfm, error)) {
        text_ = "cannot read the kfm " + error;
        return;
    }
    text_ = "model " + kfm.model_path + "\nroot " + kfm.model_root + "\n";
    for (const KnC::KfmSequence& sequence : kfm.sequences)
        text_ += std::to_string(sequence.sequence_id) + "  " + sequence.name + "  " + sequence.kf_path + "\n";
    info_ = "KFM " + std::to_string(kfm.sequences.size()) + " sequences";
}

}
