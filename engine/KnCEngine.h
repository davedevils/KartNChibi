/**
 * KnC Engine - Main Include
 * 
 * Single include file for the complete KnC rendering engine.
 * Include this file to get access to all engine features.
 * 
 * Usage:
 *   #include "KnCEngine.h"
 *   
 *   // Initialize
 *   KnC::Engine::Initialize(pakReader);
 *   
 *   // Load a map
 *   KnC::Engine::LoadMap(mapInfo);
 *   
 *   // In game loop:
 *   KnC::Engine::Update(deltaTime);
 *   KnC::Engine::Render(camera);
 */
#pragma once

// Core systems (MUST be first)
#include "core/Logger.h"
#include "core/Error.h"
#include "../common/PakReader.h"
#include "bridge/NifBridge.h"
#include "shaders/KnCShaders.h"

// Render systems
#include "render/SceneManager.h"
#include "render/MeshRenderer.h"
#include "render/UIRenderer.h"

namespace KnC {

//=============================================================================
// ENGINE - Main engine API
//=============================================================================

class Engine {
public:
    // Initialize the engine with a PAK reader
    static bool Initialize(PakReader* pakReader) {
        KNC_TRY {
            LOG_INFO("Engine", "Initializing KnC Engine...");
            
            KNC_CHECK_NULL(pakReader, "PakReader is null");
            s_pakReader = pakReader;
            
            LOG_DEBUG("Engine", "Initializing SceneManager...");
            SceneManager::Instance().Initialize(pakReader);
            
            s_initialized = true;
            LOG_INFO("Engine", "KnC Engine initialized successfully");
            return true;
        }
        KNC_CATCH_RETURN("Engine", "Initialize", false);
    }
    
    // Shutdown the engine
    static void Shutdown() {
        KNC_TRY {
            LOG_INFO("Engine", "Shutting down...");
            
            SceneManager::Instance().Shutdown();
            
            s_initialized = false;
            LOG_INFO("Engine", "Shutdown complete");
        }
        KNC_CATCH("Engine", "Shutdown");
    }
    
    // Load a map
    static bool LoadMap(const MapInfo& mapInfo) {
        KNC_TRY {
            LOG_INFO("Engine", "Loading map: %s/%s", mapInfo.category.c_str(), mapInfo.name.c_str());
            bool result = SceneManager::Instance().LoadScene(mapInfo);
            if (result) {
                LOG_INFO("Engine", "Map loaded successfully");
            } else {
                LOG_WARN("Engine", "Map load returned false");
            }
            return result;
        }
        KNC_CATCH_RETURN("Engine", "LoadMap", false);
    }
    
    // Unload current map
    static void UnloadMap() {
        KNC_TRY {
            SceneManager::Instance().UnloadCurrentScene();
        }
        KNC_CATCH("Engine", "UnloadMap");
    }
    
    // Update (call every frame)
    static void Update(float deltaTime) {
        KNC_TRY {
            // Update scene animation time
            Scene& scene = SceneManager::Instance().GetCurrentScene();
            scene.UpdateAnimation(deltaTime);
            
            // Debug: log animTime every 60 frames
            static int frameCount = 0;
            if (++frameCount % 60 == 0) {
                LOG_DEBUG("Engine", "animTime=%.2f", scene.animTime);
            }
        }
        KNC_CATCH("Engine", "Update");
    }
    
    // Render the scene (should not throw - critical path)
    static void Render(const Camera3D& camera) {
        KNC_TRY {
            MeshRenderer::Instance().RenderScene(
                SceneManager::Instance().GetCurrentScene(), 
                camera
            );
        }
        KNC_CATCH("Engine", "Render");
    }
    
    // Check if engine is initialized
    static bool IsInitialized() { return s_initialized; }
    
    // Access subsystems
    static SceneManager& GetSceneManager() { return SceneManager::Instance(); }
    static MeshRenderer& GetMeshRenderer() { return MeshRenderer::Instance(); }
    static UIRenderer& GetUIRenderer() { return UIRenderer::Instance(); }
    
    // Get current scene
    static Scene& GetCurrentScene() { return SceneManager::Instance().GetCurrentScene(); }
    
    // Render settings
    static RenderSettings& GetRenderSettings() { return MeshRenderer::Instance().GetSettings(); }
    
    // Logger access
    static Logger& GetLogger() { return Logger::Instance(); }
    
private:
    static inline PakReader* s_pakReader = nullptr;
    static inline bool s_initialized = false;
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

// Convert NIF coordinates to Real coordinates
inline Vector3 NifToRaylib(const Vector3& nif) {
    return {nif.x, nif.z, -nif.y};
}

// Convert Real coordinates to NIF coordinates
inline Vector3 RaylibToNif(const Vector3& rl) {
    return {rl.x, -rl.z, rl.y};
}

} // namespace KnC

