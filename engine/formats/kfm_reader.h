#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// NiKFMTool TransitionType stable across Gamebryo 2 6 3 2 and 4
enum class KfmTransitionType : uint32_t {
    Blend          = 0,
    Morph          = 1,
    Crossfade      = 2,
    Chain          = 3,
    DefaultSync    = 4,
    DefaultNonSync = 5,
    DefaultInvalid = 6,
};

// Text key window a blend transition crossfades over leave source clip at start key
struct KfmBlendPair {
    std::string start_key;
    std::string target_key;
};

// One hop of a Chain transition play sequence id for duration seconds before continuing
struct KfmChainStep {
    uint32_t sequence_id = 0;
    float    duration    = 0.f;
};

// How this sequence may hand over to target sequence id two Default types carry no payload
struct KfmTransition {
    uint32_t          target_sequence_id = 0;
    KfmTransitionType type               = KfmTransitionType::DefaultInvalid;
    float             duration           = 0.f;
    std::vector<KfmBlendPair> blend_pairs;
    std::vector<KfmChainStep> chain;
};

// One animation clip the manager binds kf path is stored exactly as the file holds it
struct KfmSequence {
    uint32_t    sequence_id     = 0;
    std::string kf_path;
    // Clip name present below 1 2 5 0 and from 2 5 0 0 empty between
    std::string name;
    // Index of NiControllerSequence inside kf path below 2 5 0 0 this is how clip addressed
    int32_t animation_index = -1;
    std::vector<KfmTransition> transitions;
};

// A sequence playing as part of a group with its blend weight and ramps
struct KfmSequenceGroupMember {
    uint32_t sequence_id = 0;
    int32_t  priority    = 0;
    float    weight      = 0.f;
    float    ease_in_seconds  = 0.f;
    float    ease_out_seconds = 0.f;
    // NiKFMTool SYNC SEQUENCE ID NONE 0xFFFFFFFE when unsynchronised
    uint32_t synchronize_sequence_id = 0xFFFFFFFEu;
    // Additive blending from Gamebryo 3 1 0 3 false below it
    bool additive = false;
};

// Named set of sequences meant to play together
struct KfmSequenceGroup {
    uint32_t    group_id = 0;
    std::string name;
    std::vector<KfmSequenceGroupMember> members;
};

// A decoded kfm keyframe manager the root skeleton it drives every clip and transitions
struct KfmFile {
    // One byte per component most significant first 2 1 0 0 to 0x02010000
    uint32_t    version = 0;
    std::string version_text;

    std::string model_path;
    std::string model_root;

    // Transition settings the two Default transition types resolve to below 1 2 2 0 file stores none
    KfmTransitionType default_sync_type     = KfmTransitionType::Blend;
    KfmTransitionType default_non_sync_type = KfmTransitionType::Blend;
    float default_sync_duration     = 0.f;
    float default_non_sync_duration = 0.f;

    std::vector<KfmSequence>      sequences;
    std::vector<KfmSequenceGroup> sequence_groups;
};

// Reads binary little endian kfm ASCII version comment line then binary field set gated on version
bool read_kfm(const std::string& path, KfmFile& out, std::string& error);

// The tail an asset root joins padding and turns separators around
std::string kfm_relative_path(const std::string& stored);

} // namespace KnC
