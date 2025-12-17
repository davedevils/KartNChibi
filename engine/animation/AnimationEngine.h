#pragma once
/**
 * KnC Animation Engine - COMPLETE ANIMATION SYSTEM
 * 
 * Supports:
 * - Transform animations (position, rotation, scale)
 * - Morph/Blend shape animations (vertex deformation)
 * - Alpha/Visibility animations
 * - Color animations
 * - UV/Texture coordinate animations
 * - Skeletal animations (bone hierarchy)
 * - 2D sprite/flipbook animations
 */

#include "raylib.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <algorithm>
#include <functional>

namespace KnC {
namespace Anim {

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================
struct AnimatedMesh;
struct AnimatedObject;
class AnimationEngine;

//=============================================================================
// INTERPOLATION
//=============================================================================

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

inline Vector2 LerpV2(Vector2 a, Vector2 b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

inline Vector3 LerpV3(Vector3 a, Vector3 b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t)};
}

inline Vector4 LerpV4(Vector4 a, Vector4 b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t)};
}

inline Quaternion SlerpQ(Quaternion a, Quaternion b, float t) {
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    Quaternion b2 = b;
    if (dot < 0) { dot = -dot; b2 = {-b.x, -b.y, -b.z, -b.w}; }
    
    if (dot > 0.9995f) {
        Quaternion r = {a.x + t*(b2.x-a.x), a.y + t*(b2.y-a.y), a.z + t*(b2.z-a.z), a.w + t*(b2.w-a.w)};
        float len = sqrtf(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
        return {r.x/len, r.y/len, r.z/len, r.w/len};
    }
    
    float theta = acosf(dot);
    float sinT = sinf(theta);
    float wa = sinf((1-t)*theta)/sinT;
    float wb = sinf(t*theta)/sinT;
    return {wa*a.x + wb*b2.x, wa*a.y + wb*b2.y, wa*a.z + wb*b2.z, wa*a.w + wb*b2.w};
}

//=============================================================================
// KEYFRAME TEMPLATES
//=============================================================================

template<typename T>
struct Key {
    float time;
    T value;
    T inTangent;
    T outTangent;
};

template<typename T>
struct KeyTrack {
    std::vector<Key<T>> keys;
    
    bool Empty() const { return keys.empty(); }
    
    T Evaluate(float time, T def, T (*lerp)(T, T, float)) const {
        if (keys.empty()) return def;
        if (keys.size() == 1) return keys[0].value;
        if (time <= keys[0].time) return keys[0].value;
        if (time >= keys.back().time) return keys.back().value;
        
        for (size_t i = 0; i < keys.size() - 1; i++) {
            if (time >= keys[i].time && time < keys[i+1].time) {
                float t = (time - keys[i].time) / (keys[i+1].time - keys[i].time);
                return lerp(keys[i].value, keys[i+1].value, t);
            }
        }
        return def;
    }
};

// Specialized evaluators
inline float EvalFloat(const KeyTrack<float>& track, float time, float def = 0.0f) {
    return track.Evaluate(time, def, Lerp);
}

inline Vector3 EvalVec3(const KeyTrack<Vector3>& track, float time, Vector3 def = {0,0,0}) {
    return track.Evaluate(time, def, LerpV3);
}

inline Quaternion EvalQuat(const KeyTrack<Quaternion>& track, float time, Quaternion def = {0,0,0,1}) {
    return track.Evaluate(time, def, SlerpQ);
}

//=============================================================================
// TRANSFORM ANIMATION
//=============================================================================

struct TransformAnim {
    std::string targetName;
    KeyTrack<Vector3> position;
    KeyTrack<Quaternion> rotation;
    KeyTrack<float> scale;  // Uniform scale
    KeyTrack<Vector3> scale3D;  // Non-uniform scale
    float duration = 0;
    
    bool HasKeys() const {
        return !position.Empty() || !rotation.Empty() || !scale.Empty() || !scale3D.Empty();
    }
    
    void Evaluate(float time, Vector3& outPos, Quaternion& outRot, Vector3& outScale) const {
        outPos = EvalVec3(position, time, {0, 0, 0});
        outRot = EvalQuat(rotation, time, {0, 0, 0, 1});
        
        if (!scale3D.Empty()) {
            outScale = EvalVec3(scale3D, time, {1, 1, 1});
        } else if (!scale.Empty()) {
            float s = EvalFloat(scale, time, 1.0f);
            outScale = {s, s, s};
        } else {
            outScale = {1, 1, 1};
        }
    }
};

//=============================================================================
// MORPH/BLEND SHAPE ANIMATION
//=============================================================================

struct MorphTarget {
    std::string name;
    std::vector<Vector3> deltaVertices;  // Offset from base mesh
    std::vector<Vector3> deltaNormals;
    KeyTrack<float> weight;  // Weight animation over time
};

struct MorphAnim {
    std::string targetMeshName;
    std::vector<MorphTarget> targets;
    float duration = 0;
    
    // Evaluate all weights at a given time
    void EvaluateWeights(float time, std::vector<float>& outWeights) const {
        outWeights.resize(targets.size());
        for (size_t i = 0; i < targets.size(); i++) {
            outWeights[i] = EvalFloat(targets[i].weight, time, 0.0f);
        }
    }
};

//=============================================================================
// ALPHA/VISIBILITY ANIMATION
//=============================================================================

struct AlphaAnim {
    std::string targetName;
    KeyTrack<float> alpha;  // 0.0 = invisible, 1.0 = opaque
    float duration = 0;
    
    float Evaluate(float time) const {
        return EvalFloat(alpha, time, 1.0f);
    }
};

struct VisibilityAnim {
    std::string targetName;
    std::vector<std::pair<float, bool>> keys;  // time, visible
    float duration = 0;
    
    bool Evaluate(float time) const {
        if (keys.empty()) return true;
        for (int i = (int)keys.size() - 1; i >= 0; i--) {
            if (time >= keys[i].first) return keys[i].second;
        }
        return keys[0].second;
    }
};

//=============================================================================
// COLOR ANIMATION
//=============================================================================

struct ColorAnim {
    std::string targetName;
    KeyTrack<Vector3> color;  // RGB
    KeyTrack<float> alpha;
    float duration = 0;
    
    Color Evaluate(float time) const {
        Vector3 rgb = EvalVec3(color, time, {1, 1, 1});
        float a = EvalFloat(alpha, time, 1.0f);
        return {
            (unsigned char)(rgb.x * 255),
            (unsigned char)(rgb.y * 255),
            (unsigned char)(rgb.z * 255),
            (unsigned char)(a * 255)
        };
    }
};

//=============================================================================
// UV ANIMATION
//=============================================================================

struct UVAnim {
    std::string targetName;
    KeyTrack<Vector2> offset;
    KeyTrack<float> rotation;
    KeyTrack<Vector2> scale;
    float duration = 0;
    
    void Evaluate(float time, Vector2& outOffset, float& outRotation, Vector2& outScale) const {
        outOffset = track.Empty() ? Vector2{0, 0} : EvalVec3(KeyTrack<Vector3>(), time, {0,0,0}); // Simplified
        outOffset = {0, 0};
        outRotation = EvalFloat(rotation, time, 0.0f);
        outScale = {1, 1};
        
        if (!offset.Empty()) {
            auto& keys = offset.keys;
            if (!keys.empty()) {
                // Manual evaluation for Vector2
                if (time <= keys[0].time) outOffset = keys[0].value;
                else if (time >= keys.back().time) outOffset = keys.back().value;
                else {
                    for (size_t i = 0; i < keys.size() - 1; i++) {
                        if (time >= keys[i].time && time < keys[i+1].time) {
                            float t = (time - keys[i].time) / (keys[i+1].time - keys[i].time);
                            outOffset = LerpV2(keys[i].value, keys[i+1].value, t);
                            break;
                        }
                    }
                }
            }
        }
        
        if (!scale.Empty()) {
            auto& keys = scale.keys;
            if (!keys.empty()) {
                if (time <= keys[0].time) outScale = keys[0].value;
                else if (time >= keys.back().time) outScale = keys.back().value;
                else {
                    for (size_t i = 0; i < keys.size() - 1; i++) {
                        if (time >= keys[i].time && time < keys[i+1].time) {
                            float t = (time - keys[i].time) / (keys[i+1].time - keys[i].time);
                            outScale = LerpV2(keys[i].value, keys[i+1].value, t);
                            break;
                        }
                    }
                }
            }
        }
    }
};

//=============================================================================
// SKELETAL ANIMATION (BONES)
//=============================================================================

struct Bone {
    std::string name;
    int parentIndex = -1;  // -1 = root bone
    Matrix bindPose;       // Local bind pose transform
    Matrix inverseBindPose;  // For skinning
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneMap;  // name -> index
    
    int FindBone(const std::string& name) const {
        auto it = boneMap.find(name);
        return it != boneMap.end() ? it->second : -1;
    }
};

struct BoneAnim {
    int boneIndex;
    KeyTrack<Vector3> position;
    KeyTrack<Quaternion> rotation;
    KeyTrack<Vector3> scale;
};

struct SkeletalAnim {
    std::string name;
    std::vector<BoneAnim> boneAnims;
    float duration = 0;
    
    // Evaluate all bone transforms
    void Evaluate(float time, const Skeleton& skeleton, std::vector<Matrix>& outBoneMatrices) const {
        outBoneMatrices.resize(skeleton.bones.size());
        
        // Initialize with bind pose
        for (size_t i = 0; i < skeleton.bones.size(); i++) {
            outBoneMatrices[i] = skeleton.bones[i].bindPose;
        }
        
        // Apply animations
        for (const auto& ba : boneAnims) {
            if (ba.boneIndex < 0 || ba.boneIndex >= (int)outBoneMatrices.size()) continue;
            
            Vector3 pos = EvalVec3(ba.position, time, {0, 0, 0});
            Quaternion rot = EvalQuat(ba.rotation, time, {0, 0, 0, 1});
            Vector3 scl = EvalVec3(ba.scale, time, {1, 1, 1});
            
            // Build local transform matrix
            Matrix mScale = MatrixScale(scl.x, scl.y, scl.z);
            Matrix mRot = QuaternionToMatrix(rot);
            Matrix mTrans = MatrixTranslate(pos.x, pos.y, pos.z);
            outBoneMatrices[ba.boneIndex] = MatrixMultiply(MatrixMultiply(mScale, mRot), mTrans);
        }
        
        // Build world matrices (parent-child hierarchy)
        for (size_t i = 0; i < skeleton.bones.size(); i++) {
            int parent = skeleton.bones[i].parentIndex;
            if (parent >= 0 && parent < (int)i) {
                outBoneMatrices[i] = MatrixMultiply(outBoneMatrices[i], outBoneMatrices[parent]);
            }
        }
        
        // Apply inverse bind pose for skinning
        for (size_t i = 0; i < skeleton.bones.size(); i++) {
            outBoneMatrices[i] = MatrixMultiply(skeleton.bones[i].inverseBindPose, outBoneMatrices[i]);
        }
    }
};

//=============================================================================
// 2D SPRITE/FLIPBOOK ANIMATION
//=============================================================================

struct SpriteFrame {
    Rectangle source;  // Source rect in texture atlas
    Vector2 offset;    // Pivot offset
    float duration;    // Frame duration in seconds
};

struct SpriteAnim {
    std::string name;
    Texture2D texture;
    std::vector<SpriteFrame> frames;
    bool loop = true;
    float totalDuration = 0;
    
    void CalculateDuration() {
        totalDuration = 0;
        for (const auto& f : frames) totalDuration += f.duration;
    }
    
    const SpriteFrame& GetFrame(float time) const {
        if (frames.empty()) {
            static SpriteFrame empty = {{0, 0, 0, 0}, {0, 0}, 0};
            return empty;
        }
        
        if (loop && totalDuration > 0) {
            time = fmodf(time, totalDuration);
        }
        
        float t = 0;
        for (const auto& f : frames) {
            t += f.duration;
            if (time < t) return f;
        }
        return frames.back();
    }
};

//=============================================================================
// ANIMATED MESH (with morph support)
//=============================================================================

struct AnimatedMesh {
    // Base geometry
    std::vector<float> baseVertices;
    std::vector<float> baseNormals;
    std::vector<float> texcoords;
    std::vector<unsigned short> indices;
    
    // Current animated state
    std::vector<float> currentVertices;
    std::vector<float> currentNormals;
    
    // Morph targets
    std::vector<MorphTarget> morphTargets;
    
    // GPU mesh
    Mesh gpuMesh = {0};
    bool dirty = false;
    
    void Initialize() {
        currentVertices = baseVertices;
        currentNormals = baseNormals;
        
        // Create GPU mesh
        if (gpuMesh.vertexCount > 0) {
            UnloadMesh(gpuMesh);
        }
        
        memset(&gpuMesh, 0, sizeof(Mesh));
        gpuMesh.vertexCount = (int)(baseVertices.size() / 3);
        gpuMesh.triangleCount = (int)(indices.size() / 3);
        
        gpuMesh.vertices = (float*)RL_MALLOC(baseVertices.size() * sizeof(float));
        memcpy(gpuMesh.vertices, baseVertices.data(), baseVertices.size() * sizeof(float));
        
        if (!baseNormals.empty()) {
            gpuMesh.normals = (float*)RL_MALLOC(baseNormals.size() * sizeof(float));
            memcpy(gpuMesh.normals, baseNormals.data(), baseNormals.size() * sizeof(float));
        }
        
        if (!texcoords.empty()) {
            gpuMesh.texcoords = (float*)RL_MALLOC(texcoords.size() * sizeof(float));
            memcpy(gpuMesh.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
        }
        
        if (!indices.empty()) {
            gpuMesh.indices = (unsigned short*)RL_MALLOC(indices.size() * sizeof(unsigned short));
            memcpy(gpuMesh.indices, indices.data(), indices.size() * sizeof(unsigned short));
        }
        
        UploadMesh(&gpuMesh, true);  // true = dynamic (for morphing)
    }
    
    void ApplyMorphWeights(const std::vector<float>& weights) {
        if (morphTargets.empty() || weights.empty()) return;
        
        // Reset to base
        currentVertices = baseVertices;
        if (!baseNormals.empty()) {
            currentNormals = baseNormals;
        }
        
        // Apply each morph target
        for (size_t t = 0; t < morphTargets.size() && t < weights.size(); t++) {
            float w = weights[t];
            if (fabsf(w) < 0.001f) continue;
            
            const auto& target = morphTargets[t];
            
            // Apply vertex deltas
            size_t vertCount = std::min(target.deltaVertices.size(), baseVertices.size() / 3);
            for (size_t v = 0; v < vertCount; v++) {
                currentVertices[v * 3 + 0] += target.deltaVertices[v].x * w;
                currentVertices[v * 3 + 1] += target.deltaVertices[v].y * w;
                currentVertices[v * 3 + 2] += target.deltaVertices[v].z * w;
            }
            
            // Apply normal deltas
            if (!target.deltaNormals.empty() && !currentNormals.empty()) {
                size_t normCount = std::min(target.deltaNormals.size(), baseNormals.size() / 3);
                for (size_t n = 0; n < normCount; n++) {
                    currentNormals[n * 3 + 0] += target.deltaNormals[n].x * w;
                    currentNormals[n * 3 + 1] += target.deltaNormals[n].y * w;
                    currentNormals[n * 3 + 2] += target.deltaNormals[n].z * w;
                }
            }
        }
        
        dirty = true;
    }
    
    void UpdateGPU() {
        if (!dirty || gpuMesh.vertexCount == 0) return;
        
        // Update vertex buffer on GPU
        if (gpuMesh.vertices && !currentVertices.empty()) {
            memcpy(gpuMesh.vertices, currentVertices.data(), currentVertices.size() * sizeof(float));
            UpdateMeshBuffer(gpuMesh, 0, gpuMesh.vertices, gpuMesh.vertexCount * 3 * sizeof(float), 0);
        }
        
        // Update normal buffer on GPU
        if (gpuMesh.normals && !currentNormals.empty()) {
            memcpy(gpuMesh.normals, currentNormals.data(), currentNormals.size() * sizeof(float));
            UpdateMeshBuffer(gpuMesh, 2, gpuMesh.normals, gpuMesh.vertexCount * 3 * sizeof(float), 0);
        }
        
        dirty = false;
    }
    
    void Cleanup() {
        if (gpuMesh.vertexCount > 0) {
            UnloadMesh(gpuMesh);
            memset(&gpuMesh, 0, sizeof(Mesh));
        }
    }
};

//=============================================================================
// ANIMATED OBJECT (combines all animation types)
//=============================================================================

struct AnimatedObject {
    std::string name;
    
    // Visual components
    Model model = {0};
    AnimatedMesh* animMesh = nullptr;  // If using morph animation
    Texture2D texture = {0};
    
    // Transform
    Vector3 basePosition = {0, 0, 0};
    Quaternion baseRotation = {0, 0, 0, 1};
    Vector3 baseScale = {1, 1, 1};
    
    // Animated state (updated each frame)
    Vector3 animPosition = {0, 0, 0};
    Quaternion animRotation = {0, 0, 0, 1};
    Vector3 animScale = {1, 1, 1};
    float animAlpha = 1.0f;
    Color animColor = WHITE;
    bool animVisible = true;
    
    // Animation data
    TransformAnim transformAnim;
    MorphAnim morphAnim;
    AlphaAnim alphaAnim;
    ColorAnim colorAnim;
    
    // Morph weights
    std::vector<float> morphWeights;
    
    // Playback state
    float animTime = 0;
    float animSpeed = 1.0f;
    float animDuration = 0;
    bool playing = true;
    bool looping = true;
    
    void Update(float deltaTime) {
        if (!playing) return;
        
        animTime += deltaTime * animSpeed;
        
        if (looping && animDuration > 0) {
            animTime = fmodf(animTime, animDuration);
        } else if (animTime > animDuration) {
            animTime = animDuration;
            playing = false;
        }
        
        // Evaluate transform
        if (transformAnim.HasKeys()) {
            transformAnim.Evaluate(animTime, animPosition, animRotation, animScale);
        }
        
        // Evaluate alpha
        if (!alphaAnim.alpha.Empty()) {
            animAlpha = alphaAnim.Evaluate(animTime);
        }
        
        // Evaluate color
        if (!colorAnim.color.Empty()) {
            animColor = colorAnim.Evaluate(animTime);
        }
        
        // Evaluate morph
        if (!morphAnim.targets.empty()) {
            morphAnim.EvaluateWeights(animTime, morphWeights);
            if (animMesh) {
                animMesh->ApplyMorphWeights(morphWeights);
                animMesh->UpdateGPU();
            }
        }
    }
    
    Matrix GetWorldMatrix() const {
        // Combine base + animated transforms
        Vector3 finalPos = {
            basePosition.x + animPosition.x,
            basePosition.y + animPosition.y,
            basePosition.z + animPosition.z
        };
        
        Quaternion finalRot = QuaternionMultiply(baseRotation, animRotation);
        
        Vector3 finalScale = {
            baseScale.x * animScale.x,
            baseScale.y * animScale.y,
            baseScale.z * animScale.z
        };
        
        Matrix mScale = MatrixScale(finalScale.x, finalScale.y, finalScale.z);
        Matrix mRot = QuaternionToMatrix(finalRot);
        Matrix mTrans = MatrixTranslate(finalPos.x, finalPos.y, finalPos.z);
        
        return MatrixMultiply(MatrixMultiply(mScale, mRot), mTrans);
    }
    
    void Draw() {
        if (!animVisible || animAlpha < 0.01f) return;
        
        Matrix world = GetWorldMatrix();
        
        // Apply alpha to color
        Color tint = animColor;
        tint.a = (unsigned char)(animAlpha * 255);
        
        if (animMesh && animMesh->gpuMesh.vertexCount > 0) {
            // Draw animated mesh
            Model tempModel = LoadModelFromMesh(animMesh->gpuMesh);
            tempModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
            DrawModel(tempModel, {0, 0, 0}, 1.0f, tint);
            // Note: Don't unload tempModel as it shares the mesh
        } else if (model.meshCount > 0) {
            // Draw static model with animation transform
            DrawModel(model, {0, 0, 0}, 1.0f, tint);
        }
    }
    
    void Reset() {
        animTime = 0;
        playing = true;
        animPosition = {0, 0, 0};
        animRotation = {0, 0, 0, 1};
        animScale = {1, 1, 1};
        animAlpha = 1.0f;
        animColor = WHITE;
        animVisible = true;
    }
};

//=============================================================================
// ANIMATION ENGINE (Global manager)
//=============================================================================

class AnimationEngine {
public:
    static AnimationEngine& Instance() {
        static AnimationEngine instance;
        return instance;
    }
    
    // Update all registered animated objects
    void Update(float deltaTime) {
        m_globalTime += deltaTime;
        
        for (auto& obj : m_objects) {
            if (obj) obj->Update(deltaTime);
        }
        
        // Update sprite animations
        for (auto& sprite : m_sprites) {
            sprite.time += deltaTime;
        }
    }
    
    // Register an animated object
    void Register(AnimatedObject* obj) {
        if (obj) m_objects.push_back(obj);
    }
    
    void Unregister(AnimatedObject* obj) {
        m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());
    }
    
    // Sprite animation helpers
    struct SpriteInstance {
        const SpriteAnim* anim = nullptr;
        float time = 0;
        bool playing = true;
    };
    
    int CreateSprite(const SpriteAnim* anim) {
        SpriteInstance inst;
        inst.anim = anim;
        m_sprites.push_back(inst);
        return (int)(m_sprites.size() - 1);
    }
    
    void DrawSprite(int id, Vector2 position, float scale = 1.0f, Color tint = WHITE) {
        if (id < 0 || id >= (int)m_sprites.size()) return;
        auto& sprite = m_sprites[id];
        if (!sprite.anim || sprite.anim->frames.empty()) return;
        
        const SpriteFrame& frame = sprite.anim->GetFrame(sprite.time);
        Rectangle dest = {
            position.x - frame.offset.x * scale,
            position.y - frame.offset.y * scale,
            frame.source.width * scale,
            frame.source.height * scale
        };
        
        DrawTexturePro(sprite.anim->texture, frame.source, dest, {0, 0}, 0, tint);
    }
    
    // Global time
    float GetGlobalTime() const { return m_globalTime; }
    void ResetGlobalTime() { m_globalTime = 0; }
    
    // Pause/Resume
    void Pause() { m_paused = true; }
    void Resume() { m_paused = false; }
    bool IsPaused() const { return m_paused; }
    
private:
    AnimationEngine() = default;
    
    std::vector<AnimatedObject*> m_objects;
    std::vector<SpriteInstance> m_sprites;
    float m_globalTime = 0;
    bool m_paused = false;
};

} // namespace Anim
} // namespace KnC

